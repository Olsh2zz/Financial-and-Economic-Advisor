// Файл: app.js
// Назначение: интерфейс мини-приложения XPay: рисует экраны, читает ввод и вызывает
// расчёты из папки core. Самих расчётов здесь нет.

import { checkLimit, clearLimit, forecast, setLimit } from './core/budget.js';
import {
  addCategory, addOperation, filterOperations, removeCategory, removeOperation, updateOperation,
} from './core/ledger.js';
import { formatAmount, formatPlain, formatRubles, parseAmount } from './core/money.js';
import { buildCsv, buildTextReport, expenseShares, totals } from './core/report.js';
import {
  KIND_EXPENSE, KIND_INCOME, KIND_TITLES, createDefaultState, deserialize, serialize, toStamp,
} from './core/state.js';
import { loadText, saveText } from './core/storage.js';
import * as telegram from './platform/telegram.js';

const APP_NAME = 'XPay';

const TOAST_MS = 4000;

// Цвет фона из css/style.css: в него же красится шапка клиента Telegram.
const BACKGROUND_COLOR = '#0a1304';

// Метка порядка байтов: без неё Excel читает CSV в UTF-8 как набор кракозябр.
const BYTE_ORDER_MARK = '﻿';

const view = document.getElementById('view');
const toast = document.getElementById('toast');

let state = createDefaultState();
let currentView = 'operations';
let listFilter = { from: '', to: '', category: '', kind: '' };
let reportPeriod = { from: '', to: '' };
let filterOpen = false;
// Пока данные не прочитаны без ошибок, сохранять нельзя: иначе пустое
// состояние затёрло бы то, что лежит в хранилище.
let canSave = false;
let saveQueue = Promise.resolve();
let toastTimer = 0;

// Создаёт элемент. props задаёт свойства DOM, dataset и обработчики on*.
// Текст всегда попадает в документ через textContent, а не через innerHTML,
// поэтому описание операции не может подставить в страницу свою разметку.
function el(tag, props = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(props)) {
    if (key === 'dataset') {
      Object.assign(node.dataset, value);
    } else if (key.startsWith('on')) {
      node.addEventListener(key.slice(2), value);
    } else if (key.startsWith('aria-')) {
      node.setAttribute(key, value);
    } else {
      node[key] = value;
    }
  }
  node.append(...children.filter((child) => child !== null));
  return node;
}

function showToast(message) {
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toast.hidden = true; }, TOAST_MS);
}

// Записи идут строго по очереди, чтобы более ранняя не легла поверх более поздней.
function save() {
  if (!canSave) {
    return;
  }
  const text = serialize(state);
  saveQueue = saveQueue
    .then(() => saveText(telegram.store, text))
    .catch((error) => showToast(`Не удалось сохранить данные: ${error.message}`));
}

// Применяет результат функции из core: при успехе сохраняет и перерисовывает.
function commit(result) {
  if (!result.ok) {
    return false;
  }
  state = result.state;
  save();
  render();
  return true;
}

// Значки нарисованы линиями в сетке 24 на 24. Строки заданы здесь же и не
// содержат пользовательского текста, поэтому их можно вставлять как разметку.
const ICONS = {
  expense: '<path d="M7 17 17 7M9 7h8v8"/>',
  income: '<path d="M17 7 7 17M15 17H7V9"/>',
  report: '<path d="M5 20V10M12 20V4M19 20v-7"/>',
  budget: '<circle cx="12" cy="12" r="8"/><path d="M12 8v4l3 2"/>',
  filter: '<path d="M4 6h16M7 12h10M10 18h4"/>',
};

function icon(name) {
  const node = el('span', { className: 'icon' });
  node.setAttribute('aria-hidden', 'true');
  node.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"`
    + ` stroke-linecap="round" stroke-linejoin="round">${ICONS[name]}</svg>`;
  return node;
}

function signedAmount(operation) {
  const sign = operation.kind === KIND_INCOME ? '+' : '−';
  return el('span', { className: `amount ${operation.kind}` }, `${sign}${formatAmount(operation.amount)}`);
}

// Крупная сумма: рубли большим кеглем, копейки меньшим.
function bigAmount(kopecks) {
  const [rubles, rest] = formatAmount(kopecks).split(',');
  return el('p', { className: 'balance' }, rubles, el('small', {}, `,${rest} ₽`));
}

// Светлая карточка с балансом, доходами и расходами по списку операций.
function heroCard(title, operations, ...extra) {
  const { income, expense, balance } = totals(operations);
  const chip = (name, label, amount) => el('span', { className: 'chip' },
    icon(name), `${label} ${formatAmount(amount)}`);
  return el('section', { className: 'hero' },
    el('p', { className: 'hero-title' }, title),
    bigAmount(balance),
    el('div', { className: 'chips' },
      chip('income', 'Доходы', income),
      chip('expense', 'Расходы', expense)),
    ...extra);
}

