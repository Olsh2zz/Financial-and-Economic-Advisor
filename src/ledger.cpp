// Файл: ledger.cpp
// Назначение: реализация учёта операций и категорий.

#include "xpay/ledger.hpp"

#include <algorithm>

#include "xpay/datetime.hpp"
#include "xpay/text.hpp"

namespace xpay {

namespace {

bool contains(const std::vector<std::string>& list, std::string_view name) {
    return std::find(list.begin(), list.end(), name) != list.end();
}

// Проверяет поля и возвращает их в виде операции с номером id.
Result<Operation> checked(const State& state, int id, const Draft& draft) {
    using R = Result<Operation>;
    if (!is_valid_stamp(draft.at)) {
        return R::failure("Укажите дату и время операции.");
    }
    if (draft.amount <= 0 || draft.amount > kMaxAmount) {
        return R::failure("Сумма должна быть больше нуля и не больше 999 999 999,99 руб.");
    }
    if (!contains(state.categories(draft.kind), draft.category)) {
        return R::failure("Выберите категорию из списка.");
    }
    std::string note = trim(draft.note);
    if (utf8_length(note) > kMaxNoteLength) {
        return R::failure("Описание не длиннее " + std::to_string(kMaxNoteLength) + " символов.");
    }
    return R::success(Operation{id, draft.at, draft.amount, draft.kind, draft.category, std::move(note)});
}

}  // namespace

bool Filter::empty() const noexcept {
    return from.empty() && to.empty() && category.empty() && !kind.has_value();
}

Result<Operation> add_operation(State& state, const Draft& draft) {
    Result<Operation> result = checked(state, state.next_id, draft);
    if (result.ok()) {
        state.operations.push_back(*result.value);
        ++state.next_id;
    }
    return result;
}

Result<Operation> update_operation(State& state, int id, const Draft& draft) {
    const auto found = std::find_if(state.operations.begin(), state.operations.end(),
        [id](const Operation& item) { return item.id == id; });
    if (found == state.operations.end()) {
        return Result<Operation>::failure("Операции с таким номером нет.");
    }
    Result<Operation> result = checked(state, id, draft);
    if (result.ok()) {
        *found = *result.value;
    }
    return result;
}

Status remove_operation(State& state, int id) {
    const auto found = std::find_if(state.operations.begin(), state.operations.end(),
        [id](const Operation& item) { return item.id == id; });
    if (found == state.operations.end()) {
        return {"Операции с таким номером нет."};
    }
    state.operations.erase(found);
    return {};
}

const Operation* find_operation(const State& state, int id) noexcept {
    for (const Operation& item : state.operations) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

std::vector<Operation> filter_operations(const std::vector<Operation>& operations, const Filter& filter) {
    std::vector<Operation> result;
    for (const Operation& item : operations) {
        const std::string_view day = std::string_view(item.at).substr(0, 10);
        const bool matches = (filter.from.empty() || day >= filter.from)
            && (filter.to.empty() || day <= filter.to)
            && (filter.category.empty() || item.category == filter.category)
            && (!filter.kind.has_value() || item.kind == *filter.kind);
        if (matches) {
            result.push_back(item);
        }
    }
    std::sort(result.begin(), result.end(), [](const Operation& left, const Operation& right) {
        return left.at != right.at ? left.at > right.at : left.id > right.id;
    });
    return result;
}

Status add_category(State& state, Kind kind, std::string_view raw_name) {
    const std::string name = trim(raw_name);
    if (name.empty()) {
        return {"Введите название категории."};
    }
    if (utf8_length(name) > kMaxCategoryLength) {
        return {"Название категории не длиннее " + std::to_string(kMaxCategoryLength) + " символов."};
    }
    const std::string lowered = to_lower(name);
    for (const std::string& existing : state.categories(kind)) {
        if (to_lower(existing) == lowered) {
            return {"Категория «" + existing + "» уже есть."};
        }
    }
    state.categories(kind).push_back(name);
    return {};
}

Status remove_category(State& state, Kind kind, const std::string& name) {
    std::vector<std::string>& list = state.categories(kind);
    const auto found = std::find(list.begin(), list.end(), name);
    if (found == list.end()) {
        return {"Такой категории нет."};
    }
    const auto used = std::count_if(state.operations.begin(), state.operations.end(),
        [&](const Operation& item) { return item.kind == kind && item.category == name; });
    if (used > 0) {
        return {"Категорию «" + name + "» нельзя удалить: в ней есть операции (" + std::to_string(used) + ")."};
    }
    if (list.size() == 1) {
        return {"Нельзя удалить последнюю категорию."};
    }
    if (kind == Kind::Expense) {
        state.limits.erase(name);
    }
    list.erase(found);
    return {};
}

}  // namespace xpay
