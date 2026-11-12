// Файл: storage.cpp
// Назначение: перевод состояния в JSON и обратно с проверкой каждого поля.
// Работа с диском вынесена в file_store.cpp, чтобы этот файл собирался
// и для WebAssembly, где файловой системы нет.

#include "xpay/storage.hpp"

#include <algorithm>
#include <limits>
#include <set>

#include "xpay/datetime.hpp"
#include "xpay/text.hpp"

namespace xpay {

namespace {

// Список категорий: непустые строки без повторов.
bool read_categories(const Json* json, std::vector<std::string>& out) {
    const Json::Array* items = json != nullptr ? json->as_array() : nullptr;
    if (items == nullptr || items->empty()) {
        return false;
    }
    for (const Json& item : *items) {
        const std::string* name = item.as_string();
        if (name == nullptr || name->empty() || utf8_length(*name) > kMaxCategoryLength
            || std::find(out.begin(), out.end(), *name) != out.end()) {
            return false;
        }
        out.push_back(*name);
    }
    return true;
}

bool is_amount(std::optional<long long> value) noexcept {
    return value.has_value() && *value > 0 && *value <= kMaxAmount;
}

bool read_operation(const Json& json, const State& state, Operation& out) {
    const Json* id = json.find("id");
    const Json* at = json.find("at");
    const Json* amount = json.find("amount");
    const Json* kind = json.find("kind");
    const Json* category = json.find("category");
    const Json* note = json.find("note");
    if (id == nullptr || at == nullptr || amount == nullptr || kind == nullptr || category == nullptr
        || note == nullptr) {
        return false;
    }
    const auto id_value = id->as_int();
    const std::string* kind_text = kind->as_string();
    if (!id_value || *id_value <= 0 || *id_value >= std::numeric_limits<int>::max()
        || at->as_string() == nullptr || !is_valid_stamp(*at->as_string())
        || !is_amount(amount->as_int())
        || kind_text == nullptr || (*kind_text != "income" && *kind_text != "expense")
        || category->as_string() == nullptr || note->as_string() == nullptr
        || utf8_length(*note->as_string()) > kMaxNoteLength) {
        return false;
    }
    out.id = static_cast<int>(*id_value);
    out.at = *at->as_string();
    out.amount = *amount->as_int();
    out.kind = *kind_text == "income" ? Kind::Income : Kind::Expense;
    out.category = *category->as_string();
    out.note = *note->as_string();
    return state.has_category(out.kind, out.category);
}

// Лимиты: только для существующих категорий расходов, суммы больше нуля.
bool read_limits(const Json* json, State& state) {
    const Json::Object* members = json != nullptr ? json->as_object() : nullptr;
    if (members == nullptr) {
        return false;
    }
    for (const Json::Member& member : *members) {
        if (!state.has_category(Kind::Expense, member.key) || !is_amount(member.value.as_int())) {
            return false;
        }
        state.limits[member.key] = *member.value.as_int();
    }
    return true;
}

// Список операций и счётчик номеров. Возвращает текст ошибки или пустую строку.
std::string read_operations(const Json& root, State& state) {
    const Json* operations = root.find("operations");
    const Json::Array* items = operations != nullptr ? operations->as_array() : nullptr;
    if (items == nullptr) {
        return "В данных повреждён список операций.";
    }
    std::set<int> ids;
    int max_id = 0;
    for (const Json& item : *items) {
        Operation operation;
        if (!read_operation(item, state, operation)) {
            return "В данных повреждён список операций.";
        }
        if (!ids.insert(operation.id).second) {
            return "В данных повторяются номера операций.";
        }
        max_id = std::max(max_id, operation.id);
        state.operations.push_back(std::move(operation));
    }

    // Следующий номер должен быть больше всех выданных, иначе новая операция
    // получит номер существующей.
    const Json* next_id = root.find("nextId");
    const auto next_value = next_id != nullptr ? next_id->as_int() : std::nullopt;
    if (!next_value || *next_value <= max_id || *next_value >= std::numeric_limits<int>::max()) {
        return "В данных повреждён счётчик номеров операций.";
    }
    state.next_id = static_cast<int>(*next_value);
    return {};
}

}  // namespace

Json to_json(const std::vector<std::string>& names) {
    Json list = Json::array();
    for (const std::string& name : names) {
        list.push(Json(name));
    }
    return list;
}

Json to_json(const Operation& item) {
    Json result = Json::object();
    result.set("id", Json(item.id));
    result.set("at", Json(item.at));
    result.set("amount", Json(item.amount));
    result.set("kind", Json(kind_code(item.kind)));
    result.set("category", Json(item.category));
    result.set("note", Json(item.note));
    return result;
}

std::string serialize(const State& state, bool pretty) {
    Json categories = Json::object();
    categories.set("income", to_json(state.income_categories));
    categories.set("expense", to_json(state.expense_categories));

    Json limits = Json::object();
    for (const auto& [category, amount] : state.limits) {
        limits.set(category, Json(amount));
    }

    Json operations = Json::array();
    for (const Operation& item : state.operations) {
        operations.push(to_json(item));
    }

    Json root = Json::object();
    root.set("version", Json(kFormatVersion));
    root.set("nextId", Json(state.next_id));
    root.set("categories", std::move(categories));
    root.set("limits", std::move(limits));
    root.set("operations", std::move(operations));
    return root.dump(pretty ? 2 : 0);
}

Result<State> deserialize(std::string_view text) {
    using R = Result<State>;
    const Result<Json> parsed = Json::parse(text);
    if (!parsed.ok()) {
        return R::failure("Данные не читаются. " + parsed.error);
    }
    const Json& root = *parsed.value;
    const Json* version = root.find("version");
    if (version == nullptr || version->as_int() != kFormatVersion) {
        return R::failure("Данные записаны в неизвестной версии формата.");
    }

    State state;
    const Json* categories = root.find("categories");
    if (categories == nullptr || !read_categories(categories->find("income"), state.income_categories)
        || !read_categories(categories->find("expense"), state.expense_categories)) {
        return R::failure("В данных повреждён список категорий.");
    }

    if (!read_limits(root.find("limits"), state)) {
        return R::failure("В данных повреждены лимиты.");
    }
    if (std::string error = read_operations(root, state); !error.empty()) {
        return R::failure(std::move(error));
    }
    return R::success(std::move(state));
}

}  // namespace xpay