function dateField(title, value, onChange) {
  return el('label', { className: 'field' },
    el('span', {}, title),
    el('input', { type: 'date', value, onchange: (event) => onChange(event.target.value) }));
}

function selectField(title, value, options, onChange) {
  const select = el('select', { onchange: (event) => onChange(event.target.value) },
    ...options.map(([optionValue, optionTitle]) => el('option', { value: optionValue }, optionTitle)));
  select.value = value;
  return el('label', { className: 'field' }, el('span', {}, title), select);
}

function allCategories() {
  return [...new Set([...state.categories[KIND_EXPENSE], ...state.categories[KIND_INCOME]])];
}

// Экран «Операции»: баланс, быстрые действия, фильтры и лента операций по дням.

const MONTH_NAMES = [
  'января', 'февраля', 'марта', 'апреля', 'мая', 'июня',
  'июля', 'августа', 'сентября', 'октября', 'ноября', 'декабря',
];

// Заголовок дня в ленте: «Сегодня», «Вчера» или «12 октября 2026».
function dayTitle(day) {
  const now = new Date();
  if (day === toStamp(now).slice(0, 10)) {
    return 'Сегодня';
  }
  now.setDate(now.getDate() - 1);
  if (day === toStamp(now).slice(0, 10)) {
    return 'Вчера';
  }
  return `${Number(day.slice(8))} ${MONTH_NAMES[Number(day.slice(5, 7)) - 1]} ${day.slice(0, 4)}`;
}

function operationRow(operation) {
  return el('button', { type: 'button', className: 'operation', onclick: () => openOperationDialog(operation) },
    el('span', { className: `badge ${operation.kind}` }, icon(operation.kind)),
    el('span', { className: 'grow' },
      el('span', { className: 'name' }, operation.category),
      operation.note === '' ? null : el('span', { className: 'note' }, operation.note)),
    el('span', { className: 'side' },
      signedAmount(operation),
      el('span', { className: 'note' }, operation.at.slice(11))));
}

function renderOperations() {
  const setFilter = (patch) => {
    listFilter = { ...listFilter, ...patch };
    render();
  };
  const filtered = Object.values(listFilter).some((value) => value !== '');
  const operations = filterOperations(state.operations, listFilter);

  const action = (name, label, onclick) => el('button', { type: 'button', className: 'action', onclick },
    icon(name), el('span', {}, label));
  const go = (name) => () => {
    currentView = name;
    render();
  };
  const hero = heroCard(filtered ? 'Баланс по фильтру' : 'Общий баланс', operations,
    el('div', { className: 'quick' },
      action('expense', 'Расход', () => openOperationDialog(null, KIND_EXPENSE)),
      action('income', 'Доход', () => openOperationDialog(null, KIND_INCOME)),
      action('report', 'Отчёт', go('report')),
      action('budget', 'Лимиты', go('budget'))));

  const filterButton = el('button', {
    type: 'button',
    className: `pill${filtered ? ' active' : ''}`,
    'aria-expanded': String(filterOpen),
    onclick: () => {
      filterOpen = !filterOpen;
      render();
    },
  }, icon('filter'), filtered ? 'Фильтр включён' : 'Фильтр');

  const filters = !filterOpen ? null : el('section', { className: 'card filters' },
    dateField('С даты', listFilter.from, (from) => setFilter({ from })),
    dateField('По дату', listFilter.to, (to) => setFilter({ to })),
    selectField('Категория', listFilter.category,
      [['', 'Все'], ...allCategories().map((name) => [name, name])],
      (category) => setFilter({ category })),
    selectField('Тип', listFilter.kind,
      [['', 'Все'], [KIND_INCOME, 'Доходы'], [KIND_EXPENSE, 'Расходы']],
      (kind) => setFilter({ kind })),
    filtered ? el('button', {
      type: 'button', className: 'button small wide',
      onclick: () => setFilter({ from: '', to: '', category: '', kind: '' }),
    }, 'Сбросить фильтр') : null);

  const list = [];
  if (state.operations.length === 0) {
    list.push(el('p', { className: 'hint empty' }, 'Операций пока нет. Первую можно добавить кнопкой «+».'));
  } else if (operations.length === 0) {
    list.push(el('p', { className: 'hint empty' }, 'Под этот фильтр не подходит ни одна операция.'));
  }
  let lastDay = '';
  for (const operation of operations) {
    const day = operation.at.slice(0, 10);
    if (day !== lastDay) {
      list.push(el('h3', {}, dayTitle(day)));
      lastDay = day;
    }
    list.push(operationRow(operation));
  }

  view.replaceChildren(
    el('header', { className: 'top' },
      el('span', { className: 'avatar' }, (telegram.userName || APP_NAME).slice(0, 1).toUpperCase()),
      el('div', {},
        el('strong', {}, telegram.userName || APP_NAME),
        el('p', { className: 'hint' }, telegram.userName ? `${APP_NAME}, личный бюджет` : 'Личный бюджет'))),
    hero,
    el('div', { className: 'row section-head' }, el('h2', { className: 'grow' }, 'Операции'), filterButton),
    ...(filters === null ? [] : [filters]),
    el('section', { className: 'feed' }, ...list));
}

