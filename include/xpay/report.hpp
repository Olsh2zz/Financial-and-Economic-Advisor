// Файл: report.hpp
// Назначение: статистика за период, доли расходов по категориям
// и экспорт отчёта в текст и CSV.

#pragma once

#include <string>
#include <vector>

#include "xpay/ledger.hpp"
#include "xpay/model.hpp"

namespace xpay {

struct Totals {
    Money income = 0;
    Money expense = 0;
    Money balance = 0;  ///< Доходы минус расходы.
};

struct Share {
    std::string category;
    Money amount = 0;
    int percent = 0;  ///< Доля от всех расходов, округлённая до целого.
};

Totals totals(const std::vector<Operation>& operations);

/// Расходы по категориям от больших к меньшим. Сумма округлённых долей
/// может отличаться от 100 на единицу-две.
std::vector<Share> expense_shares(const std::vector<Operation>& operations);

/// Строка по образцу из задания: «Еда: [#######] 40% (5000 руб.)».
std::string bar_line(const Share& share);

/// Сводка за период: итоги и расходы по категориям со шкалой.
/// operations должны быть уже отобраны за период period.
std::string build_summary(const std::vector<Operation>& operations, const Filter& period);

/// Текстовый отчёт: сводка и список операций.
std::string build_text_report(const std::vector<Operation>& operations, const Filter& period);

/// CSV с разделителем «;» и запятой в суммах: такой файл без настройки
/// открывается в русском Excel. После операций идут строки с итогами.
std::string build_csv(const std::vector<Operation>& operations);

}  // namespace xpay
