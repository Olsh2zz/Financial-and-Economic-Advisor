// Файл: report.cpp
// Назначение: реализация статистики и экспорта отчёта.

#include "xpay/report.hpp"

#include <algorithm>
#include <cstddef>
#include <map>

#include "xpay/datetime.hpp"

namespace xpay {

namespace {

// Масштаб шкалы взят из образца в задании: 40% это 7 решёток, 20% это 4.
constexpr int kBarCells = 7;
constexpr int kBarPercent = 40;

constexpr char kCsvSeparator = ';';
constexpr const char* kCsvLineEnd = "\r\n";

std::string period_title(const Filter& period) {
    if (period.from.empty() && period.to.empty()) {
        return "за всё время";
    }
    return (period.from.empty() ? std::string("с начала") : "с " + display_date(period.from)) + " "
        + (period.to.empty() ? std::string("по сегодня") : "по " + display_date(period.to));
}

// «2026-11-12T09:30» -> «12.11.2026 09:30».
std::string display_stamp(const std::string& stamp) {
    return display_date(std::string_view(stamp).substr(0, 10)) + " " + stamp.substr(11);
}

// Поле CSV берётся в кавычки, если в нём есть разделитель, кавычка или перевод строки.
std::string csv_field(const std::string& value) {
    if (value.find_first_of(";\"\r\n") == std::string::npos) {
        return value;
    }
    std::string quoted = "\"";
    for (const char symbol : value) {
        if (symbol == '"') {
            quoted.push_back('"');
        }
        quoted.push_back(symbol);
    }
    quoted.push_back('"');
    return quoted;
}

std::string csv_row(const std::vector<std::string>& fields) {
    std::string row;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            row.push_back(kCsvSeparator);
        }
        row += csv_field(fields[i]);
    }
    return row + kCsvLineEnd;
}

}  // namespace

Totals totals(const std::vector<Operation>& operations) {
    Totals result;
    for (const Operation& item : operations) {
        (item.kind == Kind::Income ? result.income : result.expense) += item.amount;
    }
    result.balance = result.income - result.expense;
    return result;
}

std::vector<Share> expense_shares(const std::vector<Operation>& operations) {
    std::map<std::string, Money> by_category;
    Money total = 0;
    for (const Operation& item : operations) {
        if (item.kind == Kind::Expense) {
            by_category[item.category] += item.amount;
            total += item.amount;
        }
    }
    std::vector<Share> shares;
    for (const auto& [category, amount] : by_category) {
        // Прибавка половины делителя даёт округление до ближайшего целого.
        shares.push_back({category, amount, static_cast<int>((amount * 100 + total / 2) / total)});
    }
    // stable_sort сохраняет для равных сумм порядок по названию, в котором их отдал map.
    std::stable_sort(shares.begin(), shares.end(),
        [](const Share& left, const Share& right) { return left.amount > right.amount; });
    return shares;
}

std::string bar_line(const Share& share) {
    const int cells = (share.percent * kBarCells + kBarPercent / 2) / kBarPercent;
    return share.category + ": [" + std::string(static_cast<std::size_t>(cells), '#') + "] "
        + std::to_string(share.percent) + "% (" + format_rubles(share.amount) + ")";
}

std::string build_summary(const std::vector<Operation>& operations, const Filter& period) {
    const Totals sums = totals(operations);
    std::string text = "Финансовый отчёт " + period_title(period) + "\n\n";
    text += "Доходы:  " + format_rubles(sums.income) + "\n";
    text += "Расходы: " + format_rubles(sums.expense) + "\n";
    text += "Баланс:  " + format_rubles(sums.balance) + "\n\n";
    text += "Расходы по категориям:\n";
    const std::vector<Share> shares = expense_shares(operations);
    if (shares.empty()) {
        text += "расходов нет\n";
    }
    for (const Share& share : shares) {
        text += bar_line(share) + "\n";
    }
    return text;
}

std::string build_text_report(const std::vector<Operation>& operations, const Filter& period) {
    std::string text = build_summary(operations, period);
    text += "\nОперации (" + std::to_string(operations.size()) + "):\n";
    for (const Operation& item : operations) {
        text += display_stamp(item.at) + "  " + (item.kind == Kind::Income ? "+" : "-") + format_amount(item.amount)
            + "  " + item.category;
        if (!item.note.empty()) {
            text += "  " + item.note;
        }
        text += "\n";
    }
    return text;
}

std::string build_csv(const std::vector<Operation>& operations) {
    const Totals sums = totals(operations);
    std::string text = csv_row({"Дата", "Время", "Тип", "Категория", "Сумма", "Описание"});
    for (const Operation& item : operations) {
        text += csv_row({display_date(std::string_view(item.at).substr(0, 10)), item.at.substr(11),
            std::string(kind_title(item.kind)), item.category, format_plain(item.amount), item.note});
    }
    text += kCsvLineEnd;
    text += csv_row({"Доходы", "", "", "", format_plain(sums.income), ""});
    text += csv_row({"Расходы", "", "", "", format_plain(sums.expense), ""});
    text += csv_row({"Баланс", "", "", "", format_plain(sums.balance), ""});
    return text;
}

}  // namespace xpay
