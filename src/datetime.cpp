// Файл: datetime.cpp
// Назначение: проверка и разбор дат и времени.

#include "xpay/datetime.hpp"

#include <cstddef>
#include <ctime>
#include <optional>

#include "xpay/text.hpp"

namespace xpay {

namespace {

constexpr int kMonthsInYear = 12;

// Число из строки, в которой только цифры. Пусто, если есть другой символ.
std::optional<int> to_int(std::string_view digits) noexcept {
    // Ограничение длины защищает от переполнения int.
    if (digits.empty() || digits.size() > 4) {
        return std::nullopt;
    }
    int value = 0;
    for (const char symbol : digits) {
        if (symbol < '0' || symbol > '9') {
            return std::nullopt;
        }
        value = value * 10 + (symbol - '0');
    }
    return value;
}

bool is_leap(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool is_real_date(int year, int month, int day) noexcept {
    return year >= kMinYear && year <= kMaxYear && month >= 1 && month <= kMonthsInYear
        && day >= 1 && day <= days_in_month(year, month);
}

std::string padded(int value, std::size_t width) {
    const std::string digits = std::to_string(value);
    return std::string(digits.size() < width ? width - digits.size() : 0, '0') + digits;
}

}  // namespace

int days_in_month(int year, int month) noexcept {
    constexpr int kDays[kMonthsInYear] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) {
        return 29;
    }
    return kDays[month - 1];
}

bool is_valid_date(std::string_view date) noexcept {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }
    const auto year = to_int(date.substr(0, 4));
    const auto month = to_int(date.substr(5, 2));
    const auto day = to_int(date.substr(8, 2));
    return year && month && day && is_real_date(*year, *month, *day);
}

bool is_valid_stamp(std::string_view stamp) noexcept {
    if (stamp.size() != 16 || stamp[10] != 'T' || stamp[13] != ':' || !is_valid_date(stamp.substr(0, 10))) {
        return false;
    }
    const auto hour = to_int(stamp.substr(11, 2));
    const auto minute = to_int(stamp.substr(14, 2));
    return hour && minute && *hour < 24 && *minute < 60;
}

Result<std::string> parse_date_input(std::string_view text) {
    const std::string cleaned = trim(text);
    const std::string_view view(cleaned);
    const std::size_t first = view.find('.');
    const std::size_t second = first == std::string_view::npos ? first : view.find('.', first + 1);
    if (second == std::string_view::npos) {
        return Result<std::string>::failure("Дата пишется в виде ДД.ММ.ГГГГ, например 05.11.2026.");
    }
    const auto day = to_int(view.substr(0, first));
    const auto month = to_int(view.substr(first + 1, second - first - 1));
    const auto year = to_int(view.substr(second + 1));
    if (!day || !month || !year) {
        return Result<std::string>::failure("Дата пишется в виде ДД.ММ.ГГГГ, например 05.11.2026.");
    }
    if (*year < kMinYear || *year > kMaxYear) {
        return Result<std::string>::failure("Год должен быть в диапазоне от " + std::to_string(kMinYear) + " до "
            + std::to_string(kMaxYear) + ".");
    }
    if (*month < 1 || *month > kMonthsInYear) {
        return Result<std::string>::failure("Месяц должен быть в диапазоне от 1 до 12.");
    }
    if (*day < 1 || *day > days_in_month(*year, *month)) {
        return Result<std::string>::failure("День должен быть в диапазоне от 1 до "
            + std::to_string(days_in_month(*year, *month)) + ".");
    }
    return Result<std::string>::success(padded(*year, 4) + "-" + padded(*month, 2) + "-" + padded(*day, 2));
}

Result<std::string> parse_time_input(std::string_view text) {
    const std::string cleaned = trim(text);
    const std::string_view view(cleaned);
    const std::size_t colon = view.find(':');
    const auto hour = colon == std::string_view::npos ? std::nullopt : to_int(view.substr(0, colon));
    const auto minute = colon == std::string_view::npos ? std::nullopt : to_int(view.substr(colon + 1));
    if (!hour || !minute) {
        return Result<std::string>::failure("Время пишется в виде ЧЧ:ММ, например 09:30.");
    }
    if (*hour > 23) {
        return Result<std::string>::failure("Часы должны быть в диапазоне от 0 до 23.");
    }
    if (*minute > 59) {
        return Result<std::string>::failure("Минуты должны быть в диапазоне от 0 до 59.");
    }
    return Result<std::string>::success(padded(*hour, 2) + ":" + padded(*minute, 2));
}

std::string display_date(std::string_view date) {
    return std::string(date.substr(8, 2)) + "." + std::string(date.substr(5, 2)) + "." + std::string(date.substr(0, 4));
}

int month_index(std::string_view stamp) noexcept {
    const auto year = to_int(stamp.substr(0, 4));
    const auto month = to_int(stamp.substr(5, 2));
    return year.value_or(0) * kMonthsInYear + month.value_or(0);
}

std::string current_stamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    // tm_year считается от 1900 года, tm_mon от нуля.
    return padded(local.tm_year + 1900, 4) + "-" + padded(local.tm_mon + 1, 2) + "-" + padded(local.tm_mday, 2)
        + "T" + padded(local.tm_hour, 2) + ":" + padded(local.tm_min, 2);
}

}  // namespace xpay
