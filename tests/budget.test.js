// Файл: budget.test.js
// Назначение: тесты лимитов и прогноза накоплений.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import { checkLimit, clearLimit, forecast, monthSpent, setLimit } from '../miniapp/js/core/budget.js';
import { createDefaultState } from '../miniapp/js/core/state.js';
import { SAMPLE_ROWS, stateWith } from './helpers.js';

test('лимит задаётся только для категории расходов', () => {
  const state = createDefaultState();
  const result = setLimit(state, 'Развлечения', 1000000);
  assert.equal(result.ok, true);
  assert.deepEqual(result.state.limits, { Развлечения: 1000000 });
  assert.equal(setLimit(state, 'Зарплата', 1000000).ok, false);
  assert.equal(setLimit(state, 'Космос', 1000000).ok, false);
  assert.equal(setLimit(state, 'Еда', 0).ok, false);
  assert.equal(setLimit(state, 'Еда', 10.5).ok, false);
  assert.deepEqual(clearLimit(result.state, 'Развлечения').state.limits, {});
});

test('расход категории за месяц', () => {
  const { operations } = stateWith([
    ...SAMPLE_ROWS,
    ['2026-09-30T23:59', 'expense', 'Еда', 700],
    ['2026-11-01T00:00', 'expense', 'Еда', 900],
  ]);
  assert.equal(monthSpent(operations, 'Еда', '2026-10'), 500000);
  assert.equal(monthSpent(operations, 'Еда', '2026-09'), 70000);
  assert.equal(monthSpent(operations, 'Еда', '2026-12'), 0);
  // Доход в расчёт лимита не попадает.
  assert.equal(monthSpent(operations, 'Зарплата', '2026-10'), 0);
});

test('контроль лимита: не задан, не превышен, ровно на границе, превышен', () => {
  let state = stateWith(SAMPLE_ROWS);
  assert.equal(checkLimit(state, 'Еда', '2026-10'), null);

  state = setLimit(state, 'Еда', 600000).state;
  assert.deepEqual(checkLimit(state, 'Еда', '2026-10'),
    { limit: 600000, spent: 500000, left: 100000, exceeded: false });

  // Потратить ровно лимит ещё не превышение.
  state = setLimit(state, 'Еда', 500000).state;
  assert.equal(checkLimit(state, 'Еда', '2026-10').exceeded, false);

  state = setLimit(state, 'Еда', 499999).state;
  assert.deepEqual(checkLimit(state, 'Еда', '2026-10'),
    { limit: 499999, spent: 500000, left: -1, exceeded: true });

  // В другом месяце лимит начинается заново.
  assert.equal(checkLimit(state, 'Еда', '2026-11').spent, 0);
});

test('прогноз по одному месяцу', () => {
  const { operations } = stateWith(SAMPLE_ROWS);
  assert.deepEqual(forecast(operations), {
    months: 1,
    averageIncome: 6000000,
    averageExpense: 1250000,
    potential: 4750000,
    projections: [
      { months: 3, amount: 14250000 },
      { months: 6, amount: 28500000 },
      { months: 12, amount: 57000000 },
    ],
  });
});

test('прогноз: месяц без операций входит в среднее, год может смениться', () => {
  const { operations } = stateWith([
    ['2025-11-05T10:00', 'income', 'Зарплата', 90000],
    ['2025-11-20T10:00', 'expense', 'Жильё', 30000],
    ['2026-02-05T10:00', 'income', 'Зарплата', 90000],
    ['2026-02-20T10:00', 'expense', 'Жильё', 50000],
  ]);
  const result = forecast(operations);
  // Ноябрь, декабрь, январь, февраль.
  assert.equal(result.months, 4);
  assert.equal(result.averageIncome, 4500000);
  assert.equal(result.averageExpense, 2000000);
  assert.equal(result.potential, 2500000);
});

test('прогноз без операций и с перерасходом', () => {
  assert.equal(forecast([]), null);
  const { operations } = stateWith([
    ['2026-10-01T10:00', 'income', 'Зарплата', 1000],
    ['2026-10-02T10:00', 'expense', 'Еда', 1500],
  ]);
  const result = forecast(operations);
  assert.equal(result.potential, -50000);
  assert.equal(result.projections[0].amount, -150000);
});
