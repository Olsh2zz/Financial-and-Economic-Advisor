// Файл: test_report.cpp
// Назначение: тесты статистики, экспорта, лимитов и прогноза.
// Строки отчёта сверяются с образцом из задания посимвольно.

#include <string>
#include <vector>

#include "check.hpp"
#include "suites.hpp"
#include "xpay/budget.hpp"
#include "xpay/report.hpp"

using namespace xpay;

void test_report() {
    const State state = test::state_with(test::sample_rows());
    const std::vector<Operation>& operations = state.operations;

    // Итоги.
    const Totals sums = totals(operations);
    CHECK_EQ(sums.income, 6000000);
    CHECK_EQ(sums.expense, 1250000);
    CHECK_EQ(sums.balance, 4750000);
    CHECK_EQ(totals({}).balance, 0);
    const Totals part = totals(filter_operations(operations, {"2026-10-02", "2026-10-12", "", {}}));
    CHECK_EQ(part.income, 0);
    CHECK_EQ(part.balance, -750000);

    // Доли по категориям.
    const std::vector<Share> shares = expense_shares(operations);
    CHECK_EQ(shares.size(), std::size_t{3});
    CHECK_EQ(shares[0].category, std::string("Еда"));
    CHECK_EQ(shares[0].percent, 40);
    CHECK_EQ(shares[1].category, std::string("Развлечения"));
    CHECK_EQ(shares[2].category, std::string("Транспорт"));
    CHECK_EQ(shares[2].amount, 250000);
    CHECK_EQ(shares[2].percent, 20);
    CHECK(expense_shares({}).empty());
    // Только доходы: делить не на что, список пуст.
    CHECK(expense_shares(filter_operations(operations, {"", "", "", Kind::Income})).empty());

    // Образец из задания. В нём после «Транспорт:» нет пробела, а после «Еда:»
    // есть; программа ставит пробел всегда, остальное совпадает посимвольно.
    CHECK_EQ(bar_line(shares[0]), std::string("Еда: [#######] 40% (5000 руб.)"));
    CHECK_EQ(bar_line(shares[2]), std::string("Транспорт: [####] 20% (2500 руб.)"));
    CHECK_EQ(bar_line({"Еда", 100, 0}), std::string("Еда: [] 0% (1 руб.)"));
    CHECK_EQ(bar_line({"Еда", 100, 100}), "Еда: [" + std::string(18, '#') + "] 100% (1 руб.)");

    // Текстовый отчёт целиком.
    const Filter period{"2026-10-01", "2026-10-31", "", {}};
    CHECK_EQ(build_text_report(filter_operations(operations, period), period), std::string(
        "Финансовый отчёт с 01.10.2026 по 31.10.2026\n"
        "\n"
        "Доходы:  60000 руб.\n"
        "Расходы: 12500 руб.\n"
        "Баланс:  47500 руб.\n"
        "\n"
        "Расходы по категориям:\n"
        "Еда: [#######] 40% (5000 руб.)\n"
        "Развлечения: [#######] 40% (5000 руб.)\n"
        "Транспорт: [####] 20% (2500 руб.)\n"
        "\n"
        "Операции (5):\n"
        "20.10.2026 21:00  -5 000,00  Развлечения\n"
        "12.10.2026 08:15  -2 500,00  Транспорт  проездной\n"
        "10.10.2026 19:00  -2 000,00  Еда\n"
        "03.10.2026 12:30  -3 000,00  Еда  продукты на неделю\n"
        "01.10.2026 10:00  +60 000,00  Зарплата\n"));

    const std::string empty_report = build_text_report({}, {});
    CHECK(empty_report.rfind("Финансовый отчёт за всё время\n", 0) == 0);
    CHECK(empty_report.find("расходов нет\n") != std::string::npos);
    CHECK(build_summary({}, {"2026-10-01", "", "", {}}).rfind("Финансовый отчёт с 01.10.2026 по сегодня\n", 0) == 0);

    // CSV.
    CHECK_EQ(build_csv(filter_operations(operations, {})), std::string(
        "Дата;Время;Тип;Категория;Сумма;Описание\r\n"
        "20.10.2026;21:00;Расход;Развлечения;5000,00;\r\n"
        "12.10.2026;08:15;Расход;Транспорт;2500,00;проездной\r\n"
        "10.10.2026;19:00;Расход;Еда;2000,00;\r\n"
        "03.10.2026;12:30;Расход;Еда;3000,00;продукты на неделю\r\n"
        "01.10.2026;10:00;Доход;Зарплата;60000,00;\r\n"
        "\r\n"
        "Доходы;;;;60000,00;\r\n"
        "Расходы;;;;12500,00;\r\n"
        "Баланс;;;;47500,00;\r\n"));
    const State quoted = test::state_with({{"2026-10-01T10:00", Kind::Expense, "Еда", 100, "кафе \"Утро\"; завтрак"}});
    CHECK(build_csv(quoted.operations).find("100,00;\"кафе \"\"Утро\"\"; завтрак\"\r\n") != std::string::npos);
}

