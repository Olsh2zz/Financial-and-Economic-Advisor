// Файл: json.hpp
// Назначение: чтение и запись JSON в объёме, который нужен файлу данных:
// объекты, массивы, строки, целые числа, true, false и null.
// Дробные числа не поддерживаются: суммы хранятся целыми копейками.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "xpay/result.hpp"

namespace xpay {

class Json {
public:
    struct Member;
    using Array = std::vector<Json>;
    /// Поля объекта хранятся в порядке добавления, чтобы файл данных
    /// при каждом сохранении выглядел одинаково.
    using Object = std::vector<Member>;

    Json() noexcept;
    Json(bool value) noexcept;
    Json(int value) noexcept;
    Json(long long value) noexcept;
    Json(const char* value);
    Json(std::string value) noexcept;
    Json(std::string_view value);
    Json(Array value) noexcept;
    Json(Object value) noexcept;

    static Json array();
    static Json object();

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] std::optional<bool> as_bool() const noexcept;
    [[nodiscard]] std::optional<long long> as_int() const noexcept;
    /// Возвращают nullptr, если значение другого типа.
    [[nodiscard]] const std::string* as_string() const noexcept;
    [[nodiscard]] const Array* as_array() const noexcept;
    [[nodiscard]] const Object* as_object() const noexcept;

    /// Поле объекта по имени. nullptr, если это не объект или поля нет.
    [[nodiscard]] const Json* find(std::string_view key) const noexcept;

    /// Добавляет поле в объект. Вызывать только для объекта.
    Json& set(std::string key, Json value);
    /// Добавляет элемент в массив. Вызывать только для массива.
    Json& push(Json value);

    /// Записывает значение текстом. indent задаёт отступ в пробелах,
    /// при нуле текст идёт одной строкой.
    [[nodiscard]] std::string dump(int indent = 0) const;

    /// Разбирает текст. Вложенность глубже kMaxDepth считается ошибкой,
    /// чтобы испорченный файл не переполнил стек.
    static Result<Json> parse(std::string_view text);

    static constexpr int kMaxDepth = 32;

private:
    void write(std::string& out, int indent, int level) const;

    std::variant<std::monostate, bool, long long, std::string, Array, Object> data_;
};

struct Json::Member {
    std::string key;
    Json value;
};

}  // namespace xpay
