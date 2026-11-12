// Файл: budget.cpp
// Назначение: реализация лимитов и прогноза накоплений.

#include "xpay/budget.hpp"

#include <algorithm>

#include "xpay/datetime.hpp"

namespace xpay {

namespace {

// Сроки прогноза накоплений в месяцах.
constexpr int kHorizons[] = {3, 6, 12};

}  // namespace

Status set_limit(State& state, const std::string& category, Money amount) {
    if (!state.has_category(Kind::Expense, category)) {
        return {"Лимит задаётся только для категории расходов."};
    }
    if (amount <= 0 || amount > kMaxAmount) {
        return {"Лимит должен быть больше нуля и не больше 999 999 999,99 руб."};
    }
    state.limits[category] = amount;
    return {};
}

bool clear_limit(State& state, const std::string& category) {
    return state.limits.erase(category) > 0;
}

Money month_spent(const std::vector<Operation>& operations, std::string_view category, std::string_view month) {
    Money sum = 0;
    for (const Operation& item : operations) {
        if (item.kind == Kind::Expense && item.category == category
            && std::string_view(item.at).substr(0, month.size()) == month) {
            sum += item.amount;
        }
    }
    return sum;
}

std::optional<LimitStatus> check_limit(const State& state, const std::string& category, std::string_view month) {
    const auto found = state.limits.find(category);
    if (found == state.limits.end()) {
        return std::nullopt;
    }
    const Money spent = month_spent(state.operations, category, month);
    return LimitStatus{found->second, spent, found->second - spent, spent > found->second};
}

std::optional<Forecast> forecast(const std::vector<Operation>& operations) {
    if (operations.empty()) {
        return std::nullopt;
    }
    int first = month_index(operations.front().at);
    int last = first;
    Money income = 0;
    Money expense = 0;
    for (const Operation& item : operations) {
        first = std::min(first, month_index(item.at));
        last = std::max(last, month_index(item.at));
        (item.kind == Kind::Income ? income : expense) += item.amount;
    }
    Forecast result;
    result.months = last - first + 1;
    // Прибавка половины делителя даёт округление до ближайшей копейки.
    result.average_income = (income + result.months / 2) / result.months;
    result.average_expense = (expense + result.months / 2) / result.months;
    result.potential = result.average_income - result.average_expense;
    for (const int horizon : kHorizons) {
        result.projections.push_back({horizon, result.potential * horizon});
    }
    return result;
}

}  // namespace xpay
