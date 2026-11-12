// Файл: helpers.js
// Назначение: хранилище в памяти для тестов вместо облака Telegram.

export function memoryStore() {
  const items = new Map();
  return {
    items,
    get: async (key) => items.get(key) ?? null,
    set: async (key, value) => { items.set(key, value); },
    remove: async (keys) => { keys.forEach((key) => items.delete(key)); },
  };
}
