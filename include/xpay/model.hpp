// Файл: model.hpp
// Назначение: данные приложения: операция, категории, лимиты и состояние целиком.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "xpay/money.hpp"

namespace xpay {

enum class Kind { Income, Expense };

/// Название типа для файла данных: «income» или «expense».
std::string_view kind_code(Kind kind) noexcept;

/// Название типа для пользователя: «Доход» или «Расход».
std::string_view kind_title(Kind kind) noexcept;

inline constexpr std::size_t kMaxCategoryLength = 30;
inline constexpr std::size_t kMaxNoteLength = 200;

struct Operation {
    int id = 0;
    std::string at;        ///< Момент «ГГГГ-ММ-ДДTЧЧ:ММ».
    Money amount = 0;      ///< Всегда больше нуля; знак задаёт kind.
    Kind kind = Kind::Expense;
    std::string category;
    std::string note;      ///< Может быть пустым.
};

struct State {
    int next_id = 1;
    std::vector<std::string> income_categories;
    std::vector<std::string> expense_categories;
    /// Месячные лимиты расходов: категория -> копейки.
    std::map<std::string, Money> limits;
    std::vector<Operation> operations;

    [[nodiscard]] const std::vector<std::string>& categories(Kind kind) const noexcept;
    /// Есть ли категория с точно таким названием среди категорий типа kind.
    [[nodiscard]] bool has_category(Kind kind, std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string>& categories(Kind kind) noexcept;
};

/// Состояние первого запуска: предопределённые категории, операций нет.
State default_state();

}  // namespace xpay
