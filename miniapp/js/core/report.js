// Файл: report.js
// Назначение: статистика за период, доли расходов по категориям и экспорт
// отчёта в текст и CSV.

import { formatAmount, formatPlain, formatRubles } from './money.js';
import { KIND_EXPENSE, KIND_INCOME, KIND_TITLES } from './state.js';

// Масштаб текстовой шкалы взят из образца в задании: 40% это 7 решёток,
// 20% это 4 решётки.
const BAR_CELLS = 7;
const BAR_PERCENT = 40;

const CSV_SEPARATOR = ';';

// Итоги по списку операций: { income, expense, balance } в копейках.
export function totals(operations) {
  const sum = (kind) => operations
    .filter((item) => item.kind === kind)
    .reduce((acc, item) => acc + item.amount, 0);
  const income = sum(KIND_INCOME);
  const expense = sum(KIND_EXPENSE);
  return { income, expense, balance: income - expense };
}

// Расходы по категориям: [{ category, amount, percent }], от больших к меньшим.
// percent это целая доля от всех расходов; сумма округлённых долей может
// отличаться от 100 на единицу-две.
export function expenseShares(operations) {
  const byCategory = new Map();
  for (const item of operations) {
    if (item.kind === KIND_EXPENSE) {
      byCategory.set(item.category, (byCategory.get(item.category) ?? 0) + item.amount);
    }
  }
  const total = [...byCategory.values()].reduce((acc, amount) => acc + amount, 0);
  return [...byCategory.entries()]
    .map(([category, amount]) => ({ category, amount, percent: Math.round((amount * 100) / total) }))
    .sort((left, right) => right.amount - left.amount || left.category.localeCompare(right.category, 'ru'));
}

// Строка отчёта по образцу из задания: «Еда: [#######] 40% (5000 руб.)».
export function barLine({ category, amount, percent }) {
  const cells = Math.round((percent * BAR_CELLS) / BAR_PERCENT);
  return `${category}: [${'#'.repeat(cells)}] ${percent}% (${formatRubles(amount)})`;
}

function periodTitle({ from = '', to = '' } = {}) {
  if (from === '' && to === '') {
    return 'за всё время';
  }
  return `${from === '' ? 'с начала' : `с ${from}`} ${to === '' ? 'по сегодня' : `по ${to}`}`;
}

function stampTitle(stamp) {
  return stamp.replace('T', ' ');
}

// Текстовый отчёт: итоги, расходы по категориям и список операций.
// operations должны быть уже отобраны за нужный период.
export function buildTextReport(operations, period) {
  const { income, expense, balance } = totals(operations);
  const lines = [
    `Финансовый отчёт ${periodTitle(period)}`,
    '',
    `Доходы:  ${formatRubles(income)}`,
    `Расходы: ${formatRubles(expense)}`,
    `Баланс:  ${formatRubles(balance)}`,
    '',
    'Расходы по категориям:',
  ];
  const shares = expenseShares(operations);
  lines.push(...(shares.length > 0 ? shares.map(barLine) : ['расходов нет']));
  lines.push('', `Операции (${operations.length}):`);
  for (const item of operations) {
    const sign = item.kind === KIND_INCOME ? '+' : '-';
    const note = item.note === '' ? '' : `  ${item.note}`;
    lines.push(`${stampTitle(item.at)}  ${sign}${formatAmount(item.amount)}  ${item.category}${note}`);
  }
  return `${lines.join('\n')}\n`;
}

// Поле CSV берётся в кавычки, если в нём есть разделитель, кавычка или перевод строки.
function csvField(value) {
  const text = String(value);
  return /[;"\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

// CSV с разделителем «;» и запятой в суммах: так файл без настройки
// открывается в русском Excel. После операций идут строки с итогами.
export function buildCsv(operations) {
  const { income, expense, balance } = totals(operations);
  const rows = [
    ['Дата', 'Время', 'Тип', 'Категория', 'Сумма', 'Описание'],
    ...operations.map((item) => [
      item.at.slice(0, 10), item.at.slice(11), KIND_TITLES[item.kind],
      item.category, formatPlain(item.amount), item.note,
    ]),
    [],
    ['Доходы', '', '', '', formatPlain(income), ''],
    ['Расходы', '', '', '', formatPlain(expense), ''],
    ['Баланс', '', '', '', formatPlain(balance), ''],
  ];
  return rows.map((row) => row.map(csvField).join(CSV_SEPARATOR)).join('\r\n') + '\r\n';
}
