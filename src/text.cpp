// Файл: text.cpp
// Назначение: реализация функций для строк в UTF-8.

#include "xpay/text.hpp"

namespace xpay {

namespace {

bool is_space(char symbol) noexcept {
    return symbol == ' ' || symbol == '\t' || symbol == '\r' || symbol == '\n';
}

// Байт продолжения многобайтового символа UTF-8 имеет вид 10xxxxxx.
bool is_continuation(char symbol) noexcept {
    return (static_cast<unsigned char>(symbol) & 0xC0U) == 0x80U;
}

}  // namespace

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_space(text[begin])) {
        ++begin;
    }
    while (end > begin && is_space(text[end - 1])) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::size_t utf8_length(std::string_view text) noexcept {
    std::size_t count = 0;
    for (const char symbol : text) {
        if (!is_continuation(symbol)) {
            ++count;
        }
    }
    return count;
}

std::string to_lower(std::string_view text) {
    std::string result(text);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto first = static_cast<unsigned char>(result[i]);
        if (first >= 'A' && first <= 'Z') {
            result[i] = static_cast<char>(first - 'A' + 'a');
            continue;
        }
        // Русские заглавные буквы в UTF-8 начинаются с байта 0xD0.
        if (first != 0xD0 || i + 1 >= result.size()) {
            continue;
        }
        const auto second = static_cast<unsigned char>(result[i + 1]);
        if (second >= 0x90 && second <= 0x9F) {
            // А..П (U+0410..U+041F) -> а..п (U+0430..U+043F).
            result[i + 1] = static_cast<char>(second + 0x20);
        } else if (second >= 0xA0 && second <= 0xAF) {
            // Р..Я (U+0420..U+042F) -> р..я (U+0440..U+044F), первый байт меняется на 0xD1.
            result[i] = static_cast<char>(0xD1);
            result[i + 1] = static_cast<char>(second - 0x20);
        } else if (second == 0x81) {
            // Ё (U+0401) -> ё (U+0451).
            result[i] = static_cast<char>(0xD1);
            result[i + 1] = static_cast<char>(0x91);
        }
        ++i;
    }
    return result;
}

std::optional<int> parse_digits(std::string_view text, std::size_t max_digits) noexcept {
    if (text.empty() || text.size() > max_digits) {
        return std::nullopt;
    }
    int value = 0;
    for (const char symbol : text) {
        if (symbol < '0' || symbol > '9') {
            return std::nullopt;
        }
        value = value * 10 + (symbol - '0');
    }
    return value;
}

std::string pad_right(std::string_view text, std::size_t width) {
    std::string result(text);
    const std::size_t length = utf8_length(text);
    if (length < width) {
        result.append(width - length, ' ');
    }
    return result;
}

std::string pad_left(std::string_view text, std::size_t width) {
    const std::size_t length = utf8_length(text);
    std::string result(length < width ? width - length : 0, ' ');
    result.append(text);
    return result;
}

}  // namespace xpay
