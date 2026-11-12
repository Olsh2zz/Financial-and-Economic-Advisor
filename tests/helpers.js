// Файл: helpers.js
// Назначение: общие заготовки для тестов: состояние с набором операций
// и хранилище в памяти вместо облака Telegram.

import { addOperation } from '../miniapp/js/core/ledger.js';
import { createDefaultState } from '../miniapp/js/core/state.js';

// Добавляет операции из списка строк [at, kind, category, рубли, note].
export function stateWith(rows, state = createDefaultState()) {
  let current = state;
  for (const [at, kind, category, rubles, note = ''] of rows) {
    const result = addOperation(current, { at, kind, category, amount: Math.round(rubles * 100), note });
    if (!result.ok) {
      throw new Error(result.error);
    }
    current = result.state;
  }
  return current;
}

// Набор из образца в задании: расходы 12500 руб., из них еда 5000 (40%)
// и транспорт 2500 (20%).
export const SAMPLE_ROWS = [
  ['2026-10-01T10:00', 'income', 'Зарплата', 60000],
  ['2026-10-03T12:30', 'expense', 'Еда', 3000, 'продукты на неделю'],
  ['2026-10-10T19:00', 'expense', 'Еда', 2000],
  ['2026-10-12T08:15', 'expense', 'Транспорт', 2500, 'проездной'],
  ['2026-10-20T21:00', 'expense', 'Развлечения', 5000],
];

// Хранилище в памяти с тем же интерфейсом, что у облака Telegram.
export function memoryStore() {
  const items = new Map();
  return {
    items,
    get: async (key) => items.get(key) ?? null,
    set: async (key, value) => { items.set(key, value); },
    remove: async (keys) => { keys.forEach((key) => items.delete(key)); },
  };
}
