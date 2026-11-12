// Файл: test_app.cpp
// Назначение: тесты консольного меню: сценарии подаются строками вместо
// клавиатуры, проверяются вывод программы и изменения в данных.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "check.hpp"
#include "suites.hpp"
#include "xpay/app.hpp"
#include "xpay/budget.hpp"

using namespace xpay;

namespace {

// «Сейчас» в тестах всегда одно и то же, чтобы вывод не зависел от даты запуска.
const char* const kNow = "2026-11-12T09:30";

// Запускает меню со сценарием и возвращает весь вывод программы.
std::string run(State& state, const std::string& script) {
    std::istringstream in(script);
    std::ostringstream out;
    App app(in, out, state, [] { return std::string(kNow); });
    app.run();
    return out.str();
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

}  // namespace

void test_app() {
    // Главное меню совпадает с заданием построчно.
    {
        State state = default_state();
        const std::string output = run(state, "5\n");
        CHECK(contains(output,
            "1. Показать все операции\n"
            "2. Добавить операцию (Доход/Расход)\n"
            "3. Анализ и отчеты\n"
            "4. Настройки бюджетных лимитов\n"
            "---\n"
            "5. Выход\n"));
    }

    // Конец ввода в любом месте завершает программу, а не зацикливает её.
    {
        State state = default_state();
        run(state, "");
        run(state, "2\n");
        run(state, "2\n2\n100\n");
        run(state, "1\n1\n");
        run(state, "3\n3\n\n\n1\n");
        run(state, "4\n3\n1\n1\n");
        CHECK(state.operations.empty());
    }

    // Буквы вместо цифр в меню и в полях: программа объясняет и спрашивает снова.
    {
        State state = default_state();
        const std::string output = run(state,
            "abc\n9\n\n"      // три неверных выбора в главном меню
            "2\n"             // добавить операцию
            "x\n2\n"          // тип: буква, затем расход
            "сто\n-5\n0\n1500,505\n1500,50\n"  // сумма: четыре ошибки, затем верно
            "0\n9\nеда\n1\n"  // категория: три ошибки, затем «Еда»
            "31.02.2026\nвчера\n10.11.2026\n"  // дата: две ошибки, затем верно
            "25:00\n12:60\n8:05\n"             // время: две ошибки, затем верно
            "обед в столовой\n"
            "5\n");
        CHECK(contains(output, "Введите число от 1 до 5."));
        CHECK(contains(output, "Введите 1 или 2."));
        CHECK(contains(output, "Сумма пишется цифрами, после запятой не больше двух знаков."));
        CHECK(contains(output, "Сумма должна быть больше нуля."));
        CHECK(contains(output, "День должен быть в диапазоне от 1 до 28."));
        CHECK(contains(output, "Дата пишется в виде ДД.ММ.ГГГГ"));
        CHECK(contains(output, "Часы должны быть в диапазоне от 0 до 23."));
        CHECK(contains(output, "Минуты должны быть в диапазоне от 0 до 59."));
        CHECK(contains(output, "Операция № 1 добавлена."));
        CHECK_EQ(state.operations.size(), std::size_t{1});
        CHECK_EQ(state.operations[0].at, std::string("2026-11-10T08:05"));
        CHECK_EQ(state.operations[0].amount, 150050);
        CHECK_EQ(state.operations[0].category, std::string("Еда"));
        CHECK_EQ(state.operations[0].note, std::string("обед в столовой"));
    }

    // Дата и время подставляются автоматически, описание необязательно.
    {
        State state = default_state();
        const std::string output = run(state, "2\n1\n60000\n1\n\n\n\n5\n");
        CHECK(contains(output, "Дата (ДД.ММ.ГГГГ, Enter: 12.11.2026): "));
        CHECK(contains(output, "Время (ЧЧ:ММ, Enter: 09:30): "));
        CHECK_EQ(state.operations[0].at, std::string(kNow));
        CHECK(state.operations[0].kind == Kind::Income);
        CHECK_EQ(state.operations[0].category, std::string("Зарплата"));
        CHECK_EQ(state.operations[0].note, std::string(""));
        // Отмена на первом шаге ничего не добавляет.
        run(state, "2\n0\n5\n");
        CHECK_EQ(state.operations.size(), std::size_t{1});
    }

    // Таблица операций и фильтр.
    {
        State state = test::state_with(test::sample_rows());
        const std::string all = run(state, "1\n0\n5\n");
        CHECK(contains(all,
            "№  Дата        Время  Тип     Категория    Сумма, руб.  Описание\n"
            "5  20.10.2026  21:00  Расход  Развлечения    -5 000,00\n"
            "4  12.10.2026  08:15  Расход  Транспорт      -2 500,00  проездной\n"
            "3  10.10.2026  19:00  Расход  Еда            -2 000,00\n"
            "2  03.10.2026  12:30  Расход  Еда            -3 000,00  продукты на неделю\n"
            "1  01.10.2026  10:00  Доход   Зарплата      +60 000,00\n"
            "Итого: доходы 60000 руб., расходы 12500 руб., баланс 47500 руб.\n"));

        // Фильтр: с 03.10 по 12.10, категория «Еда» (номер 1), только расходы.
        const std::string filtered = run(state, "1\n1\n03.10.2026\n12.10.2026\n1\n2\n0\n5\n");
        CHECK(contains(filtered,
            "--- Операции (фильтр включён) ---\n"
            "№  Дата        Время  Тип     Категория  Сумма, руб.  Описание\n"
            "3  10.10.2026  19:00  Расход  Еда          -2 000,00\n"
            "2  03.10.2026  12:30  Расход  Еда          -3 000,00  продукты на неделю\n"
            "Итого: доходы 0 руб., расходы 5000 руб., баланс -5000 руб.\n"));

        // Перепутанные границы периода спрашиваются заново; сброс возвращает всё.
        const std::string swapped = run(state, "1\n1\n12.10.2026\n03.10.2026\n\n\n0\n1\n2\n0\n5\n");
        CHECK(contains(swapped, "Начало периода позже его конца."));
        CHECK(contains(swapped, "1  01.10.2026  10:00  Доход"));

        const std::string none = run(state, "1\n1\n01.01.2030\n\n0\n0\n0\n5\n");
        CHECK(contains(none, "Операций нет.\n"));
    }

    // Правка: Enter оставляет поле, можно поменять все поля и убрать описание.
    {
        State state = test::state_with(test::sample_rows());
        // Операция 4: всё оставить, кроме суммы.
        const std::string kept = run(state, "1\n3\n4\n\n3000\n\n\n\n\n0\n5\n");
        CHECK(contains(kept, "Операция № 4 изменена."));
        const Operation* item = find_operation(state, 4);
        CHECK_EQ(item->amount, 300000);
        CHECK_EQ(item->at, std::string("2026-10-12T08:15"));
        CHECK_EQ(item->category, std::string("Транспорт"));
        CHECK_EQ(item->note, std::string("проездной"));

        // Операция 4: тип на доход (категорию придётся выбрать заново), новые дата, время, без описания.
        run(state, "1\n3\n4\n1\n\n\n2\n15.10.2026\n10:00\n-\n0\n5\n");
        item = find_operation(state, 4);
        CHECK(item->kind == Kind::Income);
        CHECK_EQ(item->category, std::string("Подработка"));
        CHECK_EQ(item->amount, 300000);
        CHECK_EQ(item->at, std::string("2026-10-15T10:00"));
        CHECK_EQ(item->note, std::string(""));

        CHECK(contains(run(state, "1\n3\n77\n0\n5\n"), "Введите число от 0 до 6."));
        CHECK(contains(run(state, "1\n3\n0\n0\n5\n"), "--- Операции ---"));
    }

    // Удаление с подтверждением.
    {
        State state = test::state_with(test::sample_rows());
        const std::string refused = run(state, "1\n4\n3\nн\n0\n5\n");
        CHECK(contains(refused, "Удалить операцию № 3? (д/н): "));
        CHECK(contains(refused, "Удаление отменено."));
        CHECK_EQ(state.operations.size(), std::size_t{5});

        const std::string confirmed = run(state, "1\n4\n3\nможет быть\nДА\n0\n5\n");
        CHECK(contains(confirmed, "Ответьте «д» (да) или «н» (нет)."));
        CHECK(contains(confirmed, "Операция удалена."));
        CHECK(find_operation(state, 3) == nullptr);
        CHECK(contains(run(state, "1\n4\n3\n0\n5\n"), "Операции с таким номером нет."));
    }

    // Статистика за период: строки совпадают с образцом из задания.
    {
        State state = test::state_with(test::sample_rows());
        const std::string output = run(state, "3\n1\n01.10.2026\n31.10.2026\n0\n5\n");
        CHECK(contains(output,
            "Финансовый отчёт с 01.10.2026 по 31.10.2026\n"
            "\n"
            "Доходы:  60000 руб.\n"
            "Расходы: 12500 руб.\n"
            "Баланс:  47500 руб.\n"
            "\n"
            "Расходы по категориям:\n"
            "Еда: [#######] 40% (5000 руб.)\n"
            "Развлечения: [#######] 40% (5000 руб.)\n"
            "Транспорт: [####] 20% (2500 руб.)\n"));

        const std::string outlook = run(state, "3\n2\n0\n5\n");
        CHECK(contains(outlook,
            "Прогноз по истории за 1 мес.\n"
            "Средний доход в месяц:  60000 руб.\n"
            "Средний расход в месяц: 12500 руб.\n"
            "Сберегаемый потенциал:  47500 руб. в месяц\n"
            "Накопится за 3 мес.: 142500 руб.\n"
            "Накопится за 6 мес.: 285000 руб.\n"
            "Накопится за 12 мес.: 570000 руб.\n"));

        State empty = default_state();
        CHECK(contains(run(empty, "3\n2\n0\n5\n"), "Для прогноза нужна хотя бы одна операция."));
    }

    // Экспорт в .txt и .csv.
    {
        State state = test::state_with(test::sample_rows());
        const std::string text_path = "xpay_test_report.txt";
        const std::string csv_path = "xpay_test_report.csv";
        const std::string output = run(state,
            "3\n3\n\n\n1\n" + text_path + "\n"
            "3\n10.10.2026\n\n2\n" + csv_path + "\n"
            "3\n\n\n0\n"
            "0\n5\n");
        CHECK(contains(output, "Отчёт записан в файл «" + text_path + "»."));
        CHECK(contains(output, "Отчёт записан в файл «" + csv_path + "»."));

        const std::string text = read_file(text_path);
        CHECK(contains(text, "Еда: [#######] 40% (5000 руб.)\n"));
        CHECK(contains(text, "Операции (5):\n20.10.2026 21:00  -5 000,00  Развлечения\n"));

        const std::string csv = read_file(csv_path);
        CHECK(csv.rfind("\xEF\xBB\xBF" "Дата;Время;Тип;Категория;Сумма;Описание\r\n", 0) == 0);
        CHECK(contains(csv, "12.10.2026;08:15;Расход;Транспорт;2500,00;проездной\r\n"));
        CHECK(!contains(csv, "03.10.2026"));
        CHECK(contains(csv, "Расходы;;;;9500,00;\r\n"));
        std::remove(text_path.c_str());
        std::remove(csv_path.c_str());

        CHECK(contains(run(state, "3\n3\n\n\n1\nнет_такой_папки/отчёт.txt\n0\n5\n"), "Не удалось записать файл"));
    }

    // Лимиты: установка, предупреждение при добавлении расхода, снятие.
    {
        State state = default_state();
        // Лимит 10000 на «Развлечения» (категория 4).
        const std::string set = run(state, "4\n1\n4\nмного\n10000\n0\n5\n");
        CHECK(contains(set, "Лимит на «Развлечения»: 10000 руб. в месяц."));
        CHECK(contains(set, "  Развлечения  лимит 10000 руб., потрачено 0 руб., осталось 10000 руб.\n"));
        CHECK_EQ(state.limits.at("Развлечения"), 1000000);

        // Расход ровно на лимит предупреждения не даёт.
        const std::string exact = run(state, "2\n2\n10000\n4\n\n\n\n5\n");
        CHECK(!contains(exact, "Внимание"));
        // Следующий расход превышает лимит.
        const std::string over = run(state, "2\n2\n2000,50\n4\n\n\n\n5\n");
        CHECK(contains(over,
            "Внимание: лимит на «Развлечения» превышен. Потрачено 12000,50 руб. из 10000 руб., "
            "перерасход 2000,50 руб.\n"));
        // Лимит считается по месяцу операции: небольшой расход в октябре его не задевает,
        // как и расход в другой категории.
        CHECK(!contains(run(state, "2\n2\n500\n4\n01.10.2026\n\n\n5\n"), "Внимание"));
        CHECK(!contains(run(state, "2\n2\n50000\n1\n\n\n\n5\n"), "Внимание"));

        const std::string shown = run(state, "4\n0\n5\n");
        CHECK(contains(shown, "Лимиты на 11.2026:\n"));
        CHECK(contains(shown, "  Развлечения  лимит 10000 руб., потрачено 12000,50 руб., перерасход 2000,50 руб.\n"));
        CHECK(contains(shown, "  Еда          без лимита, потрачено 50000 руб.\n"));

        // Лимит ниже уже потраченного: предупреждение сразу при установке.
        CHECK(contains(run(state, "4\n1\n1\n100\n0\n5\n"),
            "Внимание: в этом месяце уже потрачено 50000 руб., перерасход 49900 руб.\n"));

        CHECK(contains(run(state, "4\n2\n4\n0\n5\n"), "Лимит на «Развлечения» снят."));
        CHECK(contains(run(state, "4\n2\n4\n0\n5\n"), "На «Развлечения» лимита не было."));
        run(state, "4\n2\n1\n0\n5\n");
        CHECK(contains(run(state, "4\n2\n0\n5\n"), "Лимиты не заданы."));
    }

    // Категории настраиваются.
    {
        State state = test::state_with(test::sample_rows());
        const std::string added = run(state, "4\n3\n1\n2\nеда\nКофе\n0\n0\n5\n");
        CHECK(contains(added, "Категория «Еда» уже есть."));
        CHECK(contains(added, "Категория «Кофе» добавлена."));
        CHECK(contains(added, "Категории расходов: Еда, Транспорт, Жильё, Развлечения, Здоровье, Кофе\n"));
        CHECK_EQ(state.expense_categories.back(), std::string("Кофе"));

        // Новую категорию видно при добавлении операции (номер 6).
        run(state, "2\n2\n250\n6\n\n\n\n5\n");
        CHECK_EQ(state.operations.back().category, std::string("Кофе"));

        const std::string busy = run(state, "4\n3\n2\n2\n6\nд\n0\n0\n5\n");
        CHECK(contains(busy, "Категорию «Кофе» нельзя удалить: в ней есть операции (1)."));
        const std::string removed = run(state, "4\n3\n2\n2\n5\nд\n0\n0\n5\n");
        CHECK(contains(removed, "Категория удалена."));
        CHECK_EQ(state.expense_categories.size(), std::size_t{5});
        CHECK(contains(run(state, "4\n3\n2\n1\n3\nн\n0\n0\n5\n"), "Удаление отменено."));
        // Пустое название отменяет добавление.
        run(state, "4\n3\n1\n1\n   \n0\n0\n5\n");
        CHECK_EQ(state.income_categories.size(), std::size_t{3});
    }
}
