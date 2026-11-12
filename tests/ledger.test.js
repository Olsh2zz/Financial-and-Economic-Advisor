// Файл: ledger.test.js
// Назначение: тесты учёта операций и категорий.

import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  addCategory, addOperation, filterOperations, removeCategory, removeOperation, updateOperation,
} from '../miniapp/js/core/ledger.js';
import { createDefaultState } from '../miniapp/js/core/state.js';
import { SAMPLE_ROWS, stateWith } from './helpers.js';

const DRAFT = { at: '2026-11-12T09:30', amount: 15050, kind: 'expense', category: 'Еда', note: ' обед ' };

test('добавление операции', () => {
  const initial = createDefaultState();
  const result = addOperation(initial, DRAFT);
  assert.equal(result.ok, true);
  assert.deepEqual(result.operation,
    { id: 1, at: '2026-11-12T09:30', amount: 15050, kind: 'expense', category: 'Еда', note: 'обед' });
  assert.equal(result.state.nextId, 2);
  // Исходное состояние не меняется.
  assert.equal(initial.operations.length, 0);
});

test('описание необязательно', () => {
  const result = addOperation(createDefaultState(), { ...DRAFT, note: undefined });
  assert.equal(result.operation.note, '');
});

test('операция с ошибкой в поле отклоняется', () => {
  const state = createDefaultState();
  const bad = [
    { at: '2026-02-30T10:00' },
    { at: '2026-11-12' },
    { at: '2026-11-12T24:00' },
    { amount: 0 },
    { amount: -100 },
    { amount: 10.5 },
    { kind: 'transfer' },
    { category: 'Космос' },
    // Категория дохода не подходит для расхода.
    { category: 'Зарплата' },
    { note: 'я'.repeat(201) },
  ];
  for (const patch of bad) {
    const result = addOperation(state, { ...DRAFT, ...patch });
    assert.equal(result.ok, false, JSON.stringify(patch));
    assert.ok(result.error.length > 0);
  }
});

test('правка меняет все поля, номер остаётся', () => {
  const state = stateWith(SAMPLE_ROWS);
  const result = updateOperation(state, 2,
    { at: '2026-10-04T13:00', amount: 99900, kind: 'income', category: 'Подарки', note: 'возврат' });
  assert.equal(result.ok, true);
  assert.deepEqual(result.state.operations.find((item) => item.id === 2),
    { id: 2, at: '2026-10-04T13:00', amount: 99900, kind: 'income', category: 'Подарки', note: 'возврат' });
  assert.equal(result.state.operations.length, state.operations.length);
  assert.equal(updateOperation(state, 999, DRAFT).ok, false);
  assert.equal(updateOperation(state, 2, { ...DRAFT, amount: 0 }).ok, false);
});

test('удаление операции', () => {
  const state = stateWith(SAMPLE_ROWS);
  const result = removeOperation(state, 3);
  assert.equal(result.ok, true);
  assert.deepEqual(result.state.operations.map((item) => item.id), [1, 2, 4, 5]);
  assert.equal(removeOperation(state, 999).ok, false);
  // Номер удалённой операции заново не выдаётся.
  assert.equal(addOperation(result.state, DRAFT).operation.id, 6);
});

test('фильтр по дате, категории и типу', () => {
  const { operations } = stateWith(SAMPLE_ROWS);
  const ids = (filter) => filterOperations(operations, filter).map((item) => item.id);
  assert.deepEqual(ids(), [5, 4, 3, 2, 1]);
  assert.deepEqual(ids({ from: '2026-10-10' }), [5, 4, 3]);
  assert.deepEqual(ids({ to: '2026-10-10' }), [3, 2, 1]);
  // Границы периода входят в него.
  assert.deepEqual(ids({ from: '2026-10-03', to: '2026-10-12' }), [4, 3, 2]);
  assert.deepEqual(ids({ category: 'Еда' }), [3, 2]);
  assert.deepEqual(ids({ kind: 'income' }), [1]);
  assert.deepEqual(ids({ kind: 'expense', category: 'Еда', from: '2026-10-05' }), [3]);
  assert.deepEqual(ids({ from: '2027-01-01' }), []);
});

test('категории добавляются и удаляются', () => {
  let state = createDefaultState();
  const added = addCategory(state, 'expense', '  Кофе ');
  assert.equal(added.ok, true);
  assert.ok(added.state.categories.expense.includes('Кофе'));
  state = added.state;

  assert.equal(addCategory(state, 'expense', 'кофе').ok, false, 'повтор без учёта регистра');
  assert.equal(addCategory(state, 'expense', '   ').ok, false);
  assert.equal(addCategory(state, 'expense', 'я'.repeat(31)).ok, false);
  assert.equal(addCategory(state, 'other', 'Кофе').ok, false);

  const removed = removeCategory(state, 'expense', 'Кофе');
  assert.equal(removed.ok, true);
  assert.ok(!removed.state.categories.expense.includes('Кофе'));
  assert.equal(removeCategory(state, 'expense', 'Космос').ok, false);
});

test('категорию с операциями удалить нельзя', () => {
  const state = stateWith(SAMPLE_ROWS);
  const result = removeCategory(state, 'expense', 'Еда');
  assert.equal(result.ok, false);
  assert.match(result.error, /есть операции \(2\)/);
});

test('удаление категории снимает её лимит, последнюю категорию удалить нельзя', () => {
  const state = { ...createDefaultState(), limits: { Жильё: 3000000 } };
  assert.deepEqual(removeCategory(state, 'expense', 'Жильё').state.limits, {});
  const single = { ...state, limits: {}, categories: { income: ['Зарплата'], expense: ['Еда'] } };
  assert.equal(removeCategory(single, 'income', 'Зарплата').ok, false);
});
