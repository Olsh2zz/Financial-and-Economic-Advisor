// Файл: budget.js
// Назначение: финансовое планирование: месячные лимиты по категориям расходов,
// проверка превышения и прогноз накоплений по средним доходам и расходам.

import { MAX_AMOUNT } from './money.js';
import { KIND_EXPENSE, KIND_INCOME } from './state.js';

const MONTHS_IN_YEAR = 12;

// Сроки прогноза накоплений в месяцах.
export const FORECAST_HORIZONS = [3, 6, 12];

function fail(error) {
  return { ok: false, error };
}

// Задаёт месячный лимит в копейках для категории расходов.
export function setLimit(state, category, amount) {
  if (!state.categories[KIND_EXPENSE].includes(category)) {
    return fail('Лимит задаётся только для категории расходов.');
  }
  if (!Number.isInteger(amount) || amount <= 0 || amount > MAX_AMOUNT) {
    return fail('Лимит должен быть больше нуля.');
  }
  return { ok: true, state: { ...state, limits: { ...state.limits, [category]: amount } } };
}

export function clearLimit(state, category) {
  const limits = { ...state.limits };
  delete limits[category];
  return { ok: true, state: { ...state, limits } };
}

// Сумма расходов по категории за месяц. Месяц задаётся строкой «ГГГГ-ММ».
export function monthSpent(operations, category, month) {
  return operations
    .filter((item) => item.kind === KIND_EXPENSE && item.category === category && item.at.startsWith(month))
    .reduce((sum, item) => sum + item.amount, 0);
}

// Состояние лимита категории за месяц.
// Возвращает null, если лимит не задан, иначе { limit, spent, left, exceeded }.
// left отрицательный, когда лимит превышен.
export function checkLimit(state, category, month) {
  const limit = state.limits[category];
  if (limit === undefined) {
    return null;
  }
  const spent = monthSpent(state.operations, category, month);
  return { limit, spent, left: limit - spent, exceeded: spent > limit };
}

function monthIndex(stamp) {
  return Number(stamp.slice(0, 4)) * MONTHS_IN_YEAR + Number(stamp.slice(5, 7));
}

// Прогноз «сберегаемого потенциала». Средние считаются по всем календарным
// месяцам от первой операции до последней включительно: месяц без операций
// тоже входит в счёт, иначе средний расход получился бы завышенным.
// Возвращает null, если операций нет.
export function forecast(operations) {
  if (operations.length === 0) {
    return null;
  }
  const indexes = operations.map((item) => monthIndex(item.at));
  const months = Math.max(...indexes) - Math.min(...indexes) + 1;
  const total = (kind) => operations
    .filter((item) => item.kind === kind)
    .reduce((sum, item) => sum + item.amount, 0);
  const averageIncome = Math.round(total(KIND_INCOME) / months);
  const averageExpense = Math.round(total(KIND_EXPENSE) / months);
  const potential = averageIncome - averageExpense;
  return {
    months,
    averageIncome,
    averageExpense,
    potential,
    projections: FORECAST_HORIZONS.map((horizon) => ({ months: horizon, amount: potential * horizon })),
  };
}
