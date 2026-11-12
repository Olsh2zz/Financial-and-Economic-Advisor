// Файл: storage.js
// Назначение: сохранение длинного текста в хранилище «ключ, значение» по частям.
// Облачное хранилище Telegram принимает значения не длиннее 4096 символов,
// поэтому состояние режется на куски, а их число пишется в отдельный ключ.

// Запас относительно предела в 4096 символов.
export const CHUNK_SIZE = 3500;

export const META_KEY = 'fa_meta';
export const CHUNK_PREFIX = 'fa_chunk_';

// Хранилище передаётся объектом с тремя асинхронными методами:
// get(key) -> строка или null, set(key, value), remove(keys).

function chunkKey(index) {
  return `${CHUNK_PREFIX}${index}`;
}

function isHighSurrogate(code) {
  return code >= 0xd800 && code <= 0xdbff;
}

// Режет текст на куски не длиннее size. Суррогатную пару (например, эмодзи
// в описании операции) не разрывает: половина пары испортилась бы при передаче.
export function splitText(text, size = CHUNK_SIZE) {
  const chunks = [];
  let start = 0;
  while (start < text.length) {
    let end = Math.min(start + size, text.length);
    if (end < text.length && end - start > 1 && isHighSurrogate(text.charCodeAt(end - 1))) {
      end -= 1;
    }
    chunks.push(text.slice(start, end));
    start = end;
  }
  return chunks;
}

async function readChunkCount(store) {
  const meta = await store.get(META_KEY);
  if (meta === null || meta === '') {
    return 0;
  }
  const count = Number(meta);
  if (!Number.isInteger(count) || count < 0) {
    throw new Error('В хранилище повреждена запись о числе частей.');
  }
  return count;
}

// Записывает текст. Число частей пишется последним: если запись оборвётся
// раньше, при чтении останется виден прежний счётчик.
export async function saveText(store, text) {
  const previous = await readChunkCount(store).catch(() => 0);
  const chunks = splitText(text);
  for (const [index, chunk] of chunks.entries()) {
    await store.set(chunkKey(index), chunk);
  }
  await store.set(META_KEY, String(chunks.length));
  const stale = [];
  for (let index = chunks.length; index < previous; index += 1) {
    stale.push(chunkKey(index));
  }
  if (stale.length > 0) {
    await store.remove(stale);
  }
}

// Читает текст. Возвращает null, если ничего не сохранено.
export async function loadText(store) {
  const count = await readChunkCount(store);
  if (count === 0) {
    return null;
  }
  const chunks = [];
  for (let index = 0; index < count; index += 1) {
    const chunk = await store.get(chunkKey(index));
    if (chunk === null) {
      throw new Error(`В хранилище не хватает части ${index + 1} из ${count}.`);
    }
    chunks.push(chunk);
  }
  return chunks.join('');
}
