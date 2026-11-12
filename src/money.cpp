// Файл: money.cpp
// Назначение: разбор и вывод денежных сумм.

#include "xpay/money.hpp"

#include <cstddef>

namespace xpay {

namespace {

// Рублей в kMaxAmount девять цифр: более длинная запись заведомо больше предела.
constexpr std::size_t kMaxRubleDigits = 9;

bool all_digits(std::string_view text) noexcept {
    for (const char symbol : text) {
        if (symbol < '0' || symbol > '9') {
            return false;
        }
    }
    return !text.empty();
}

Money to_number(std::string_view digits) noexcept {
    Money value = 0;
    for (const char symbol : digits) {
        value = value * 10 + (symbol - '0');
    }
    return value;
}

struct Parts {
    bool negative;
    Money rubles;
    Money kopecks;
};

Parts split(Money amount) noexcept {
    const Money absolute = amount < 0 ? -amount : amount;
    return {amount < 0, absolute / kKopecksInRuble, absolute % kKopecksInRuble};
}

std::string two_digits(Money value) {
    return std::string(value < 10 ? "0" : "") + std::to_string(value);
}

}  // namespace

Result<Money> parse_amount(std::string_view text) {
    // Пробелы убираются, чтобы принять «1 500»; неразрывный пробел в UTF-8
    // занимает два байта: 0xC2 0xA0.
    std::string cleaned;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto symbol = static_cast<unsigned char>(text[i]);
        if (symbol == ' ' || symbol == '\t' || symbol == '\r' || symbol == '\n') {
            continue;
        }
        if (symbol == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA0) {
            ++i;
            continue;
        }
        cleaned.push_back(text[i] == ',' ? '.' : text[i]);
    }
    if (cleaned.empty()) {
        return Result<Money>::failure("Введите сумму.");
    }

    const std::size_t dot = cleaned.find('.');
    const std::string_view view(cleaned);
    std::string_view rubles = view.substr(0, dot);
    const std::string_view kopecks = dot == std::string::npos ? std::string_view() : view.substr(dot + 1);
    const bool fraction_ok = dot == std::string::npos || (all_digits(kopecks) && kopecks.size() <= 2);
    if (!all_digits(rubles) || !fraction_ok) {
        return Result<Money>::failure("Сумма пишется цифрами, после запятой не больше двух знаков.");
    }

    while (rubles.size() > 1 && rubles.front() == '0') {
        rubles.remove_prefix(1);
    }
    if (rubles.size() > kMaxRubleDigits) {
        return Result<Money>::failure("Сумма не может быть больше 999 999 999,99 руб.");
    }
    Money value = to_number(rubles) * kKopecksInRuble;
    if (kopecks.size() == 1) {
        value += to_number(kopecks) * 10;
    } else if (kopecks.size() == 2) {
        value += to_number(kopecks);
    }
    if (value <= 0) {
        return Result<Money>::failure("Сумма должна быть больше нуля.");
    }
    return Result<Money>::success(value);
}

std::string format_amount(Money kopecks) {
    const Parts parts = split(kopecks);
    const std::string digits = std::to_string(parts.rubles);
    std::string grouped;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            grouped.push_back(' ');
        }
        grouped.push_back(digits[i]);
    }
    return std::string(parts.negative ? "-" : "") + grouped + "," + two_digits(parts.kopecks);
}

std::string format_rubles(Money kopecks) {
    const Parts parts = split(kopecks);
    std::string result = std::string(parts.negative ? "-" : "") + std::to_string(parts.rubles);
    if (parts.kopecks != 0) {
        result += "," + two_digits(parts.kopecks);
    }
    return result + " руб.";
}

std::string format_plain(Money kopecks) {
    const Parts parts = split(kopecks);
    return std::string(parts.negative ? "-" : "") + std::to_string(parts.rubles) + "," + two_digits(parts.kopecks);
}

}  // namespace xpay
