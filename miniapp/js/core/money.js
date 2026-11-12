// Файл: money.js
// Назначение: разбор и вывод денежных сумм. Суммы хранятся целым числом
// копеек, чтобы при сложении не накапливалась ошибка дробных чисел.

export const KOPECKS_IN_RUBLE = 100;

// Верхняя граница одной суммы: 999 999 999,99 руб.
export const MAX_AMOUNT = 99_999_999_999;

const NBSP = ' ';

// Разбирает строку вида «1500», «1 500,5» или «1500.50» в копейки.
// Возвращает { ok: true, value } или { ok: false, error } с текстом для пользователя.
export function parseAmount(text) {
  const cleaned = String(text ?? '').replace(/[\s ]/g, '').replace(',', '.');
  if (cleaned === '') {
    return { ok: false, error: 'Введите сумму.' };
  }
  if (!/^\d+(\.\d{1,2})?$/.test(cleaned)) {
    return { ok: false, error: 'Сумма пишется цифрами, после запятой не больше двух знаков.' };
  }
  const [rubles, kopecks = ''] = cleaned.split('.');
  const value = Number(rubles) * KOPECKS_IN_RUBLE + Number(kopecks.padEnd(2, '0'));
  if (value <= 0) {
    return { ok: false, error: 'Сумма должна быть больше нуля.' };
  }
  if (value > MAX_AMOUNT) {
    return { ok: false, error: 'Сумма не может быть больше 999 999 999,99 руб.' };
  }
  return { ok: true, value };
}

function splitAmount(kopecks) {
  const absolute = Math.abs(kopecks);
  return {
    sign: kopecks < 0 ? '-' : '',
    rubles: Math.floor(absolute / KOPECKS_IN_RUBLE),
    rest: String(absolute % KOPECKS_IN_RUBLE).padStart(2, '0'),
  };
}

// Полная запись для таблиц: «12 500,00». Разряды разделены неразрывным пробелом.
export function formatAmount(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  const grouped = String(rubles).replace(/\B(?=(\d{3})+$)/g, NBSP);
  return `${sign}${grouped},${rest}`;
}

// Запись для отчёта, как в образце из задания: «5000 руб.».
// Копейки показываются, только если они не нулевые.
export function formatRubles(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  return rest === '00' ? `${sign}${rubles} руб.` : `${sign}${rubles},${rest} руб.`;
}

// Запись без разделителей разрядов: для поля ввода и для CSV.
export function formatPlain(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  return `${sign}${rubles},${rest}`;
}
