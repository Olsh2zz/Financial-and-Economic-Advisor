// Файл: state.js
// Назначение: описание данных приложения (операции, категории, лимиты),
// начальное состояние и перевод состояния в JSON и обратно с проверкой.

export const STATE_VERSION = 1;

export const KIND_INCOME = 'income';
export const KIND_EXPENSE = 'expense';
export const KINDS = [KIND_INCOME, KIND_EXPENSE];

export const KIND_TITLES = {
  [KIND_INCOME]: 'Доход',
  [KIND_EXPENSE]: 'Расход',
};

export const MAX_CATEGORY_LENGTH = 30;
export const MAX_NOTE_LENGTH = 200;

// Формат момента операции: местное время без часового пояса, «2026-11-12T09:30».
// Такие строки сравниваются как текст, и порядок совпадает с порядком по времени.
const STAMP_PATTERN = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})$/;

// Проверяет и формат, и то, что такая дата существует (30 февраля не пройдёт).
export function isValidStamp(stamp) {
  const match = STAMP_PATTERN.exec(String(stamp));
  if (!match) {
    return false;
  }
  const [year, month, day, hour, minute] = match.slice(1).map(Number);
  const date = new Date(year, month - 1, day, hour, minute);
  return date.getFullYear() === year && date.getMonth() === month - 1
    && date.getDate() === day && hour < 24 && minute < 60;
}

// Переводит Date в строку формата операции по местному времени.
export function toStamp(date) {
  const pad = (number) => String(number).padStart(2, '0');
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
    + `T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

export function createDefaultState() {
  return {
    version: STATE_VERSION,
    nextId: 1,
    categories: {
      [KIND_INCOME]: ['Зарплата', 'Подработка', 'Подарки'],
      [KIND_EXPENSE]: ['Еда', 'Транспорт', 'Жильё', 'Развлечения', 'Здоровье'],
    },
    // Месячные лимиты расходов: название категории -> копейки.
    limits: {},
    operations: [],
  };
}

export function serialize(state) {
  return JSON.stringify(state);
}

function isCategoryList(list) {
  return Array.isArray(list)
    && list.every((name) => typeof name === 'string' && name !== '')
    && new Set(list).size === list.length;
}

function isOperation(operation, categories) {
  return operation !== null && typeof operation === 'object'
    && Number.isInteger(operation.id) && operation.id > 0
    && isValidStamp(operation.at)
    && Number.isInteger(operation.amount) && operation.amount > 0
    && KINDS.includes(operation.kind)
    && categories[operation.kind].includes(operation.category)
    && typeof operation.note === 'string';
}

// Разбирает сохранённый текст. Испорченные данные не чинит, а сообщает об ошибке,
// чтобы приложение не перезаписало их молча пустым состоянием.
export function deserialize(text) {
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    return { ok: false, error: 'Сохранённые данные не читаются как JSON.' };
  }
  if (data === null || typeof data !== 'object' || data.version !== STATE_VERSION) {
    return { ok: false, error: 'Сохранённые данные записаны в неизвестной версии формата.' };
  }
  const { categories, limits, operations, nextId } = data;
  if (categories === null || typeof categories !== 'object'
    || !KINDS.every((kind) => isCategoryList(categories[kind]))) {
    return { ok: false, error: 'В сохранённых данных повреждён список категорий.' };
  }
  if (limits === null || typeof limits !== 'object' || Array.isArray(limits)
    || !Object.entries(limits).every(([name, amount]) =>
      categories[KIND_EXPENSE].includes(name) && Number.isInteger(amount) && amount > 0)) {
    return { ok: false, error: 'В сохранённых данных повреждены лимиты.' };
  }
  if (!Array.isArray(operations) || !operations.every((item) => isOperation(item, categories))) {
    return { ok: false, error: 'В сохранённых данных повреждён список операций.' };
  }
  const maxId = operations.reduce((max, item) => Math.max(max, item.id), 0);
  if (!Number.isInteger(nextId) || nextId <= maxId
    || new Set(operations.map((item) => item.id)).size !== operations.length) {
    return { ok: false, error: 'В сохранённых данных повреждены номера операций.' };
  }
  return {
    ok: true,
    state: {
      version: STATE_VERSION,
      nextId,
      categories: {
        [KIND_INCOME]: [...categories[KIND_INCOME]],
        [KIND_EXPENSE]: [...categories[KIND_EXPENSE]],
      },
      limits: { ...limits },
      operations: operations.map(({ id, at, amount, kind, category, note }) =>
        ({ id, at, amount, kind, category, note })),
    },
  };
}
