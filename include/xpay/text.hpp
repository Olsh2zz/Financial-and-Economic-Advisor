// Файл: text.hpp
// Назначение: работа со строками в UTF-8: обрезка пробелов, длина в символах,
// нижний регистр для кириллицы, выравнивание столбцов таблицы.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace xpay {

/// Убирает пробелы, табуляции и переводы строк по краям.
std::string trim(std::string_view text);

/// Число символов (кодовых точек), а не байтов. Нужно для ширины столбцов:
/// русская буква занимает два байта, но одну позицию на экране.
std::size_t utf8_length(std::string_view text) noexcept;

/// Нижний регистр для латиницы и русских букв. Остальные символы не меняются.
std::string to_lower(std::string_view text);

/// Целое неотрицательное число из строки, в которой только цифры.
/// Пусто, если строка пустая, длиннее max_digits или содержит другой символ.
/// max_digits не больше 9, чтобы результат поместился в int.
std::optional<int> parse_digits(std::string_view text, std::size_t max_digits) noexcept;

/// Дополняет строку пробелами справа или слева до ширины width в символах.
std::string pad_right(std::string_view text, std::size_t width);
std::string pad_left(std::string_view text, std::size_t width);

}  // namespace xpay