void test_budget() {
    // Лимит задаётся только для категории расходов.
    {
        State state = default_state();
        CHECK(set_limit(state, "Развлечения", 1000000).ok());
        CHECK_EQ(state.limits.at("Развлечения"), 1000000);
        CHECK(!set_limit(state, "Зарплата", 1000000).ok());
        CHECK(!set_limit(state, "Космос", 1000000).ok());
        CHECK(!set_limit(state, "Еда", 0).ok());
        CHECK(!set_limit(state, "Еда", -1).ok());
        CHECK(clear_limit(state, "Развлечения"));
        CHECK(!clear_limit(state, "Развлечения"));
    }

    // Расход за месяц: соседние месяцы и доходы не попадают.
    std::vector<test::Row> rows = test::sample_rows();
    rows.push_back({"2026-09-30T23:59", Kind::Expense, "Еда", 700});
    rows.push_back({"2026-11-01T00:00", Kind::Expense, "Еда", 900});
    State state = test::state_with(rows);
    CHECK_EQ(month_spent(state.operations, "Еда", "2026-10"), 500000);
    CHECK_EQ(month_spent(state.operations, "Еда", "2026-09"), 70000);
    CHECK_EQ(month_spent(state.operations, "Еда", "2026-12"), 0);
    CHECK_EQ(month_spent(state.operations, "Зарплата", "2026-10"), 0);

    // Контроль лимита: не задан, не превышен, ровно на границе, превышен на копейку.
    CHECK(!check_limit(state, "Еда", "2026-10").has_value());
    set_limit(state, "Еда", 600000);
    CHECK_EQ(check_limit(state, "Еда", "2026-10")->left, 100000);
    CHECK(!check_limit(state, "Еда", "2026-10")->exceeded);
    set_limit(state, "Еда", 500000);
    CHECK(!check_limit(state, "Еда", "2026-10")->exceeded);
    set_limit(state, "Еда", 499999);
    CHECK(check_limit(state, "Еда", "2026-10")->exceeded);
    CHECK_EQ(check_limit(state, "Еда", "2026-10")->left, -1);
    // В другом месяце счёт идёт заново.
    CHECK_EQ(check_limit(state, "Еда", "2026-12")->spent, 0);

    // Прогноз по одному месяцу.
    {
        const auto result = forecast(test::state_with(test::sample_rows()).operations);
        CHECK(result.has_value());
        CHECK_EQ(result->months, 1);
        CHECK_EQ(result->average_income, 6000000);
        CHECK_EQ(result->average_expense, 1250000);
        CHECK_EQ(result->potential, 4750000);
        CHECK_EQ(result->projections.size(), std::size_t{3});
        CHECK_EQ(result->projections[0].months, 3);
        CHECK_EQ(result->projections[0].amount, 14250000);
        CHECK_EQ(result->projections[2].months, 12);
        CHECK_EQ(result->projections[2].amount, 57000000);
    }
    // Месяц без операций входит в среднее, год может смениться.
    {
        const auto result = forecast(test::state_with({
            {"2025-11-05T10:00", Kind::Income, "Зарплата", 90000},
            {"2025-11-20T10:00", Kind::Expense, "Жильё", 30000},
            {"2026-02-05T10:00", Kind::Income, "Зарплата", 90000},
            {"2026-02-20T10:00", Kind::Expense, "Жильё", 50000},
        }).operations);
        CHECK_EQ(result->months, 4);
        CHECK_EQ(result->average_income, 4500000);
        CHECK_EQ(result->average_expense, 2000000);
        CHECK_EQ(result->potential, 2500000);
    }
    // Без операций прогноза нет; перерасход даёт отрицательный потенциал.
    {
        CHECK(!forecast({}).has_value());
        const auto result = forecast(test::state_with({
            {"2026-10-01T10:00", Kind::Income, "Зарплата", 1000},
            {"2026-10-02T10:00", Kind::Expense, "Еда", 1500},
        }).operations);
        CHECK_EQ(result->potential, -50000);
        CHECK_EQ(result->projections[0].amount, -150000);
    }
}
