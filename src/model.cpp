// Файл: model.cpp
// Назначение: названия типов операций и состояние первого запуска.

#include "xpay/model.hpp"

#include <algorithm>

namespace xpay {

std::string_view kind_code(Kind kind) noexcept {
    return kind == Kind::Income ? "income" : "expense";
}

std::string_view kind_title(Kind kind) noexcept {
    return kind == Kind::Income ? "Доход" : "Расход";
}

const std::vector<std::string>& State::categories(Kind kind) const noexcept {
    return kind == Kind::Income ? income_categories : expense_categories;
}

std::vector<std::string>& State::categories(Kind kind) noexcept {
    return kind == Kind::Income ? income_categories : expense_categories;
}

bool State::has_category(Kind kind, std::string_view name) const noexcept {
    const std::vector<std::string>& list = categories(kind);
    return std::find(list.begin(), list.end(), name) != list.end();
}

State default_state() {
    State state;
    state.income_categories = {"Зарплата", "Подработка", "Подарки"};
    state.expense_categories = {"Еда", "Транспорт", "Жильё", "Развлечения", "Здоровье"};
    return state;
}

}  // namespace xpay
