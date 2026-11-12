// Файл: test_storage.cpp
// Назначение: тесты JSON, сохранения состояния и команд для мини-приложения.

#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "suites.hpp"
#include "xpay/api.hpp"
#include "xpay/budget.hpp"
#include "xpay/json.hpp"
#include "xpay/storage.hpp"

using namespace xpay;

namespace {

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Заменяет первое вхождение from на to: так из верного JSON получается испорченный.
std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    CHECK(position != std::string::npos);
    if (position != std::string::npos) {
        text.replace(position, from.size(), to);
    }
    return text;
}

bool same(const State& left, const State& right) {
    if (left.next_id != right.next_id || left.income_categories != right.income_categories
        || left.expense_categories != right.expense_categories || left.limits != right.limits
        || left.operations.size() != right.operations.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.operations.size(); ++i) {
        const Operation& a = left.operations[i];
        const Operation& b = right.operations[i];
        if (a.id != b.id || a.at != b.at || a.amount != b.amount || a.kind != b.kind || a.category != b.category
            || a.note != b.note) {
            return false;
        }
    }
    return true;
}

}  // namespace

void test_storage() {
    // JSON: разбор и запись.
    {
        const auto parsed = Json::parse(R"( {"a": [1, -20, true, false, null], "b": {"c": "x\ny"}, "d": ""} )");
        CHECK(parsed.ok());
        CHECK_EQ(parsed.value->dump(), std::string(R"({"a":[1,-20,true,false,null],"b":{"c":"x\ny"},"d":""})"));
        CHECK_EQ(parsed.value->find("a")->as_array()->at(1).as_int().value_or(0), -20);
        CHECK(parsed.value->find("нет") == nullptr);
        CHECK(parsed.value->find("a")->as_string() == nullptr);

        // Кириллица пишется как есть, \u и суррогатная пара читаются в UTF-8.
        CHECK_EQ(Json(std::string("Еда \"обед\"\t\\")).dump(), std::string(R"("Еда \"обед\"\t\\")"));
        CHECK_EQ(*Json::parse(R"("\u0415\u0434\u0430 \ud83c\udf72 \/")").value->as_string(), std::string("Еда 🍲 /"));
        CHECK_EQ(Json(std::string("\x01")).dump(), std::string("\"\\u0001\""));

        CHECK_EQ(Json::object().dump(2), std::string("{}"));
        Json pretty = Json::object();
        pretty.set("a", Json(1));
        pretty.set("b", Json::array().push(Json("x")));
        CHECK_EQ(pretty.dump(2), std::string("{\n  \"a\": 1,\n  \"b\": [\n    \"x\"\n  ]\n}"));

        for (const char* text : {"", "{", "[1,]", "{\"a\":}", "{\"a\" 1}", "{a:1}", "\"нет конца", "1.5", "1e3",
                 "tru", "[1] 2", "\"\\x\"", "\"\\ud83c\"", "\"\\u12G4\"", "-", "99999999999999999999",
                 "\"таб\tвнутри\""}) {
            const auto broken = Json::parse(text);
            CHECK(!broken.ok());
            CHECK(!broken.error.empty());
        }
        // Слишком глубокая вложенность не роняет программу, а даёт ошибку.
        CHECK(!Json::parse(std::string(100000, '[')).ok());
        CHECK(Json::parse(std::string(20, '[') + std::string(20, ']')).ok());
    }

    // Состояние переживает запись и чтение.
    State state = test::state_with(test::sample_rows());
    set_limit(state, "Развлечения", 1000000);
    state.operations[1].note = "кафе \"Утро\" 🍲\nвторая строка";
    {
        const auto restored = deserialize(serialize(state));
        CHECK(restored.ok());
        CHECK(same(*restored.value, state));
        CHECK(same(*deserialize(serialize(state, true)).value, state));
        CHECK(same(*deserialize(serialize(default_state())).value, default_state()));
        CHECK(contains(serialize(state), R"("version":1,"nextId":6,"categories":{"income":["Зарплата",)"));
    }

    // Испорченные данные отклоняются с пояснением.
    {
        const std::string good = serialize(state);
        const std::vector<std::string> broken = {
            "не JSON",
            "null",
            "[]",
            replaced(good, "\"version\":1", "\"version\":99"),
            replaced(good, "\"income\":[", "\"доходы\":["),
            replaced(good, "\"Зарплата\",\"Подработка\"", "\"Зарплата\",\"Зарплата\""),
            replaced(good, "\"limits\":{\"Развлечения\"", "\"limits\":{\"Космос\""),
            replaced(good, "\"Развлечения\":1000000", "\"Развлечения\":-5"),
            replaced(good, "\"amount\":6000000", "\"amount\":\"6000000\""),
            replaced(good, "\"amount\":6000000", "\"amount\":0"),
            replaced(good, "\"at\":\"2026-10-01T10:00\"", "\"at\":\"2026-02-30T10:00\""),
            replaced(good, "\"kind\":\"income\"", "\"kind\":\"transfer\""),
            replaced(good, "\"category\":\"Транспорт\"", "\"category\":\"Космос\""),
            replaced(good, "\"id\":2", "\"id\":1"),
            replaced(good, "\"nextId\":6", "\"nextId\":5"),
            replaced(good, ",\"note\":\"\"", ""),
        };
        for (const std::string& text : broken) {
            const auto result = deserialize(text);
            CHECK(!result.ok());
            CHECK(!result.error.empty());
        }
    }

    // Файл: первого запуска нет, запись и чтение, повреждённый файл.
    {
        const std::string path = "xpay_test_данные.json";
        std::remove(path.c_str());
        const LoadOutcome fresh = load_file(path);
        CHECK(!fresh.existed);
        CHECK(fresh.error.empty());
        CHECK(same(fresh.state, default_state()));

        CHECK(save_file(path, state).ok());
        const LoadOutcome loaded = load_file(path);
        CHECK(loaded.existed);
        CHECK(loaded.error.empty());
        CHECK(same(loaded.state, state));

        CHECK(write_text_file(path, "{\"version\":1").ok());
        const LoadOutcome damaged = load_file(path);
        CHECK(damaged.existed);
        CHECK(contains(damaged.error, "повреждён"));
        std::remove(path.c_str());

        CHECK(!save_file("нет_такой_папки/данные.json", state).ok());
    }
}

void test_api() {
    State state = default_state();
    const auto call = [&](const std::string& request) { return handle_request(state, request); };

    CHECK(contains(call("не JSON"), R"("ok":false)"));
    CHECK(contains(call("[]"), R"("ok":false)"));
    CHECK(contains(call(R"({"cmd":"взлететь"})"), "Неизвестная команда"));

    // Добавление: сумма идёт текстом, как её ввёл пользователь.
    CHECK_EQ(call(
        R"({"cmd":"add","amount":"1 500,50","at":"2026-11-12T09:30","kind":"expense",)"
        R"("category":"Еда","note":"обед"})"),
        std::string(
            R"({"ok":true,"operation":{"id":1,"at":"2026-11-12T09:30","amount":150050,)"
            R"("kind":"expense","category":"Еда","note":"обед"},"limit":null})"));
    CHECK(contains(call(R"({"cmd":"add","amount":"abc","at":"2026-11-12T09:30","kind":"expense","category":"Еда"})"),
        "Сумма пишется цифрами"));
    CHECK(contains(call(R"({"cmd":"add","amount":"5","at":"2026-11-12T09:30","kind":"","category":"Еда"})"),
        "Выберите тип операции"));
    CHECK(contains(call(R"({"cmd":"add","amount":"5","at":"вчера","kind":"expense","category":"Еда"})"),
        "Укажите дату и время"));
    CHECK_EQ(state.operations.size(), std::size_t{1});

    // Лимит и предупреждение о превышении в ответе на добавление.
    CHECK(contains(call(R"({"cmd":"setLimit","category":"Еда","amount":"2000"})"), R"("ok":true)"));
    CHECK(contains(call(
        R"({"cmd":"setLimit","category":"Зарплата","amount":"2000"})"), "только для категории расходов"));
    CHECK(contains(call(R"({"cmd":"setLimit","category":"Еда","amount":"ноль"})"), R"("ok":false)"));
    CHECK(contains(call(
        R"({"cmd":"add","amount":"600","at":"2026-11-13T10:00","kind":"expense",)"
        R"("category":"Еда","note":""})"),
        R"("limit":{"limit":200000,"spent":210050,"left":-10050,"exceeded":true})"));
    CHECK(contains(call(
        R"({"cmd":"add","amount":"70000","at":"2026-11-01T10:00","kind":"income",)"
        R"("category":"Зарплата","note":""})"),
        R"("limit":null)"));

    // Правка и удаление.
    CHECK(contains(call(
        R"({"cmd":"update","id":2,"amount":"100","at":"2026-11-13T10:00","kind":"expense",)"
        R"("category":"Еда","note":"кофе"})"),
        R"("limit":{"limit":200000,"spent":160050,"left":39950,"exceeded":false})"));
    CHECK(contains(call(
        R"({"cmd":"update","id":99,"amount":"100","at":"2026-11-13T10:00","kind":"expense",)"
        R"("category":"Еда"})"),
        "Операции с таким номером нет"));
    CHECK(contains(call(R"({"cmd":"remove","id":"2"})"), R"("ok":false)"));
    CHECK(contains(call(R"({"cmd":"remove","id":99})"), R"("ok":false)"));

    // Список с фильтром и итогами.
    CHECK_EQ(call(R"({"cmd":"list","kind":"expense","from":"2026-11-13"})"),
        std::string(
            R"({"ok":true,"operations":[{"id":2,"at":"2026-11-13T10:00","amount":10000,)"
            R"("kind":"expense","category":"Еда","note":"кофе"}],"totals":{"income":0,)"
            R"("expense":10000,"balance":-10000}})"));
    CHECK(contains(call(R"({"cmd":"list"})"), R"("totals":{"income":7000000,"expense":160050,"balance":6839950})"));
    CHECK(contains(call(R"({"cmd":"list","from":"13.11.2026"})"), "Дата фильтра записана неверно"));
    CHECK(contains(call(R"({"cmd":"list","from":"2026-11-13","to":"2026-11-01"})"), "Начало периода позже"));

    // Отчёт, экспорт, лимиты.
    const std::string report = call(R"({"cmd":"report"})");
    CHECK(contains(report, R"("shares":[{"category":"Еда","amount":160050,"percent":100}])"));
    CHECK(contains(report,
        R"("forecast":{"months":1,"averageIncome":7000000,"averageExpense":160050,)"
        R"("potential":6839950,)"));
    CHECK(contains(call(R"({"cmd":"report","to":"2020-01-01"})"), R"("shares":[],"forecast":{"months":1)"));
    CHECK(contains(call(R"({"cmd":"export","format":"txt"})"), "Еда: [##################] 100% (1600,50 руб.)"));
    CHECK(contains(call(R"({"cmd":"export","format":"csv"})"), R"(Дата;Время;Тип;Категория;Сумма;Описание\r\n)"));
    CHECK(contains(call(R"({"cmd":"export","format":"pdf"})"), R"("ok":false)"));
    CHECK(contains(call(R"({"cmd":"limits","month":"2026-11"})"),
        R"({"category":"Еда","spent":160050,"status":{"limit":200000,"spent":160050,"left":39950,"exceeded":false}})"));
    CHECK(contains(call(R"({"cmd":"limits","month":"2026-11"})"),
        R"({"category":"Транспорт","spent":0,"status":null})"));
    CHECK(contains(call(R"({"cmd":"limits","month":"ноябрь"})"), R"("ok":false)"));
    CHECK(contains(call(R"({"cmd":"clearLimit","category":"Еда"})"), R"("ok":true)"));
    CHECK(state.limits.empty());

    // Категории.
    CHECK(contains(call(R"({"cmd":"addCategory","kind":"expense","name":"Кофе"})"), R"("ok":true)"));
    CHECK(contains(call(R"({"cmd":"addCategory","kind":"expense","name":"кофе"})"), "уже есть"));
    CHECK(contains(call(R"({"cmd":"removeCategory","kind":"expense","name":"Еда"})"), "есть операции"));
    CHECK(contains(call(R"({"cmd":"removeCategory","kind":"expense","name":"Кофе"})"), R"("ok":true)"));
    CHECK(contains(call(R"({"cmd":"addCategory","kind":"","name":"Кофе"})"), R"("ok":false)"));
    CHECK(contains(call(R"({"cmd":"state"})"),
        R"("expense":["Еда","Транспорт","Жильё","Развлечения","Здоровье"]},"count":3)"));

    // Выгрузка и загрузка: тот же текст, что пишет консольная программа.
    const std::string dumped = call(R"({"cmd":"dump"})");
    const auto parsed = Json::parse(dumped);
    CHECK(parsed.ok());
    const std::string data = *parsed.value->find("data")->as_string();
    CHECK_EQ(data, serialize(state));

    State other = default_state();
    Json load = Json::object();
    load.set("cmd", Json("load"));
    load.set("data", Json(data));
    CHECK(contains(handle_request(other, load.dump()), R"("ok":true)"));
    CHECK_EQ(serialize(other), data);
    // Испорченные данные не заменяют состояние.
    CHECK(contains(handle_request(other, R"({"cmd":"load","data":"{}"})"), R"("ok":false)"));
    CHECK_EQ(serialize(other), data);
}
