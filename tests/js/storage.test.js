// Файл: storage.test.js
// Назначение: тесты записи текста в хранилище по частям и вывода сумм на экране.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import { formatAmount, formatPlain, formatRubles, toStamp } from '../../miniapp/js/core/format.js';
import {
  CHUNK_PREFIX, CHUNK_SIZE, META_KEY, loadText, saveText, splitText,
} from '../../miniapp/js/core/storage.js';
import { memoryStore } from './helpers.js';

test('текст режется на части без разрыва суррогатной пары', () => {
  assert.deepEqual(splitText('abcdefg', 3), ['abc', 'def', 'g']);
  assert.deepEqual(splitText('', 3), []);
  // Эмодзи занимает две кодовые единицы и целиком уходит в следующую часть.
  assert.deepEqual(splitText('ab😀cd', 3), ['ab', '😀c', 'd']);
});

test('длинный текст сохраняется по частям и читается обратно', async () => {
  const text = 'обед 🍲 и кофе; '.repeat(1000);
  const store = memoryStore();
  await saveText(store, text);
  const count = Number(store.items.get(META_KEY));
  assert.ok(count > 1);
  for (let index = 0; index < count; index += 1) {
    assert.ok(store.items.get(`${CHUNK_PREFIX}${index}`).length <= CHUNK_SIZE);
  }
  assert.equal(await loadText(store), text);
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

test('вывод сумм и момента операции', () => {
  assert.equal(formatAmount(1250000), '12\u00a0500,00');
  assert.equal(formatAmount(-123456789), '-1\u00a0234\u00a0567,89');
  assert.equal(formatAmount(5), '0,05');
  assert.equal(formatRubles(500000), '5000 руб.');
  assert.equal(formatRubles(500050), '5000,50 руб.');
  assert.equal(formatPlain(150050), '1500,50');
  assert.equal(toStamp(new Date(2026, 0, 5, 7, 3)), '2026-01-05T07:03');
});
