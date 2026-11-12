// Файл: engine.test.js
// Назначение: тесты связки JavaScript и ядра C++: загружают собранный
// miniapp/wasm/xpay.wasm и проверяют ответы на команды, включая кириллицу,
// эмодзи и сохранение через хранилище по частям.

import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

import { createEngine } from '../../miniapp/js/core/engine.js';
import { CHUNK_SIZE, META_KEY, loadText, saveText } from '../../miniapp/js/core/storage.js';
import { memoryStore } from './helpers.js';

const wasmBytes = await readFile(new URL('../../miniapp/wasm/xpay.wasm', import.meta.url));

// Каждый тест получает своё ядро с состоянием первого запуска.
const newEngine = () => createEngine(wasmBytes);

const FOOD = { amount: '1 500,50', at: '2026-11-12T09:30', kind: 'expense', category: 'Еда', note: 'обед 🍲' };

test('первый запуск: предопределённые категории, операций нет', async () => {
  const core = await newEngine();
  assert.deepEqual(core.call('state'), {
    ok: true,
    categories: {
      income: ['Зарплата', 'Подработка', 'Подарки'],
      expense: ['Еда', 'Транспорт', 'Жильё', 'Развлечения', 'Здоровье'],
    },
    count: 0,
  });
  assert.deepEqual(core.call('list'), { ok: true, operations: [], totals: { income: 0, expense: 0, balance: 0 } });
  assert.equal(core.call('report').forecast, null);
});

test('добавление, правка и удаление операции', async () => {
  const core = await newEngine();
  assert.deepEqual(core.call('add', FOOD), {
    ok: true,
    operation: { id: 1, at: '2026-11-12T09:30', amount: 150050, kind: 'expense', category: 'Еда', note: 'обед 🍲' },
    limit: null,
  });
  const updated = core.call('update', { ...FOOD, id: 1, amount: '200', note: '' });
  assert.equal(updated.operation.amount, 20000);
  assert.equal(updated.operation.note, '');
  assert.equal(core.call('remove', { id: 1 }).ok, true);
  assert.equal(core.call('remove', { id: 1 }).ok, false);
  assert.equal(core.call('state').count, 0);
});

test('ошибки ввода возвращаются текстом для пользователя', async () => {
  const core = await newEngine();
  assert.deepEqual(core.call('add', { ...FOOD, amount: 'abc' }),
    { ok: false, error: 'Сумма пишется цифрами, после запятой не больше двух знаков.' });
  assert.equal(core.call('add', { ...FOOD, amount: '0' }).error, 'Сумма должна быть больше нуля.');
  assert.equal(core.call('add', { ...FOOD, category: 'Космос' }).error, 'Выберите категорию из списка.');
  assert.equal(core.call('add', { ...FOOD, at: '2026-02-30T10:00' }).ok, false);
  assert.equal(core.call('взлететь').ok, false);
  assert.equal(core.call('state').count, 0);
});

test('отчёт совпадает с образцом из задания', async () => {
  const core = await newEngine();
  const rows = [
    ['60000', '2026-10-01T10:00', 'income', 'Зарплата'],
    ['3000', '2026-10-03T12:30', 'expense', 'Еда'],
    ['2000', '2026-10-10T19:00', 'expense', 'Еда'],
    ['2500', '2026-10-12T08:15', 'expense', 'Транспорт'],
    ['5000', '2026-10-20T21:00', 'expense', 'Развлечения'],
  ];
  for (const [amount, at, kind, category] of rows) {
    assert.equal(core.call('add', { amount, at, kind, category, note: '' }).ok, true);
  }
  const report = core.call('report', { from: '2026-10-01', to: '2026-10-31' });
  assert.deepEqual(report.totals, { income: 6000000, expense: 1250000, balance: 4750000 });
  assert.deepEqual(report.shares[0], { category: 'Еда', amount: 500000, percent: 40 });
  assert.equal(report.forecast.potential, 4750000);

  const text = core.call('export', { format: 'txt' }).text;
  assert.ok(text.includes('Еда: [#######] 40% (5000 руб.)\n'));
  assert.ok(text.includes('Транспорт: [####] 20% (2500 руб.)\n'));
  assert.ok(core.call('export', { format: 'csv' }).text.startsWith('Дата;Время;Тип;Категория;Сумма;Описание\r\n'));

  assert.deepEqual(core.call('list', { category: 'Еда', from: '2026-10-05' }).operations.map((item) => item.id), [3]);
  assert.equal(core.call('list', { from: '2026-10-20', to: '2026-10-01' }).ok, false);
});

test('лимит: предупреждение приходит в ответе на добавление', async () => {
  const core = await newEngine();
  assert.equal(core.call('setLimit', { category: 'Еда', amount: '2000' }).ok, true);
  assert.deepEqual(core.call('add', FOOD).limit, { limit: 200000, spent: 150050, left: 49950, exceeded: false });
  assert.deepEqual(core.call('add', { ...FOOD, amount: '500' }).limit,
    { limit: 200000, spent: 200050, left: -50, exceeded: true });
  const food = core.call('limits', { month: '2026-11' }).items.find((item) => item.category === 'Еда');
  assert.equal(food.status.exceeded, true);
  assert.equal(core.call('clearLimit', { category: 'Еда' }).ok, true);
  assert.equal(core.call('limits', { month: '2026-11' }).items[0].status, null);
});

test('состояние сохраняется по частям и читается другим ядром', async () => {
  const core = await newEngine();
  core.call('addCategory', { kind: 'expense', name: 'Кофе' });
  core.call('setLimit', { category: 'Кофе', amount: '3000' });
  for (let day = 1; day <= 28; day += 1) {
    for (let hour = 8; hour < 14; hour += 1) {
      const at = `2026-10-${String(day).padStart(2, '0')}T${String(hour).padStart(2, '0')}:00`;
      const note = 'латте ☕ "двойной"';
      core.call('add', { amount: String(day + hour), at, kind: 'expense', category: 'Кофе', note });
    }
  }
  const { data } = core.call('dump');
  assert.ok(data.length > CHUNK_SIZE * 3, 'состояние должно занять несколько частей');

  const store = memoryStore();
  await saveText(store, data);
  assert.ok(Number(store.items.get(META_KEY)) > 3);

  const other = await newEngine();
  assert.equal(other.call('load', { data: await loadText(store) }).ok, true);
  assert.equal(other.call('dump').data, data);
  assert.equal(other.call('state').count, 168);

  // Испорченные данные ядро отклоняет и состояние не меняет.
  assert.equal(other.call('load', { data: data.slice(0, -20) }).ok, false);
  assert.equal(other.call('load', { data: 'не JSON' }).ok, false);
  assert.equal(other.call('dump').data, data);
});

test('ядро выдерживает много вызовов подряд и длинные строки', async () => {
  const core = await newEngine();
  const note = 'я'.repeat(200);
  for (let index = 0; index < 3000; index += 1) {
    const result = core.call('add', { ...FOOD, note });
    assert.equal(result.operation.id, index + 1);
  }
  assert.equal(core.call('list').operations.length, 3000);
  assert.equal(core.call('add', { ...FOOD, note: `${note}я` }).ok, false);
});
