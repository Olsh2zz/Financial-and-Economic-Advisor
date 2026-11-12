// Файл: ledger.hpp
// Назначение: учёт операций и категорий: добавление, изменение, удаление, отбор.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpay/model.hpp"
#include "xpay/result.hpp"

namespace xpay {

/// Поля операции до присвоения номера.
struct Draft {
    std::string at;
    Money amount = 0;
    Kind kind = Kind::Expense;
    std::string category;
    std::string note;
};

/// Условия отбора. Пустая строка и пустой kind означают «без ограничения».
/// from и to задают даты «ГГГГ-ММ-ДД», обе границы входят в период.
struct Filter {
    std::string from;
    std::string to;
    std::string category;
    std::optional<Kind> kind;

    [[nodiscard]] bool empty() const noexcept;
};

/// Добавляет операцию и возвращает её с присвоенным номером.
/// При ошибке в полях состояние не меняется.
Result<Operation> add_operation(State& state, const Draft& draft);

/// Заменяет все поля операции с номером id. Сам номер остаётся прежним.
Result<Operation> update_operation(State& state, int id, const Draft& draft);

Status remove_operation(State& state, int id);

/// Возвращает nullptr, если операции с таким номером нет.
const Operation* find_operation(const State& state, int id) noexcept;

/// Отбирает операции по фильтру. Результат идёт от новых к старым.
std::vector<Operation> filter_operations(const std::vector<Operation>& operations, const Filter& filter);

/// Добавляет категорию. Название не длиннее kMaxCategoryLength символов
/// и не совпадает с существующим без учёта регистра.
Status add_category(State& state, Kind kind, std::string_view name);

/// Удаляет категорию вместе с её лимитом. Категорию, в которой есть операции,
/// и последнюю категорию своего типа удалить нельзя.
Status remove_category(State& state, Kind kind, const std::string& name);

}  // namespace xpay
