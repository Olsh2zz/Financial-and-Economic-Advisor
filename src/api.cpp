// Файл: api.cpp
// Назначение: команды ядра для мини-приложения Telegram. Каждая команда
// читает поля запроса JSON, вызывает функции учёта и собирает ответ.
//
// Команды:
//   load {data}                 заменить состояние сохранённым текстом
//   dump                        -> {data}: состояние текстом для сохранения
//   state                       -> {categories, count}
//   list {from,to,category,kind}-> {operations, totals}
//   add {amount,at,kind,category,note}        -> {operation, limit}
//   update {id, ...те же поля}                -> {operation, limit}
//   remove {id}
//   report {from,to}            -> {totals, shares, forecast}
//   export {from,to,format}     -> {text}, format равен «txt» или «csv»
//   limits {month}              -> {items}
//   setLimit {category,amount}, clearLimit {category}
//   addCategory {kind,name}, removeCategory {kind,name}
// Суммы в запросах идут текстом, как их ввёл пользователь, в ответах копейками.

#include "xpay/api.hpp"

#include <optional>

#include "xpay/budget.hpp"
#include "xpay/datetime.hpp"
#include "xpay/ledger.hpp"
#include "xpay/report.hpp"
#include "xpay/storage.hpp"

namespace xpay {

namespace {

std::string failure(const std::string& message) {
    Json response = Json::object();
    response.set("ok", Json(false));
    response.set("error", Json(message));
    return response.dump();
}

Json success() {
    Json response = Json::object();
    response.set("ok", Json(true));
    return response;
}

// Строковое поле запроса. Отсутствующее поле читается как пустая строка.
std::string text_field(const Json& request, std::string_view key) {
    const Json* value = request.find(key);
    const std::string* text = value != nullptr ? value->as_string() : nullptr;
    return text != nullptr ? *text : std::string();
}

std::optional<Kind> kind_field(const Json& request) {
    const std::string code = text_field(request, "kind");
    if (code == "income") {
        return Kind::Income;
    }
    if (code == "expense") {
        return Kind::Expense;
    }
    return std::nullopt;
}

std::optional<int> id_field(const Json& request) {
    const Json* value = request.find("id");
    const auto number = value != nullptr ? value->as_int() : std::nullopt;
    if (!number || *number <= 0 || *number > 2'000'000'000) {
        return std::nullopt;
    }
    return static_cast<int>(*number);
}

// Читает фильтр. Возвращает текст ошибки, если дата записана неверно.
std::string read_filter(const Json& request, Filter& filter) {
    filter.from = text_field(request, "from");
    filter.to = text_field(request, "to");
    filter.category = text_field(request, "category");
    filter.kind = kind_field(request);
    if ((!filter.from.empty() && !is_valid_date(filter.from)) || (!filter.to.empty() && !is_valid_date(filter.to))) {
        return "Дата фильтра записана неверно.";
    }
    if (!filter.from.empty() && !filter.to.empty() && filter.from > filter.to) {
        return "Начало периода позже его конца.";
    }
    return {};
}

Json totals_json(const Totals& sums) {
    Json result = Json::object();
    result.set("income", Json(sums.income));
    result.set("expense", Json(sums.expense));
    result.set("balance", Json(sums.balance));
    return result;
}

Json limit_json(const std::optional<LimitStatus>& status) {
    if (!status) {
        return {};
    }
    Json result = Json::object();
    result.set("limit", Json(status->limit));
    result.set("spent", Json(status->spent));
    result.set("left", Json(status->left));
    result.set("exceeded", Json(status->exceeded));
    return result;
}

// Ответ на добавление и правку: сама операция и состояние лимита её категории
// за месяц операции (null для дохода и для категории без лимита).
std::string operation_response(const State& state, const Result<Operation>& result) {
    if (!result.ok()) {
        return failure(result.error);
    }
    const Operation& item = *result.value;
    Json response = success();
    response.set("operation", to_json(item));
    response.set("limit", item.kind == Kind::Expense
        ? limit_json(check_limit(state, item.category, std::string_view(item.at).substr(0, 7)))
        : Json());
    return response.dump();
}

// Собирает черновик операции из запроса. При ошибке возвращает её текст.
std::string read_draft(const Json& request, Draft& draft) {
    const Result<Money> amount = parse_amount(text_field(request, "amount"));
    if (!amount.ok()) {
        return amount.error;
    }
    const std::optional<Kind> kind = kind_field(request);
    if (!kind) {
        return "Выберите тип операции: доход или расход.";
    }
    draft = Draft{text_field(request, "at"), *amount.value, *kind, text_field(request, "category"),
        text_field(request, "note")};
    return {};
}

std::string handle_list(State& state, const Json& request) {
    Filter filter;
    if (const std::string error = read_filter(request, filter); !error.empty()) {
        return failure(error);
    }
    const std::vector<Operation> operations = filter_operations(state.operations, filter);
    Json list = Json::array();
    for (const Operation& item : operations) {
        list.push(to_json(item));
    }
    Json response = success();
    response.set("operations", std::move(list));
    response.set("totals", totals_json(totals(operations)));
    return response.dump();
}

std::string handle_report(State& state, const Json& request) {
    Filter period;
    if (const std::string error = read_filter(request, period); !error.empty()) {
        return failure(error);
    }
    const std::vector<Operation> operations = filter_operations(state.operations, period);
    Json shares = Json::array();
    for (const Share& share : expense_shares(operations)) {
        Json item = Json::object();
        item.set("category", Json(share.category));
        item.set("amount", Json(share.amount));
        item.set("percent", Json(share.percent));
        shares.push(std::move(item));
    }
    // Прогноз считается по всей истории, а не по периоду: среднее
    // за короткий отрезок слишком зависит от случайных трат.
    Json outlook;
    if (const std::optional<Forecast> result = forecast(state.operations)) {
        Json projections = Json::array();
        for (const Projection& projection : result->projections) {
            Json item = Json::object();
            item.set("months", Json(projection.months));
            item.set("amount", Json(projection.amount));
            projections.push(std::move(item));
        }
        outlook = Json::object();
        outlook.set("months", Json(result->months));
        outlook.set("averageIncome", Json(result->average_income));
        outlook.set("averageExpense", Json(result->average_expense));
        outlook.set("potential", Json(result->potential));
        outlook.set("projections", std::move(projections));
    }
    Json response = success();
    response.set("totals", totals_json(totals(operations)));
    response.set("shares", std::move(shares));
    response.set("forecast", std::move(outlook));
    return response.dump();
}

std::string handle_export(State& state, const Json& request) {
    Filter period;
    if (const std::string error = read_filter(request, period); !error.empty()) {
        return failure(error);
    }
    const std::string format = text_field(request, "format");
    if (format != "txt" && format != "csv") {
        return failure("Формат экспорта: txt или csv.");
    }
    const std::vector<Operation> operations = filter_operations(state.operations, period);
    Json response = success();
    response.set("text", Json(format == "txt" ? build_text_report(operations, period) : build_csv(operations)));
    return response.dump();
}

std::string handle_limits(State& state, const Json& request) {
    const std::string month = text_field(request, "month");
    if (!is_valid_date(month + "-01")) {
        return failure("Месяц записывается в виде ГГГГ-ММ.");
    }
    Json items = Json::array();
    for (const std::string& category : state.expense_categories) {
        Json item = Json::object();
        item.set("category", Json(category));
        item.set("spent", Json(month_spent(state.operations, category, month)));
        item.set("status", limit_json(check_limit(state, category, month)));
        items.push(std::move(item));
    }
    Json response = success();
    response.set("items", std::move(items));
    return response.dump();
}

std::string status_response(const Status& status) {
    return status.ok() ? success().dump() : failure(status.error);
}

std::string handle_load(State& state, const Json& request) {
    Result<State> loaded = deserialize(text_field(request, "data"));
    if (!loaded.ok()) {
        return failure(loaded.error);
    }
    state = std::move(*loaded.value);
    return success().dump();
}

std::string handle_dump(State& state, const Json&) {
    Json response = success();
    response.set("data", Json(serialize(state)));
    return response.dump();
}

std::string handle_state(State& state, const Json&) {
    Json categories = Json::object();
    categories.set("income", to_json(state.income_categories));
    categories.set("expense", to_json(state.expense_categories));
    Json response = success();
    response.set("categories", std::move(categories));
    response.set("count", Json(static_cast<long long>(state.operations.size())));
    return response.dump();
}

std::string handle_add(State& state, const Json& request) {
    Draft draft;
    if (const std::string error = read_draft(request, draft); !error.empty()) {
        return failure(error);
    }
    return operation_response(state, add_operation(state, draft));
}

std::string handle_update(State& state, const Json& request) {
    Draft draft;
    if (const std::string error = read_draft(request, draft); !error.empty()) {
        return failure(error);
    }
    const std::optional<int> id = id_field(request);
    if (!id) {
        return failure("Операции с таким номером нет.");
    }
    return operation_response(state, update_operation(state, *id, draft));
}

std::string handle_remove(State& state, const Json& request) {
    const std::optional<int> id = id_field(request);
    return status_response(id ? remove_operation(state, *id) : Status{"Операции с таким номером нет."});
}

std::string handle_set_limit(State& state, const Json& request) {
    const Result<Money> amount = parse_amount(text_field(request, "amount"));
    if (!amount.ok()) {
        return failure(amount.error);
    }
    return status_response(set_limit(state, text_field(request, "category"), *amount.value));
}

std::string handle_clear_limit(State& state, const Json& request) {
    clear_limit(state, text_field(request, "category"));
    return success().dump();
}

std::string handle_add_category(State& state, const Json& request) {
    const std::optional<Kind> kind = kind_field(request);
    return kind ? status_response(add_category(state, *kind, text_field(request, "name")))
                : failure("Неизвестный тип категории.");
}

std::string handle_remove_category(State& state, const Json& request) {
    const std::optional<Kind> kind = kind_field(request);
    return kind ? status_response(remove_category(state, *kind, text_field(request, "name")))
                : failure("Неизвестный тип категории.");
}

struct Command {
    std::string_view name;
    std::string (*handler)(State&, const Json&);
};

// Таблица команд: чтобы добавить команду, нужна одна функция и одна строка здесь.
constexpr Command kCommands[] = {
    {"load", handle_load},
    {"dump", handle_dump},
    {"state", handle_state},
    {"list", handle_list},
    {"add", handle_add},
    {"update", handle_update},
    {"remove", handle_remove},
    {"report", handle_report},
    {"export", handle_export},
    {"limits", handle_limits},
    {"setLimit", handle_set_limit},
    {"clearLimit", handle_clear_limit},
    {"addCategory", handle_add_category},
    {"removeCategory", handle_remove_category},
};

}  // namespace

std::string handle_request(State& state, std::string_view request_text) {
    const Result<Json> parsed = Json::parse(request_text);
    if (!parsed.ok() || parsed.value->as_object() == nullptr) {
        return failure("Запрос должен быть объектом JSON.");
    }
    const std::string name = text_field(*parsed.value, "cmd");
    for (const Command& command : kCommands) {
        if (command.name == name) {
            return command.handler(state, *parsed.value);
        }
    }
    return failure("Неизвестная команда «" + name + "».");
}

}  // namespace xpay