// Экран «Отчёты»: статистика за период, расходы по категориям, прогноз, экспорт.

function renderReport() {
  const setPeriod = (patch) => {
    reportPeriod = { ...reportPeriod, ...patch };
    render();
  };
  const operations = filterOperations(state.operations, reportPeriod);
  const shares = expenseShares(operations);

  const sharesCard = el('section', { className: 'card' },
    el('h3', {}, 'Расходы по категориям'),
    ...(shares.length === 0
      ? [el('p', { className: 'hint' }, 'За этот период расходов нет.')]
      : shares.map((share) => el('div', { className: 'share' },
        el('div', { className: 'row' },
          el('span', {}, share.category),
          el('span', { className: 'amount' }, `${share.percent}% (${formatRubles(share.amount)})`)),
        el('div', { className: 'bar' }, el('i', { style: `width: ${share.percent}%` }))))));

  // Прогноз считается по всей истории, а не по выбранному периоду:
  // среднее за короткий отрезок слишком зависит от случайных трат.
  const outlook = forecast(state.operations);
  const line = (title, amount) => el('div', { className: 'row' },
    el('span', { className: 'grow' }, title),
    el('span', { className: 'amount' }, formatRubles(amount)));
  const forecastCard = el('section', { className: 'card' },
    el('h3', {}, 'Прогноз накоплений'),
    ...(outlook === null
      ? [el('p', { className: 'hint' }, 'Прогноз появится после первой операции.')]
      : [
        el('p', { className: 'hint' }, `Средние за ${outlook.months} мес. истории`),
        line('Доход в месяц', outlook.averageIncome),
        line('Расход в месяц', outlook.averageExpense),
        line('Можно откладывать в месяц', outlook.potential),
        ...outlook.projections.map((item) => line(`Накопится за ${item.months} мес.`, item.amount)),
        outlook.potential < 0
          ? el('p', { className: 'error' }, 'Расходы в среднем больше доходов, откладывать не из чего.')
          : null,
      ]));

  const exportCard = el('section', { className: 'card' },
    el('h3', {}, 'Экспорт отчёта'),
    el('div', { className: 'actions' },
      el('button', {
        type: 'button', className: 'button',
        onclick: () => openExportDialog('report.txt', 'text/plain', buildTextReport(operations, reportPeriod)),
      }, 'Текст (.txt)'),
      el('button', {
        type: 'button', className: 'button',
        onclick: () => openExportDialog('report.csv', 'text/csv', buildCsv(operations)),
      }, 'Таблица (.csv)')));

  view.replaceChildren(
    el('h1', {}, 'Отчёты'),
    heroCard(reportPeriod.from === '' && reportPeriod.to === '' ? 'Баланс за всё время' : 'Баланс за период', operations),
    el('section', { className: 'card filters' },
      dateField('С даты', reportPeriod.from, (from) => setPeriod({ from })),
      dateField('По дату', reportPeriod.to, (to) => setPeriod({ to }))),
    sharesCard,
    forecastCard,
    exportCard);
}

// Экран «Лимиты»: месячный лимит и расход текущего месяца по каждой категории.

