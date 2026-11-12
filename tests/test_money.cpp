// Файл: test_money.cpp
// Назначение: тесты разбора и вывода сумм, дат и времени, строк в UTF-8.

#include <string>

#include "check.hpp"
#include "xpay/datetime.hpp"
#include "xpay/money.hpp"
#include "xpay/text.hpp"

using namespace xpay;

void test_money() {
    // Разбор в копейки.
    CHECK_EQ(parse_amount("1500").value.value_or(-1), 150000);
    CHECK_EQ(parse_amount("1500,5").value.value_or(-1), 150050);
    CHECK_EQ(parse_amount("1500.05").value.value_or(-1), 150005);
    CHECK_EQ(parse_amount(" 1 500,50 ").value.value_or(-1), 150050);
    CHECK_EQ(parse_amount("1\xC2\xA0" "500").value.value_or(-1), 150000);
    CHECK_EQ(parse_amount("0,01").value.value_or(-1), 1);
    CHECK_EQ(parse_amount("007").value.value_or(-1), 700);

    // В дробных числах 0.1 + 0.2 не равно 0.3, в копейках равно.
    CHECK_EQ(*parse_amount("0,1").value + *parse_amount("0,2").value, *parse_amount("0,3").value);

    // Ввод букв вместо цифр и другие ошибки.
    for (const char* text : {"", "   ", "abc", "12a", "-5", "1,234", "1,2,3", "1e3", ",5", "5,", "+5", "пять"}) {
        const auto result = parse_amount(text);
        CHECK(!result.ok());
        CHECK(!result.error.empty());
    }
    CHECK_EQ(parse_amount("0").error, std::string("Сумма должна быть больше нуля."));
    CHECK(!parse_amount("0,00").ok());

    // Граница наибольшей суммы.
    CHECK_EQ(parse_amount("999999999,99").value.value_or(-1), kMaxAmount);
    CHECK(!parse_amount("1000000000").ok());
    CHECK(!parse_amount("99999999999999999999999999").ok());

    // Вывод.
    CHECK_EQ(format_amount(1250000), std::string("12 500,00"));
    CHECK_EQ(format_amount(5), std::string("0,05"));
    CHECK_EQ(format_amount(-123456789), std::string("-1 234 567,89"));
    CHECK_EQ(format_amount(100000), std::string("1 000,00"));
    CHECK_EQ(format_rubles(500000), std::string("5000 руб."));
    CHECK_EQ(format_rubles(500050), std::string("5000,50 руб."));
    CHECK_EQ(format_rubles(-250000), std::string("-2500 руб."));
    CHECK_EQ(format_plain(150050), std::string("1500,50"));

    // Выведенная сумма разбирается обратно.
    for (const Money kopecks : {Money{1}, Money{99}, Money{100}, Money{150050}, kMaxAmount}) {
        CHECK_EQ(parse_amount(format_plain(kopecks)).value.value_or(-1), kopecks);
        CHECK_EQ(parse_amount(format_amount(kopecks)).value.value_or(-1), kopecks);
    }
}

void test_datetime() {
    CHECK(is_valid_stamp("2026-11-12T09:30"));
    CHECK(is_valid_stamp("2024-02-29T00:00"));
    CHECK(!is_valid_stamp("2026-02-29T00:00"));
    CHECK(!is_valid_stamp("2026-13-01T00:00"));
    CHECK(!is_valid_stamp("2026-11-12T24:00"));
    CHECK(!is_valid_stamp("2026-11-12T09:60"));
    CHECK(!is_valid_stamp("2026-11-12 09:30"));
    CHECK(!is_valid_stamp("2026-11-12"));
    CHECK(!is_valid_stamp(""));
    CHECK(is_valid_date("1900-01-01"));
    CHECK(!is_valid_date("1899-12-31"));
    CHECK(!is_valid_date("2026-1-01"));

    // Високосные годы: 2000 да, 1900 нет.
    CHECK_EQ(days_in_month(2000, 2), 29);
    CHECK_EQ(days_in_month(1900, 2), 28);
    CHECK_EQ(days_in_month(2026, 4), 30);

    CHECK_EQ(parse_date_input("05.11.2026").value.value_or(""), std::string("2026-11-05"));
    CHECK_EQ(parse_date_input(" 5.1.2026 ").value.value_or(""), std::string("2026-01-05"));
    CHECK_EQ(parse_date_input("29.02.2024").value.value_or(""), std::string("2024-02-29"));
    CHECK_EQ(parse_date_input("30.02.2026").error, std::string("День должен быть в диапазоне от 1 до 28."));
    CHECK_EQ(parse_date_input("01.13.2026").error, std::string("Месяц должен быть в диапазоне от 1 до 12."));
    CHECK(!parse_date_input("01.01.1800").ok());
    for (const char* text : {"", "завтра", "2026-11-05", "05/11/2026", "05.11", "5.11.26a", "..", "1.2.3.4"}) {
        CHECK(!parse_date_input(text).ok());
    }

    CHECK_EQ(parse_time_input("9:05").value.value_or(""), std::string("09:05"));
    CHECK_EQ(parse_time_input("23:59").value.value_or(""), std::string("23:59"));
    CHECK_EQ(parse_time_input("24:00").error, std::string("Часы должны быть в диапазоне от 0 до 23."));
    CHECK_EQ(parse_time_input("12:60").error, std::string("Минуты должны быть в диапазоне от 0 до 59."));
    for (const char* text : {"", "полдень", "12", "12.30", ":30", "12:"}) {
        CHECK(!parse_time_input(text).ok());
    }

    CHECK_EQ(display_date("2026-11-05"), std::string("05.11.2026"));
    CHECK_EQ(month_index("2026-01-15T00:00") - month_index("2025-11-15T00:00"), 2);
    CHECK(is_valid_stamp(current_stamp()));

    // Строки в UTF-8.
    CHECK_EQ(trim("  \tтекст \r\n"), std::string("текст"));
    CHECK_EQ(utf8_length("Еда"), std::size_t{3});
    CHECK_EQ(utf8_length("abc😀"), std::size_t{4});
    CHECK_EQ(to_lower("ЕДА и Ёлка ЯЩИК Zoo"), std::string("еда и ёлка ящик zoo"));
    CHECK_EQ(pad_right("Еда", 5), std::string("Еда  "));
    CHECK_EQ(pad_left("Еда", 5), std::string("  Еда"));
    CHECK_EQ(pad_left("Транспорт", 3), std::string("Транспорт"));
}
