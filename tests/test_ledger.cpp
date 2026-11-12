// Файл: test_ledger.cpp
// Назначение: тесты учёта операций и категорий.

#include <string>
#include <vector>

#include "check.hpp"
#include "xpay/ledger.hpp"

using namespace xpay;

namespace {

std::string ids(const std::vector<Operation>& operations) {
    std::string result;
    for (const Operation& item : operations) {
        result += (result.empty() ? "" : " ") + std::to_string(item.id);
    }
    return result;
}

std::string repeat(const std::string& piece, std::size_t count) {
    std::string result;
    for (std::size_t i = 0; i < count; ++i) {
        result += piece;
    }
    return result;
}

Draft draft() {
    return Draft{"2026-11-12T09:30", 15050, Kind::Expense, "Еда", " обед "};
}

}  // namespace

void test_ledger() {
    // Добавление.
    {
        State state = default_state();
        const auto added = add_operation(state, draft());
        CHECK(added.ok());
        CHECK_EQ(added.value->id, 1);
        CHECK_EQ(added.value->note, std::string("обед"));
        CHECK_EQ(state.next_id, 2);
        CHECK_EQ(state.operations.size(), std::size_t{1});
    }

    // Ошибка в любом поле: операция не добавляется, состояние прежнее.
    {
        State state = default_state();
        std::vector<Draft> bad(8, draft());
        bad[0].at = "2026-02-30T10:00";
        bad[1].at = "2026-11-12";
        bad[2].amount = 0;
        bad[3].amount = -100;
        bad[4].amount = kMaxAmount + 1;
        bad[5].category = "Космос";
        bad[6].category = "Зарплата";  // категория дохода не подходит расходу
        bad[7].note = repeat("я", kMaxNoteLength + 1);
        for (const Draft& item : bad) {
            const auto result = add_operation(state, item);
            CHECK(!result.ok());
            CHECK(!result.error.empty());
        }
        CHECK(state.operations.empty());
        CHECK_EQ(state.next_id, 1);

        // Описание ровно предельной длины в символах (не в байтах) проходит.
        Draft longest = draft();
        longest.note = repeat("я", kMaxNoteLength);
        CHECK(add_operation(state, longest).ok());
    }

    // Правка меняет все поля, номер остаётся.
    {
        State state = test::state_with(test::sample_rows());
        const auto updated = update_operation(state, 2,
            Draft{"2026-10-04T13:00", 99900, Kind::Income, "Подарки", "возврат"});
        CHECK(updated.ok());
        const Operation* item = find_operation(state, 2);
        CHECK(item != nullptr);
        CHECK_EQ(item->at, std::string("2026-10-04T13:00"));
        CHECK_EQ(item->amount, 99900);
        CHECK(item->kind == Kind::Income);
        CHECK_EQ(item->category, std::string("Подарки"));
        CHECK_EQ(item->note, std::string("возврат"));
        CHECK_EQ(state.operations.size(), std::size_t{5});

        CHECK(!update_operation(state, 999, draft()).ok());
        Draft broken = draft();
        broken.amount = 0;
        CHECK(!update_operation(state, 3, broken).ok());
        CHECK_EQ(find_operation(state, 3)->amount, 200000);
    }

    // Удаление. Номер удалённой операции заново не выдаётся.
    {
        State state = test::state_with(test::sample_rows());
        CHECK(remove_operation(state, 3).ok());
        CHECK_EQ(ids(state.operations), std::string("1 2 4 5"));
        CHECK(!remove_operation(state, 3).ok());
        CHECK(find_operation(state, 3) == nullptr);
        CHECK_EQ(add_operation(state, draft()).value->id, 6);
    }

    // Фильтр: от новых к старым, границы периода входят в него.
    {
        const State state = test::state_with(test::sample_rows());
        const auto pick = [&](const Filter& filter) { return ids(filter_operations(state.operations, filter)); };
        CHECK_EQ(pick({}), std::string("5 4 3 2 1"));
        CHECK_EQ(pick({"2026-10-10", "", "", {}}), std::string("5 4 3"));
        CHECK_EQ(pick({"", "2026-10-10", "", {}}), std::string("3 2 1"));
        CHECK_EQ(pick({"2026-10-03", "2026-10-12", "", {}}), std::string("4 3 2"));
        CHECK_EQ(pick({"", "", "Еда", {}}), std::string("3 2"));
        CHECK_EQ(pick({"", "", "", Kind::Income}), std::string("1"));
        CHECK_EQ(pick({"2026-10-05", "", "Еда", Kind::Expense}), std::string("3"));
        CHECK_EQ(pick({"2027-01-01", "", "", {}}), std::string(""));
        CHECK(Filter{}.empty());
        CHECK(!(Filter{"", "", "", Kind::Income}.empty()));
    }

    // Категории.
    {
        State state = default_state();
        CHECK(add_category(state, Kind::Expense, "  Кофе ").ok());
        CHECK_EQ(state.expense_categories.back(), std::string("Кофе"));
        CHECK(!add_category(state, Kind::Expense, "КОФЕ").ok());  // повтор без учёта регистра
        CHECK(!add_category(state, Kind::Expense, "   ").ok());
        CHECK(!add_category(state, Kind::Expense, "Очень длинное название категории расходов").ok());
        // То же название в другом типе допустимо.
        CHECK(add_category(state, Kind::Income, "Кофе").ok());

        CHECK(remove_category(state, Kind::Expense, "Кофе").ok());
        CHECK(!remove_category(state, Kind::Expense, "Кофе").ok());
    }
    {
        State state = test::state_with(test::sample_rows());
        const Status status = remove_category(state, Kind::Expense, "Еда");
        CHECK(!status.ok());
        CHECK(status.error.find("есть операции (2)") != std::string::npos);

        state.limits["Жильё"] = 3000000;
        CHECK(remove_category(state, Kind::Expense, "Жильё").ok());
        CHECK(state.limits.empty());

        State single = default_state();
        single.income_categories = {"Зарплата"};
        CHECK(!remove_category(single, Kind::Income, "Зарплата").ok());
    }
}