function renderBudget() {
  const month = toStamp(new Date()).slice(0, 7);
  const cards = state.categories[KIND_EXPENSE].map((category) => {
    const status = checkLimit(state, category, month);
    const input = el('input', {
      type: 'text', inputMode: 'decimal', autocomplete: 'off', placeholder: 'без лимита',
      value: status === null ? '' : formatPlain(status.limit),
    });
    input.setAttribute('aria-label', `Лимит на «${category}», руб. в месяц`);
    const error = el('p', { className: 'error', hidden: true });
    const apply = () => {
      const parsed = parseAmount(input.value);
      const result = parsed.ok ? setLimit(state, category, parsed.value) : parsed;
      if (!commit(result)) {
        error.textContent = result.error;
        error.hidden = false;
      }
    };
    const form = el('form', {
      className: 'row',
      onsubmit: (event) => {
        event.preventDefault();
        apply();
      },
    },
    el('div', { className: 'grow' }, input),
    el('button', { type: 'submit', className: 'button small' }, 'Задать'),
    status === null ? null : el('button', {
      type: 'button', className: 'button small danger',
      onclick: () => commit(clearLimit(state, category)),
    }, 'Снять'));

    let progress = [el('p', { className: 'hint' }, 'Лимит не задан.')];
    if (status !== null) {
      const percent = Math.min(100, Math.round((status.spent * 100) / status.limit));
      progress = [
        el('div', { className: `bar${status.exceeded ? ' over' : ''}` }, el('i', { style: `width: ${percent}%` })),
        el('p', { className: status.exceeded ? 'error' : 'hint' }, status.exceeded
          ? `Потрачено ${formatRubles(status.spent)} из ${formatRubles(status.limit)}, превышение ${formatRubles(-status.left)}`
          : `Потрачено ${formatRubles(status.spent)} из ${formatRubles(status.limit)}, осталось ${formatRubles(status.left)}`),
      ];
    }
    return el('section', { className: 'card' }, el('strong', {}, category), ...progress, form, error);
  });

  view.replaceChildren(
    el('h1', {}, 'Лимиты'),
    el('p', { className: 'hint' }, 'Лимит задаётся на календарный месяц. Ниже расходы за текущий месяц.'),
    ...cards);
}

// Экран «Категории»: два списка с добавлением и удалением.

function renderCategories() {
  const block = (kind, title) => {
    const input = el('input', { type: 'text', maxLength: 30, autocomplete: 'off', placeholder: 'Новая категория' });
    input.setAttribute('aria-label', `Новая категория, ${title.toLowerCase()}`);
    const error = el('p', { className: 'error', hidden: true });
    const report = (result) => {
      if (!commit(result)) {
        error.textContent = result.error;
        error.hidden = false;
      }
    };
    return el('section', { className: 'card' },
      el('h3', {}, title),
      ...state.categories[kind].map((name) => el('div', { className: 'row' },
        el('span', { className: 'grow' }, name),
        el('button', {
          type: 'button', className: 'button small danger',
          onclick: async () => {
            if (await telegram.showConfirm(`Удалить категорию «${name}»?`)) {
              report(removeCategory(state, kind, name));
            }
          },
        }, 'Удалить'))),
      el('form', {
        className: 'row',
        onsubmit: (event) => {
          event.preventDefault();
          report(addCategory(state, kind, input.value));
        },
      },
      el('div', { className: 'grow' }, input),
      el('button', { type: 'submit', className: 'button small' }, 'Добавить')),
      error);
  };

  view.replaceChildren(
    el('h1', {}, 'Категории'),
    block(KIND_EXPENSE, 'Расходы'),
    block(KIND_INCOME, 'Доходы'),
    el('p', { className: 'hint' }, telegram.usesCloud
      ? 'Данные хранятся в облаке Telegram и видны на всех ваших устройствах.'
      : 'Данные хранятся в этом браузере.'));
}

const RENDERERS = {
  operations: renderOperations,
  report: renderReport,
  budget: renderBudget,
  categories: renderCategories,
};

function render() {
  RENDERERS[currentView]();
  for (const tab of document.querySelectorAll('.tab')) {
    if (tab.dataset.view === currentView) {
      tab.setAttribute('aria-current', 'page');
    } else {
      tab.removeAttribute('aria-current');
    }
  }
}

// Окно операции: одно и то же для добавления и правки.

const operationDialog = document.getElementById('operation-dialog');
const operationForm = document.getElementById('operation-form');
const operationError = document.getElementById('operation-error');
const operationDelete = document.getElementById('operation-delete');
// Номер операции, которая правится, или null при добавлении.
let editedId = null;

function fillCategoryOptions(kind, selected) {
  const select = operationForm.elements.category;
  select.replaceChildren(...state.categories[kind].map((name) => el('option', { value: name }, name)));
  if (state.categories[kind].includes(selected)) {
    select.value = selected;
  }
}

// kind задаёт тип для новой операции; при правке берётся тип самой операции.
function openOperationDialog(operation = null, kind = KIND_EXPENSE) {
  editedId = operation?.id ?? null;
  const { elements } = operationForm;
  document.getElementById('operation-title').textContent =
    operation === null ? 'Новая операция' : 'Правка операции';
  elements.kind.value = operation?.kind ?? kind;
  fillCategoryOptions(elements.kind.value, operation?.category);
  elements.amount.value = operation === null ? '' : formatPlain(operation.amount);
  elements.at.value = operation?.at ?? toStamp(new Date());
  elements.note.value = operation?.note ?? '';
  operationError.hidden = true;
  operationDelete.hidden = operation === null;
  operationDialog.showModal();
}

