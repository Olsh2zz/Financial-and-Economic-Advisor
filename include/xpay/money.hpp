// Файл: money.hpp
// Назначение: денежные суммы. Сумма хранится целым числом копеек, чтобы
// при сложении не накапливалась ошибка дробных чисел.

#pragma once

#include <string>
#include <string_view>

#include "xpay/result.hpp"

namespace xpay {

/// Сумма в копейках.
using Money = long long;

inline constexpr Money kKopecksInRuble = 100;

/// Наибольшая сумма одной операции: 999 999 999,99 руб.
inline constexpr Money kMaxAmount = 99'999'999'999;

/// Разбирает «1500», «1 500,5» или «1500.50» в копейки.
/// Сумма должна быть больше нуля и не больше kMaxAmount.
Result<Money> parse_amount(std::string_view text);

/// «12 500,00»: разряды через пробел, всегда две цифры копеек.
std::string format_amount(Money kopecks);

/// «5000 руб.» как в образце из задания. Копейки выводятся, если они не нулевые.
std::string format_rubles(Money kopecks);

/// «1500,50»: без разделителей разрядов, для CSV и подсказок ввода.
std::string format_plain(Money kopecks);

}  // namespace xpay
