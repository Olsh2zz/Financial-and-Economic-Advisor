// Файл: test_main.cpp
// Назначение: запускает все наборы тестов и возвращает ненулевой код,
// если хотя бы одна проверка не прошла.

#include <iostream>

#include "check.hpp"
#include "suites.hpp"

int main() {
    test_money();
    test_datetime();
    test_ledger();
    test_report();
    test_budget();
    test_storage();
    test_api();
    test_app();

    if (test::failures() > 0) {
        std::cerr << "Не прошло проверок: " << test::failures() << " из " << test::checks() << "\n";
        return 1;
    }
    std::cout << "Все проверки прошли: " << test::checks() << "\n";
    return 0;
}