function limitWarning(operation) {
  if (operation.kind !== KIND_EXPENSE) {
    return null;
  }
  const status = checkLimit(state, operation.category, operation.at.slice(0, 7));
  if (status === null || !status.exceeded) {
    return null;
  }
  return `Лимит на «${operation.category}» превышен: потрачено ${formatRubles(status.spent)} `
    + `из ${formatRubles(status.limit)}, перерасход ${formatRubles(-status.left)}`;
}

function submitOperation(event) {
  event.preventDefault();
  const { elements } = operationForm;
  const parsed = parseAmount(elements.amount.value);
  let result = parsed;
  if (parsed.ok) {
    const draft = {
      // Поле даты может вернуть время с секундами, формат операции их не хранит.
      at: elements.at.value.slice(0, 16),
      amount: parsed.value,
      kind: elements.kind.value,
      category: elements.category.value,
      note: elements.note.value,
    };
    result = editedId === null ? addOperation(state, draft) : updateOperation(state, editedId, draft);
  }
  if (!commit(result)) {
    operationError.textContent = result.error;
    operationError.hidden = false;
    telegram.haptic('error');
    return;
  }
  operationDialog.close();
  const warning = limitWarning(result.operation);
  if (warning === null) {
    telegram.haptic('success');
  } else {
    telegram.haptic('warning');
    telegram.showAlert(warning);
  }
}

async function deleteOperation() {
  const id = editedId;
  // Окно подтверждения браузера не видно под открытым диалогом в части клиентов,
  // поэтому сначала закрывается диалог.
  operationDialog.close();
  if (await telegram.showConfirm('Удалить эту операцию? Вернуть её будет нельзя.')) {
    commit(removeOperation(state, id));
  }
}

// Окно экспорта. В Telegram скачивание файла со страницы работает не во всех
// клиентах, поэтому текст всегда можно скопировать.

const exportDialog = document.getElementById('export-dialog');
const exportText = document.getElementById('export-text');
let exportFile = { name: '', type: '' };

function openExportDialog(name, type, text) {
  exportFile = { name, type };
  document.getElementById('export-title').textContent = `Экспорт: ${name}`;
  exportText.value = text;
  exportDialog.showModal();
}

async function copyExport() {
  try {
    await navigator.clipboard.writeText(exportText.value);
  } catch {
    // Запасной путь для клиентов без доступа к буферу обмена: выделить текст,
    // чтобы его можно было скопировать вручную.
    exportText.select();
    return;
  }
  exportDialog.close();
  showToast('Отчёт скопирован.');
}

function downloadExport() {
  const prefix = exportFile.type === 'text/csv' ? BYTE_ORDER_MARK : '';
  const blob = new Blob([prefix + exportText.value], { type: `${exportFile.type};charset=utf-8` });
  const url = URL.createObjectURL(blob);
  el('a', { href: url, download: exportFile.name }).click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function bindEvents() {
  for (const tab of document.querySelectorAll('.tab')) {
    tab.addEventListener('click', () => {
      currentView = tab.dataset.view;
      render();
      globalThis.scrollTo(0, 0);
    });
  }
  document.getElementById('add-button').addEventListener('click', () => openOperationDialog());
  operationForm.addEventListener('submit', submitOperation);
  operationForm.addEventListener('change', (event) => {
    if (event.target.name === 'kind') {
      fillCategoryOptions(event.target.value);
    }
  });
  operationDelete.addEventListener('click', deleteOperation);
  document.getElementById('operation-cancel').addEventListener('click', () => operationDialog.close());
  document.getElementById('export-copy').addEventListener('click', copyExport);
  document.getElementById('export-download').addEventListener('click', downloadExport);
  document.getElementById('export-close').addEventListener('click', () => exportDialog.close());
}

async function load() {
  try {
    const text = await loadText(telegram.store);
    if (text === null) {
      canSave = true;
      return;
    }
    const parsed = deserialize(text);
    if (parsed.ok) {
      state = parsed.state;
      canSave = true;
    } else {
      showToast(`${parsed.error} Изменения в этот раз не сохранятся.`);
    }
  } catch (error) {
    showToast(`Не удалось прочитать данные: ${error.message} Изменения в этот раз не сохранятся.`);
  }
}

telegram.start(BACKGROUND_COLOR);
bindEvents();
await load();
render();
