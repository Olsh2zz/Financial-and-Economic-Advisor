// Файл: datetime.hpp
// Назначение: дата и время операции. Момент хранится строкой вида
// «2026-11-12T09:30» по местному времени: такие строки сравниваются как текст,
// и порядок совпадает с порядком по времени.

#pragma once

#include <string>
#include <string_view>

#include "xpay/result.hpp"

namespace xpay {

inline constexpr int kMinYear = 1900;
inline constexpr int kMaxYear = 9999;

/// Число дней в месяце с учётом високосного года. month от 1 до 12.
int days_in_month(int year, int month) noexcept;

/// Проверяет дату «ГГГГ-ММ-ДД»: формат и существование такого дня.
bool is_valid_date(std::string_view date) noexcept;

/// Проверяет момент «ГГГГ-ММ-ДДTЧЧ:ММ».
bool is_valid_stamp(std::string_view stamp) noexcept;

/// Разбирает ввод пользователя «ДД.ММ.ГГГГ» в «ГГГГ-ММ-ДД».
Result<std::string> parse_date_input(std::string_view text);

/// Разбирает ввод пользователя «ЧЧ:ММ», возвращает его же с ведущими нулями.
Result<std::string> parse_time_input(std::string_view text);

/// «2026-11-12» превращает в «12.11.2026». Аргумент должен быть верной датой.
std::string display_date(std::string_view date);

/// Номер месяца по порядку от начала летоисчисления: год * 12 + месяц.
/// По разности таких номеров считается число месяцев между операциями.
int month_index(std::string_view stamp) noexcept;

/// Текущий момент по часам компьютера.
std::string current_stamp();

}  // namespace xpay
