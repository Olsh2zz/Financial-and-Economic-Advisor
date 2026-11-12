// Файл: money.test.js
// Назначение: тесты разбора и вывода денежных сумм.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import { formatAmount, formatPlain, formatRubles, parseAmount } from '../miniapp/js/core/money.js';

test('сумма разбирается в копейки', () => {
  assert.deepEqual(parseAmount('1500'), { ok: true, value: 150000 });
  assert.deepEqual(parseAmount('1500,5'), { ok: true, value: 150050 });
  assert.deepEqual(parseAmount('1500.05'), { ok: true, value: 150005 });
  assert.deepEqual(parseAmount(' 1 500,50 '), { ok: true, value: 150050 });
  assert.deepEqual(parseAmount('0,01'), { ok: true, value: 1 });
});

test('сумма без ошибки округления дробных чисел', () => {
  // 0.1 + 0.2 в дробных числах не равно 0.3, в копейках равно.
  assert.equal(parseAmount('0,1').value + parseAmount('0,2').value, parseAmount('0,3').value);
  assert.equal(parseAmount('1,15').value, 115);
});

test('некорректная сумма отклоняется с пояснением', () => {
  for (const text of ['', '   ', 'abc', '12a', '-5', '1,234', '1,2,3', '1e3', ',5', '5,']) {
    const result = parseAmount(text);
    assert.equal(result.ok, false, `«${text}» должно быть отклонено`);
    assert.ok(result.error.length > 0);
  }
  assert.equal(parseAmount('0').error, 'Сумма должна быть больше нуля.');
  assert.equal(parseAmount('0,00').ok, false);
});

test('граница наибольшей суммы', () => {
  assert.equal(parseAmount('999999999,99').ok, true);
  assert.equal(parseAmount('1000000000').ok, false);
});

test('вывод суммы', () => {
  assert.equal(formatAmount(1250000), '12 500,00');
  assert.equal(formatAmount(5), '0,05');
  assert.equal(formatAmount(-123456789), '-1 234 567,89');
  assert.equal(formatRubles(500000), '5000 руб.');
  assert.equal(formatRubles(500050), '5000,50 руб.');
  assert.equal(formatRubles(-250000), '-2500 руб.');
  assert.equal(formatPlain(150050), '1500,50');
});

test('выведенная сумма разбирается обратно', () => {
  for (const kopecks of [1, 99, 100, 150050, 99999999999]) {
    assert.equal(parseAmount(formatPlain(kopecks)).value, kopecks);
    assert.equal(parseAmount(formatAmount(kopecks)).value, kopecks);
  }
});
