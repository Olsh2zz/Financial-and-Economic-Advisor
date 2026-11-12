// Файл: model.cpp
// Назначение: названия типов операций и состояние первого запуска.

#include "xpay/model.hpp"

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

State default_state() {
    State state;
    state.income_categories = {"Зарплата", "Подработка", "Подарки"};
    state.expense_categories = {"Еда", "Транспорт", "Жильё", "Развлечения", "Здоровье"};
    return state;
}

}  // namespace xpay
