// Файл: ledger.js
// Назначение: учёт операций и категорий: добавление, изменение, удаление,
// фильтрация. Функции не меняют переданное состояние, а возвращают новое.

import { MAX_AMOUNT } from './money.js';
import {
  KINDS, KIND_EXPENSE, MAX_CATEGORY_LENGTH, MAX_NOTE_LENGTH, isValidStamp,
} from './state.js';

function fail(error) {
  return { ok: false, error };
}

// Проверяет поля операции. Сумма ожидается уже в копейках.
function checkDraft(state, draft) {
  if (!isValidStamp(draft.at)) {
    return fail('Укажите дату и время операции.');
  }
  if (!Number.isInteger(draft.amount) || draft.amount <= 0 || draft.amount > MAX_AMOUNT) {
    return fail('Сумма должна быть больше нуля.');
  }
  if (!KINDS.includes(draft.kind)) {
    return fail('Выберите тип операции: доход или расход.');
  }
  if (!state.categories[draft.kind].includes(draft.category)) {
    return fail('Выберите категорию из списка.');
  }
  const note = String(draft.note ?? '').trim();
  if (note.length > MAX_NOTE_LENGTH) {
    return fail(`Описание не длиннее ${MAX_NOTE_LENGTH} символов.`);
  }
  return {
    ok: true,
    fields: { at: draft.at, amount: draft.amount, kind: draft.kind, category: draft.category, note },
  };
}

// Добавляет операцию. Возвращает { ok, state, operation } или { ok: false, error }.
export function addOperation(state, draft) {
  const checked = checkDraft(state, draft);
  if (!checked.ok) {
    return checked;
  }
  const operation = { id: state.nextId, ...checked.fields };
  return {
    ok: true,
    operation,
    state: { ...state, nextId: state.nextId + 1, operations: [...state.operations, operation] },
  };
}

// Заменяет все поля операции с номером id, сам номер остаётся прежним.
export function updateOperation(state, id, draft) {
  if (!state.operations.some((item) => item.id === id)) {
    return fail('Операция не найдена.');
  }
  const checked = checkDraft(state, draft);
  if (!checked.ok) {
    return checked;
  }
  const operation = { id, ...checked.fields };
  return {
    ok: true,
    operation,
    state: { ...state, operations: state.operations.map((item) => (item.id === id ? operation : item)) },
  };
}

export function removeOperation(state, id) {
  if (!state.operations.some((item) => item.id === id)) {
    return fail('Операция не найдена.');
  }
  return { ok: true, state: { ...state, operations: state.operations.filter((item) => item.id !== id) } };
}

// Отбирает операции. Все поля фильтра необязательны:
// from и to задают даты «ГГГГ-ММ-ДД» включительно, category и kind точное совпадение.
// Результат отсортирован от новых к старым.
export function filterOperations(operations, filter = {}) {
  const { from = '', to = '', category = '', kind = '' } = filter;
  return operations
    .filter((item) => {
      const day = item.at.slice(0, 10);
      return (from === '' || day >= from)
        && (to === '' || day <= to)
        && (category === '' || item.category === category)
        && (kind === '' || item.kind === kind);
    })
    .sort((left, right) => right.at.localeCompare(left.at) || right.id - left.id);
}

function checkCategoryName(state, kind, rawName) {
  if (!KINDS.includes(kind)) {
    return fail('Неизвестный тип категории.');
  }
  const name = String(rawName ?? '').trim();
  if (name === '') {
    return fail('Введите название категории.');
  }
  if (name.length > MAX_CATEGORY_LENGTH) {
    return fail(`Название категории не длиннее ${MAX_CATEGORY_LENGTH} символов.`);
  }
  const lower = name.toLowerCase();
  if (state.categories[kind].some((item) => item.toLowerCase() === lower)) {
    return fail(`Категория «${name}» уже есть.`);
  }
  return { ok: true, name };
}

export function addCategory(state, kind, rawName) {
  const checked = checkCategoryName(state, kind, rawName);
  if (!checked.ok) {
    return checked;
  }
  return {
    ok: true,
    state: {
      ...state,
      categories: { ...state.categories, [kind]: [...state.categories[kind], checked.name] },
    },
  };
}

// Удаляет категорию вместе с её лимитом. Категорию с операциями удалить нельзя:
// иначе операции остались бы без категории.
export function removeCategory(state, kind, name) {
  if (!KINDS.includes(kind) || !state.categories[kind].includes(name)) {
    return fail('Категория не найдена.');
  }
  const used = state.operations.filter((item) => item.kind === kind && item.category === name).length;
  if (used > 0) {
    return fail(`Категорию «${name}» нельзя удалить: в ней есть операции (${used}).`);
  }
  if (state.categories[kind].length === 1) {
    return fail('Нельзя удалить последнюю категорию.');
  }
  const limits = { ...state.limits };
  if (kind === KIND_EXPENSE) {
    delete limits[name];
  }
  return {
    ok: true,
    state: {
      ...state,
      limits,
      categories: { ...state.categories, [kind]: state.categories[kind].filter((item) => item !== name) },
    },
  };
}
