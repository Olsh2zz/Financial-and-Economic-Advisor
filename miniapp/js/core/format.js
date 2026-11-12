// Файл: format.js
// Назначение: вывод сумм и дат на экране. Только оформление: расчёты
// и разбор ввода выполняет ядро на C++. Суммы приходят из ядра в копейках.

const KOPECKS_IN_RUBLE = 100;
const NBSP = ' ';

function splitAmount(kopecks) {
  const absolute = Math.abs(kopecks);
  return {
    sign: kopecks < 0 ? '-' : '',
    rubles: Math.floor(absolute / KOPECKS_IN_RUBLE),
    rest: String(absolute % KOPECKS_IN_RUBLE).padStart(2, '0'),
  };
}

// «12 500,00»: разряды разделены неразрывным пробелом, чтобы сумма не переносилась.
export function formatAmount(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  const grouped = String(rubles).replace(/\B(?=(\d{3})+$)/g, NBSP);
  return `${sign}${grouped},${rest}`;
}

// «5000 руб.»; копейки показываются, только если они не нулевые.
export function formatRubles(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  return rest === '00' ? `${sign}${rubles} руб.` : `${sign}${rubles},${rest} руб.`;
}

// «1500,50»: в таком виде сумма подставляется в поле ввода.
export function formatPlain(kopecks) {
  const { sign, rubles, rest } = splitAmount(kopecks);
  return `${sign}${rubles},${rest}`;
}

// Переводит Date в формат момента операции «2026-11-12T09:30» по местному времени.
export function toStamp(date) {
  const pad = (number) => String(number).padStart(2, '0');
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
    + `T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}
