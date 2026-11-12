// Файл: report.test.js
// Назначение: тесты статистики и экспорта, включая строки из образца в задании.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import { filterOperations } from '../miniapp/js/core/ledger.js';
import { barLine, buildCsv, buildTextReport, expenseShares, totals } from '../miniapp/js/core/report.js';
import { SAMPLE_ROWS, stateWith } from './helpers.js';

const { operations } = stateWith(SAMPLE_ROWS);

test('итоги: доходы, расходы, баланс', () => {
  assert.deepEqual(totals(operations), { income: 6000000, expense: 1250000, balance: 4750000 });
  assert.deepEqual(totals([]), { income: 0, expense: 0, balance: 0 });
});

test('итоги за период', () => {
  const part = filterOperations(operations, { from: '2026-10-02', to: '2026-10-12' });
  assert.deepEqual(totals(part), { income: 0, expense: 750000, balance: -750000 });
});

test('доли расходов по категориям', () => {
  assert.deepEqual(expenseShares(operations), [
    { category: 'Еда', amount: 500000, percent: 40 },
    { category: 'Развлечения', amount: 500000, percent: 40 },
    { category: 'Транспорт', amount: 250000, percent: 20 },
  ]);
  assert.deepEqual(expenseShares([]), []);
  // Только доходы: делить не на что, список пуст.
  assert.deepEqual(expenseShares(operations.filter((item) => item.kind === 'income')), []);
});

test('строки отчёта совпадают с образцом из задания', () => {
  const lines = expenseShares(operations).map(barLine);
  assert.equal(lines[0], 'Еда: [#######] 40% (5000 руб.)');
  // В образце после «Транспорт:» нет пробела, а после «Еда:» есть.
  // Здесь пробел ставится всегда, остальное совпадает посимвольно.
  assert.equal(lines[2], 'Транспорт: [####] 20% (2500 руб.)');
});

test('шкала на краях', () => {
  assert.equal(barLine({ category: 'Еда', amount: 100, percent: 0 }), 'Еда: [] 0% (1 руб.)');
  assert.equal(barLine({ category: 'Еда', amount: 100, percent: 100 }),
    `Еда: [${'#'.repeat(18)}] 100% (1 руб.)`);
});

test('текстовый отчёт', () => {
  const period = { from: '2026-10-01', to: '2026-10-31' };
  assert.equal(buildTextReport(filterOperations(operations, period), period), [
    'Финансовый отчёт с 2026-10-01 по 2026-10-31',
    '',
    'Доходы:  60000 руб.',
    'Расходы: 12500 руб.',
    'Баланс:  47500 руб.',
    '',
    'Расходы по категориям:',
    'Еда: [#######] 40% (5000 руб.)',
    'Развлечения: [#######] 40% (5000 руб.)',
    'Транспорт: [####] 20% (2500 руб.)',
    '',
    'Операции (5):',
    '2026-10-20 21:00  -5 000,00  Развлечения',
    '2026-10-12 08:15  -2 500,00  Транспорт  проездной',
    '2026-10-10 19:00  -2 000,00  Еда',
    '2026-10-03 12:30  -3 000,00  Еда  продукты на неделю',
    '2026-10-01 10:00  +60 000,00  Зарплата',
    '',
  ].join('\n'));
});

test('текстовый отчёт без операций', () => {
  const text = buildTextReport([], {});
  assert.match(text, /^Финансовый отчёт за всё время\n/);
  assert.match(text, /расходов нет/);
  assert.match(text, /Операции \(0\):\n$/);
});

test('CSV: заголовок, операции, итоги', () => {
  const rows = buildCsv(filterOperations(operations)).split('\r\n');
  assert.equal(rows[0], 'Дата;Время;Тип;Категория;Сумма;Описание');
  assert.equal(rows[2], '2026-10-12;08:15;Расход;Транспорт;2500,00;проездной');
  assert.equal(rows[5], '2026-10-01;10:00;Доход;Зарплата;60000,00;');
  assert.deepEqual(rows.slice(6), ['', 'Доходы;;;;60000,00;', 'Расходы;;;;12500,00;', 'Баланс;;;;47500,00;', '']);
});

test('CSV: поле с разделителем и кавычками берётся в кавычки', () => {
  const state = stateWith([['2026-10-01T10:00', 'expense', 'Еда', 100, 'кафе "Утро"; завтрак']]);
  assert.equal(buildCsv(state.operations).split('\r\n')[1],
    '2026-10-01;10:00;Расход;Еда;100,00;"кафе ""Утро""; завтрак"');
});
