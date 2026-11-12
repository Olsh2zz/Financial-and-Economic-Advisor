// Файл: state.test.js
// Назначение: тесты сохранения: перевод состояния в JSON и обратно,
// отказ от испорченных данных, запись по частям в хранилище.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import { setLimit } from '../miniapp/js/core/budget.js';
import {
  createDefaultState, deserialize, isValidStamp, serialize, toStamp,
} from '../miniapp/js/core/state.js';
import {
  CHUNK_PREFIX, CHUNK_SIZE, META_KEY, loadText, saveText, splitText,
} from '../miniapp/js/core/storage.js';
import { SAMPLE_ROWS, memoryStore, stateWith } from './helpers.js';

test('проверка даты и времени', () => {
  assert.equal(isValidStamp('2026-11-12T09:30'), true);
  assert.equal(isValidStamp('2024-02-29T00:00'), true);
  assert.equal(isValidStamp('2026-02-29T00:00'), false);
  assert.equal(isValidStamp('2026-13-01T00:00'), false);
  assert.equal(isValidStamp('2026-11-12T09:60'), false);
  assert.equal(isValidStamp('2026-11-12 09:30'), false);
  assert.equal(isValidStamp(''), false);
  assert.equal(toStamp(new Date(2026, 0, 5, 7, 3)), '2026-01-05T07:03');
});

test('состояние переживает сохранение и загрузку', () => {
  const state = setLimit(stateWith(SAMPLE_ROWS), 'Развлечения', 1000000).state;
  const restored = deserialize(serialize(state));
  assert.equal(restored.ok, true);
  assert.deepEqual(restored.state, state);
  assert.deepEqual(deserialize(serialize(createDefaultState())).state, createDefaultState());
});

test('испорченные данные отклоняются', () => {
  const good = stateWith(SAMPLE_ROWS);
  const broken = [
    'не JSON',
    'null',
    '[]',
    serialize({ ...good, version: 99 }),
    serialize({ ...good, categories: { income: ['Зарплата'] } }),
    serialize({ ...good, categories: { income: ['А', 'А'], expense: ['Еда'] } }),
    serialize({ ...good, limits: { Космос: 100 } }),
    serialize({ ...good, limits: { Еда: -5 } }),
    serialize({ ...good, operations: [{ ...good.operations[0], amount: '100' }] }),
    serialize({ ...good, operations: [{ ...good.operations[0], category: 'Космос' }] }),
    serialize({ ...good, operations: [good.operations[0], good.operations[0]] }),
    serialize({ ...good, nextId: 1 }),
  ];
  for (const text of broken) {
    const result = deserialize(text);
    assert.equal(result.ok, false, text.slice(0, 60));
    assert.ok(result.error.length > 0);
  }
});

test('текст режется на части без разрыва суррогатной пары', () => {
  assert.deepEqual(splitText('abcdefg', 3), ['abc', 'def', 'g']);
  assert.deepEqual(splitText('', 3), []);
  // Эмодзи занимает две кодовые единицы и целиком уходит в следующую часть.
  assert.deepEqual(splitText('ab😀cd', 3), ['ab', '😀c', 'd']);
});

test('длинное состояние сохраняется по частям и читается обратно', async () => {
  const rows = [];
  for (let day = 1; day <= 28; day += 1) {
    for (let hour = 8; hour < 20; hour += 1) {
      const stamp = `2026-10-${String(day).padStart(2, '0')}T${String(hour).padStart(2, '0')}:00`;
      rows.push([stamp, 'expense', 'Еда', day + hour, 'обед 🍲 и кофе']);
    }
  }
  const text = serialize(stateWith(rows));
  const store = memoryStore();
  await saveText(store, text);

  const count = Number(store.items.get(META_KEY));
  assert.ok(count > 1, 'состояние должно занять несколько частей');
  for (let index = 0; index < count; index += 1) {
    assert.ok(store.items.get(`${CHUNK_PREFIX}${index}`).length <= CHUNK_SIZE);
  }
  assert.equal(await loadText(store), text);
  assert.equal(deserialize(await loadText(store)).ok, true);
});

test('после короткой записи лишние части удаляются', async () => {
  const store = memoryStore();
  await saveText(store, 'я'.repeat(CHUNK_SIZE * 3));
  assert.equal(store.items.size, 4);
  await saveText(store, 'коротко');
  assert.equal(store.items.size, 2);
  assert.equal(await loadText(store), 'коротко');
});

test('пустое и повреждённое хранилище', async () => {
  assert.equal(await loadText(memoryStore()), null);

  const lost = memoryStore();
  await saveText(lost, 'я'.repeat(CHUNK_SIZE * 2));
  lost.items.delete(`${CHUNK_PREFIX}1`);
  await assert.rejects(loadText(lost), /не хватает части 2 из 2/);

  const garbled = memoryStore();
  garbled.items.set(META_KEY, 'много');
  await assert.rejects(loadText(garbled), /повреждена запись/);
});
