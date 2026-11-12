// Файл: result.hpp
// Назначение: типы для возврата «значение или текст ошибки» без исключений.
// Ядро не бросает исключений, поэтому оно собирается и для WebAssembly.

#pragma once

#include <optional>
#include <string>
#include <utility>

namespace xpay {

/// Итог действия без значения. Пустая строка ошибки означает успех.
struct Status {
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

/// Значение или текст ошибки для пользователя.
template <typename T>
struct Result {
    std::optional<T> value;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return value.has_value(); }

    static Result success(T item) { return Result{std::move(item), {}}; }
    static Result failure(std::string message) { return Result{std::nullopt, std::move(message)}; }
};

}  // namespace xpay
