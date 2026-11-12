// Файл: budget.hpp
// Назначение: финансовое планирование: месячные лимиты по категориям расходов,
// контроль превышения и прогноз накоплений.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpay/model.hpp"
#include "xpay/result.hpp"

namespace xpay {

/// Задаёт месячный лимит для категории расходов. Сумма больше нуля.
Status set_limit(State& state, const std::string& category, Money amount);

/// Снимает лимит. Возвращает false, если лимита не было.
bool clear_limit(State& state, const std::string& category);

/// Расходы по категории за месяц. Месяц задаётся строкой «ГГГГ-ММ».
Money month_spent(const std::vector<Operation>& operations, std::string_view category, std::string_view month);

struct LimitStatus {
    Money limit = 0;
    Money spent = 0;
    Money left = 0;         ///< Отрицательный, когда лимит превышен.
    bool exceeded = false;  ///< Потратить ровно лимит ещё не превышение.
};

/// Состояние лимита категории за месяц «ГГГГ-ММ». Пусто, если лимит не задан.
std::optional<LimitStatus> check_limit(const State& state, const std::string& category, std::string_view month);

struct Projection {
    int months = 0;
    Money amount = 0;
};

struct Forecast {
    int months = 0;  ///< Сколько календарных месяцев истории учтено.
    Money average_income = 0;
    Money average_expense = 0;
    Money potential = 0;  ///< Сберегаемый потенциал: средний доход минус средний расход.
    std::vector<Projection> projections;  ///< Накопления за 3, 6 и 12 месяцев.
};

/// Прогноз по средним за все календарные месяцы от первой операции до последней.
/// Месяц без операций тоже входит в счёт. Пусто, если операций нет.
std::optional<Forecast> forecast(const std::vector<Operation>& operations);

}  // namespace xpay
