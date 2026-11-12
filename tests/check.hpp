// Файл: check.hpp
// Назначение: проверки для тестов без сторонних библиотек: макросы CHECK
// и CHECK_EQ считают провалы, по ним main возвращает код завершения.

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xpay/ledger.hpp"
#include "xpay/model.hpp"

namespace test {

inline int& failures() {
    static int count = 0;
    return count;
}

inline int& checks() {
    static int count = 0;
    return count;
}

template <typename T>
std::string show(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string show(bool value) { return value ? "true" : "false"; }

template <typename Left, typename Right>
void check_equal(const Left& left, const Right& right, const char* expression, const char* file, int line) {
    ++checks();
    if (!(left == right)) {
        ++failures();
        std::cerr << file << ":" << line << ": " << expression << "\n  получено: " << show(left)
                  << "\n  ожидалось: " << show(right) << "\n";
    }
}

inline void check_true(bool condition, const char* expression, const char* file, int line) {
    ++checks();
    if (!condition) {
        ++failures();
        std::cerr << file << ":" << line << ": не выполнено: " << expression << "\n";
    }
}

/// Одна строка набора: момент, тип, категория, сумма в рублях, описание.
struct Row {
    const char* at;
    xpay::Kind kind;
    const char* category;
    long long rubles;
    const char* note = "";
};

/// Состояние первого запуска с добавленными операциями.
inline xpay::State state_with(const std::vector<Row>& rows) {
    xpay::State state = xpay::default_state();
    for (const Row& row : rows) {
        const auto added = xpay::add_operation(state,
            xpay::Draft{row.at, row.rubles * 100, row.kind, row.category, row.note});
        check_true(added.ok(), "операция из набора добавляется", __FILE__, __LINE__);
    }
    return state;
}

/// Набор по образцу из задания: расходы 12500 руб., из них еда 5000 (40%)
/// и транспорт 2500 (20%).
inline std::vector<Row> sample_rows() {
    using xpay::Kind;
    return {
        {"2026-10-01T10:00", Kind::Income, "Зарплата", 60000},
        {"2026-10-03T12:30", Kind::Expense, "Еда", 3000, "продукты на неделю"},
        {"2026-10-10T19:00", Kind::Expense, "Еда", 2000},
        {"2026-10-12T08:15", Kind::Expense, "Транспорт", 2500, "проездной"},
        {"2026-10-20T21:00", Kind::Expense, "Развлечения", 5000},
    };
}

}  // namespace test

#define CHECK(condition) test::check_true((condition), #condition, __FILE__, __LINE__)
#define CHECK_EQ(left, right) test::check_equal((left), (right), #left " == " #right, __FILE__, __LINE__)
