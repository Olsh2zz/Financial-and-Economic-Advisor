// Файл: app.cpp
// Назначение: реализация консольного меню.

#include "xpay/app.hpp"

#include <algorithm>
#include <cstddef>
#include <istream>
#include <ostream>
#include <utility>

#include "xpay/budget.hpp"
#include "xpay/datetime.hpp"
#include "xpay/report.hpp"
#include "xpay/storage.hpp"
#include "xpay/text.hpp"

namespace xpay {

namespace {

// Метка порядка байтов: без неё Excel читает CSV в UTF-8 в другой кодировке.
constexpr const char* kByteOrderMark = "\xEF\xBB\xBF";

// Номера пунктов меню и операций: не больше шести цифр.
std::optional<int> to_int(const std::string& text) noexcept {
    return parse_digits(text, 6);
}

std::string stamp_date(const std::string& stamp) {
    return stamp.substr(0, 10);
}

std::string stamp_time(const std::string& stamp) {
    return stamp.substr(11);
}

std::string quoted(const std::string& text) {
    return "«" + text + "»";
}

}  // namespace

App::App(std::istream& in, std::ostream& out, State& state, Clock clock)
    : in_(in), out_(out), state_(state), clock_(std::move(clock)) {}

// --- Ввод ---

std::optional<std::string> App::read_line(std::string_view prompt) {
    if (input_closed_) {
        return std::nullopt;
    }
    out_ << prompt << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        // Без этого флага меню спрашивало бы снова и зациклилось на конце ввода.
        input_closed_ = true;
        out_ << "\n";
        return std::nullopt;
    }
    return trim(line);
}

std::optional<int> App::read_choice(std::string_view prompt, int min, int max) {
    while (const auto line = read_line(prompt)) {
        const std::optional<int> value = to_int(*line);
        if (value && *value >= min && *value <= max) {
            return value;
        }
        out_ << "Введите число от " << min << " до " << max << ".\n";
    }
    return std::nullopt;
}

std::optional<Money> App::read_amount(std::string_view prompt, std::optional<Money> fallback) {
    while (const auto line = read_line(prompt)) {
        if (line->empty() && fallback) {
            return fallback;
        }
        const Result<Money> parsed = parse_amount(*line);
        if (parsed.ok()) {
            return parsed.value;
        }
        out_ << parsed.error << "\n";
    }
    return std::nullopt;
}

std::optional<std::string> App::read_date(std::string_view prompt, const std::string& fallback) {
    while (const auto line = read_line(prompt)) {
        if (line->empty()) {
            return fallback;
        }
        const Result<std::string> parsed = parse_date_input(*line);
        if (parsed.ok()) {
            return parsed.value;
        }
        out_ << parsed.error << "\n";
    }
    return std::nullopt;
}

std::optional<std::string> App::read_time(const std::string& fallback) {
    const std::string prompt = "Время (ЧЧ:ММ, Enter: " + fallback + "): ";
    while (const auto line = read_line(prompt)) {
        if (line->empty()) {
            return fallback;
        }
        const Result<std::string> parsed = parse_time_input(*line);
        if (parsed.ok()) {
            return parsed.value;
        }
        out_ << parsed.error << "\n";
    }
    return std::nullopt;
}

std::optional<bool> App::read_yes_no(std::string_view prompt) {
    while (const auto line = read_line(prompt)) {
        const std::string answer = to_lower(*line);
        if (answer == "д" || answer == "да" || answer == "y" || answer == "yes") {
            return true;
        }
        if (answer == "н" || answer == "нет" || answer == "n" || answer == "no") {
            return false;
        }
        out_ << "Ответьте «д» (да) или «н» (нет).\n";
    }
    return std::nullopt;
}

std::optional<Kind> App::read_kind(std::optional<Kind> fallback) {
    const std::string prompt = fallback
        ? "Тип (1: доход, 2: расход, Enter: " + to_lower(kind_title(*fallback)) + "): "
        : std::string("Тип (1: доход, 2: расход, 0: отмена): ");
    while (const auto line = read_line(prompt)) {
        if (line->empty() && fallback) {
            return fallback;
        }
        if (*line == "1") {
            return Kind::Income;
        }
        if (*line == "2") {
            return Kind::Expense;
        }
        if (*line == "0" && !fallback) {
            return std::nullopt;
        }
        out_ << "Введите 1 или 2.\n";
    }
    return std::nullopt;
}

std::optional<std::string> App::read_category(Kind kind, const std::string& fallback) {
    const std::vector<std::string>& list = state_.categories(kind);
    out_ << "Категории:\n";
    for (std::size_t i = 0; i < list.size(); ++i) {
        out_ << "  " << i + 1 << ". " << list[i] << "\n";
    }
    const std::string prompt = fallback.empty() ? std::string("Категория (номер): ")
                                                : "Категория (номер, Enter: " + fallback + "): ";
    while (const auto line = read_line(prompt)) {
        if (line->empty() && !fallback.empty()) {
            return fallback;
        }
        const std::optional<int> number = to_int(*line);
        if (number && *number >= 1 && static_cast<std::size_t>(*number) <= list.size()) {
            return list[static_cast<std::size_t>(*number) - 1];
        }
        out_ << "Введите число от 1 до " << list.size() << ".\n";
    }
    return std::nullopt;
}

std::optional<Filter> App::read_period() {
    while (!input_closed_) {
        Filter period;
        const auto from = read_date("С даты (ДД.ММ.ГГГГ, Enter: без ограничения): ", "");
        if (!from) {
            break;
        }
        const auto to = read_date("По дату (ДД.ММ.ГГГГ, Enter: без ограничения): ", "");
        if (!to) {
            break;
        }
        period.from = *from;
        period.to = *to;
        if (period.from.empty() || period.to.empty() || period.from <= period.to) {
            return period;
        }
        out_ << "Начало периода позже его конца. Введите даты ещё раз.\n";
    }
    return std::nullopt;
}

std::optional<Filter> App::read_filter() {
    std::optional<Filter> filter = read_period();
    if (!filter) {
        return std::nullopt;
    }

    std::vector<std::string> names = state_.expense_categories;
    for (const std::string& name : state_.income_categories) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
    out_ << "Категории:\n  0. Все\n";
    for (std::size_t i = 0; i < names.size(); ++i) {
        out_ << "  " << i + 1 << ". " << names[i] << "\n";
    }
    const auto category = read_choice("Категория (номер): ", 0, static_cast<int>(names.size()));
    if (!category) {
        return std::nullopt;
    }
    if (*category > 0) {
        filter->category = names[static_cast<std::size_t>(*category) - 1];
    }

    const auto kind = read_choice("Тип (0: все, 1: доходы, 2: расходы): ", 0, 2);
    if (!kind) {
        return std::nullopt;
    }
    if (*kind > 0) {
        filter->kind = *kind == 1 ? Kind::Income : Kind::Expense;
    }
    return filter;
}

// Спрашивает поля операции. При правке (current не пуст) Enter оставляет
// прежнее значение поля.
std::optional<Draft> App::read_draft(const Operation* current) {
    Draft draft;
    const auto kind = read_kind(current != nullptr ? std::optional<Kind>(current->kind) : std::nullopt);
    if (!kind) {
        return std::nullopt;
    }
    draft.kind = *kind;

    const std::string amount_prompt = current != nullptr
        ? "Сумма, руб. (Enter: " + format_plain(current->amount) + "): "
        : std::string("Сумма, руб.: ");
    const auto amount = read_amount(amount_prompt,
        current != nullptr ? std::optional<Money>(current->amount) : std::nullopt);
    if (!amount) {
        return std::nullopt;
    }
    draft.amount = *amount;

    // Прежняя категория годится, только если тип операции не поменяли.
    const bool keep_category = current != nullptr && current->kind == draft.kind;
    const auto category = read_category(draft.kind, keep_category ? current->category : std::string());
    if (!category) {
        return std::nullopt;
    }
    draft.category = *category;

    const std::string base = current != nullptr ? current->at : clock_();
    const auto date = read_date("Дата (ДД.ММ.ГГГГ, Enter: " + display_date(stamp_date(base)) + "): ", stamp_date(base));
    if (!date) {
        return std::nullopt;
    }
    const auto time = read_time(stamp_time(base));
    if (!time) {
        return std::nullopt;
    }
    draft.at = *date + "T" + *time;

    while (!input_closed_) {
        const bool has_note = current != nullptr && !current->note.empty();
        const auto note = read_line(has_note ? "Описание (Enter: оставить, «-»: убрать): "
                                             : "Описание (Enter: без описания): ");
        if (!note) {
            break;
        }
        if (utf8_length(*note) > kMaxNoteLength) {
            out_ << "Описание не длиннее " << kMaxNoteLength << " символов.\n";
            continue;
        }
        if (has_note && note->empty()) {
            draft.note = current->note;
        } else if (!(has_note && *note == "-")) {
            draft.note = *note;
        }
        return draft;
    }
    return std::nullopt;
}

// --- Вывод ---

void App::print_operations(const std::vector<Operation>& operations) {
    if (operations.empty()) {
        out_ << "Операций нет.\n";
        return;
    }
    // Ширина столбцов подбирается по самому длинному значению.
    std::size_t id_width = 1;
    std::size_t category_width = utf8_length("Категория");
    std::size_t amount_width = utf8_length("Сумма, руб.");
    for (const Operation& item : operations) {
        id_width = std::max(id_width, std::to_string(item.id).size());
        category_width = std::max(category_width, utf8_length(item.category));
        amount_width = std::max(amount_width, format_amount(item.amount).size() + 1);
    }
    const std::size_t kind_width = utf8_length("Расход");

    out_ << pad_left("№", id_width) << "  " << pad_right("Дата", 10) << "  " << pad_right("Время", 5) << "  "
         << pad_right("Тип", kind_width) << "  " << pad_right("Категория", category_width) << "  "
         << pad_left("Сумма, руб.", amount_width) << "  Описание\n";
    for (const Operation& item : operations) {
        const std::string amount = (item.kind == Kind::Income ? "+" : "-") + format_amount(item.amount);
        out_ << pad_left(std::to_string(item.id), id_width) << "  " << display_date(stamp_date(item.at)) << "  "
             << stamp_time(item.at) << "  " << pad_right(kind_title(item.kind), kind_width) << "  "
             << pad_right(item.category, category_width) << "  " << pad_left(amount, amount_width);
        if (!item.note.empty()) {
            out_ << "  " << item.note;
        }
        out_ << "\n";
    }
    const Totals sums = totals(operations);
    out_ << "Итого: доходы " << format_rubles(sums.income) << ", расходы " << format_rubles(sums.expense)
         << ", баланс " << format_rubles(sums.balance) << "\n";
}

void App::print_limit_warning(const Operation& operation) {
    if (operation.kind != Kind::Expense) {
        return;
    }
    const auto status = check_limit(state_, operation.category, std::string_view(operation.at).substr(0, 7));
    if (status && status->exceeded) {
        out_ << "Внимание: лимит на " << quoted(operation.category) << " превышен. Потрачено "
             << format_rubles(status->spent) << " из " << format_rubles(status->limit) << ", перерасход "
             << format_rubles(-status->left) << "\n";
    }
}

void App::print_limits() {
    const std::string month = clock_().substr(0, 7);
    out_ << "Лимиты на " << month.substr(5, 2) << "." << month.substr(0, 4) << ":\n";
    std::size_t width = 0;
    for (const std::string& category : state_.expense_categories) {
        width = std::max(width, utf8_length(category));
    }
    for (const std::string& category : state_.expense_categories) {
        out_ << "  " << pad_right(category, width) << "  ";
        const auto status = check_limit(state_, category, month);
        if (!status) {
            out_ << "без лимита, потрачено " << format_rubles(month_spent(state_.operations, category, month));
        } else if (status->exceeded) {
            out_ << "лимит " << format_rubles(status->limit) << ", потрачено " << format_rubles(status->spent)
                 << ", перерасход " << format_rubles(-status->left);
        } else {
            out_ << "лимит " << format_rubles(status->limit) << ", потрачено " << format_rubles(status->spent)
                 << ", осталось " << format_rubles(status->left);
        }
        out_ << "\n";
    }
}

void App::print_categories() {
    for (const Kind kind : {Kind::Expense, Kind::Income}) {
        out_ << (kind == Kind::Expense ? "Категории расходов: " : "Категории доходов: ");
        const std::vector<std::string>& list = state_.categories(kind);
        for (std::size_t i = 0; i < list.size(); ++i) {
            out_ << (i > 0 ? ", " : "") << list[i];
        }
        out_ << "\n";
    }
}

// --- Меню ---

void App::run() {
    while (!input_closed_) {
        out_ << "\n=== Финансово-экономический советник ===\n"
                "1. Показать все операции\n"
                "2. Добавить операцию (Доход/Расход)\n"
                "3. Анализ и отчеты\n"
                "4. Настройки бюджетных лимитов\n"
                "---\n"
                "5. Выход\n";
        const auto choice = read_choice("Выберите пункт: ", 1, 5);
        if (!choice || *choice == 5) {
            break;
        }
        switch (*choice) {
            case 1: operations_menu(); break;
            case 2: add_operation_flow(); break;
            case 3: reports_menu(); break;
            default: settings_menu(); break;
        }
    }
}

void App::operations_menu() {
    Filter filter;
    while (!input_closed_) {
        out_ << "\n--- Операции" << (filter.empty() ? "" : " (фильтр включён)") << " ---\n";
        print_operations(filter_operations(state_.operations, filter));
        out_ << "\n1. Фильтр по дате, категории и типу\n"
                "2. Сбросить фильтр\n"
                "3. Редактировать операцию\n"
                "4. Удалить операцию\n"
                "0. Назад\n";
        const auto choice = read_choice("Выберите пункт: ", 0, 4);
        if (!choice || *choice == 0) {
            return;
        }
        if (*choice == 1) {
            if (const auto entered = read_filter()) {
                filter = *entered;
            }
        } else if (*choice == 2) {
            filter = Filter{};
        } else if (*choice == 3) {
            edit_operation_flow();
        } else {
            remove_operation_flow();
        }
    }
}

void App::add_operation_flow() {
    out_ << "\n--- Новая операция ---\n";
    const auto draft = read_draft(nullptr);
    if (!draft) {
        out_ << "Операция не добавлена.\n";
        return;
    }
    const Result<Operation> result = add_operation(state_, *draft);
    if (!result.ok()) {
        out_ << result.error << "\n";
        return;
    }
    out_ << "Операция № " << result.value->id << " добавлена.\n";
    print_limit_warning(*result.value);
}

void App::edit_operation_flow() {
    if (state_.operations.empty()) {
        out_ << "Редактировать нечего: операций нет.\n";
        return;
    }
    const auto id = read_choice("Номер операции (0: отмена): ", 0, state_.next_id);
    if (!id || *id == 0) {
        return;
    }
    const Operation* found = find_operation(state_, *id);
    if (found == nullptr) {
        out_ << "Операции с таким номером нет.\n";
        return;
    }
    // Копия нужна, потому что правка заменит операцию в списке.
    const Operation current = *found;
    out_ << "Enter оставляет прежнее значение поля.\n";
    const auto draft = read_draft(&current);
    if (!draft) {
        out_ << "Операция не изменена.\n";
        return;
    }
    const Result<Operation> result = update_operation(state_, current.id, *draft);
    if (!result.ok()) {
        out_ << result.error << "\n";
        return;
    }
    out_ << "Операция № " << current.id << " изменена.\n";
    print_limit_warning(*result.value);
}

void App::remove_operation_flow() {
    if (state_.operations.empty()) {
        out_ << "Удалять нечего: операций нет.\n";
        return;
    }
    const auto id = read_choice("Номер операции (0: отмена): ", 0, state_.next_id);
    if (!id || *id == 0) {
        return;
    }
    const Operation* found = find_operation(state_, *id);
    if (found == nullptr) {
        out_ << "Операции с таким номером нет.\n";
        return;
    }
    print_operations({*found});
    const auto confirmed = read_yes_no("Удалить операцию № " + std::to_string(*id) + "? (д/н): ");
    if (confirmed && *confirmed) {
        remove_operation(state_, *id);
        out_ << "Операция удалена.\n";
    } else {
        out_ << "Удаление отменено.\n";
    }
}

void App::reports_menu() {
    while (!input_closed_) {
        out_ << "\n--- Анализ и отчеты ---\n"
                "1. Статистика за период\n"
                "2. Финансовый прогноз\n"
                "3. Экспорт отчета в файл\n"
                "0. Назад\n";
        const auto choice = read_choice("Выберите пункт: ", 0, 3);
        if (!choice || *choice == 0) {
            return;
        }
        if (*choice == 1) {
            statistics_flow();
        } else if (*choice == 2) {
            forecast_flow();
        } else {
            export_flow();
        }
    }
}

void App::statistics_flow() {
    const auto period = read_period();
    if (!period) {
        return;
    }
    out_ << "\n" << build_summary(filter_operations(state_.operations, *period), *period);
}

void App::forecast_flow() {
    const std::optional<Forecast> result = forecast(state_.operations);
    if (!result) {
        out_ << "Для прогноза нужна хотя бы одна операция.\n";
        return;
    }
    out_ << "\nПрогноз по истории за " << result->months << " мес.\n"
         << "Средний доход в месяц:  " << format_rubles(result->average_income) << "\n"
         << "Средний расход в месяц: " << format_rubles(result->average_expense) << "\n"
         << "Сберегаемый потенциал:  " << format_rubles(result->potential) << " в месяц\n";
    if (result->potential <= 0) {
        out_ << "Расходы в среднем не меньше доходов, откладывать не из чего.\n";
        return;
    }
    for (const Projection& projection : result->projections) {
        out_ << "Накопится за " << projection.months << " мес.: " << format_rubles(projection.amount) << "\n";
    }
}

void App::export_flow() {
    const auto period = read_period();
    if (!period) {
        return;
    }
    const auto format = read_choice("Формат (1: текст .txt, 2: таблица .csv, 0: отмена): ", 0, 2);
    if (!format || *format == 0) {
        return;
    }
    const std::string fallback = *format == 1 ? "report.txt" : "report.csv";
    const auto name = read_line("Имя файла (Enter: " + fallback + "): ");
    if (!name) {
        return;
    }
    const std::string path = name->empty() ? fallback : *name;
    const std::vector<Operation> operations = filter_operations(state_.operations, *period);
    const std::string text = *format == 1 ? build_text_report(operations, *period)
                                          : kByteOrderMark + build_csv(operations);
    const Status written = write_text_file(path, text);
    if (written.ok()) {
        out_ << "Отчёт записан в файл " << quoted(path) << ".\n";
    } else {
        out_ << written.error << "\n";
    }
}

void App::settings_menu() {
    while (!input_closed_) {
        out_ << "\n--- Настройки бюджетных лимитов ---\n";
        print_limits();
        out_ << "\n1. Установить лимит\n"
                "2. Снять лимит\n"
                "3. Категории\n"
                "0. Назад\n";
        const auto choice = read_choice("Выберите пункт: ", 0, 3);
        if (!choice || *choice == 0) {
            return;
        }
        if (*choice == 1) {
            set_limit_flow();
        } else if (*choice == 2) {
            clear_limit_flow();
        } else {
            categories_menu();
        }
    }
}

void App::set_limit_flow() {
    const auto category = read_category(Kind::Expense, "");
    if (!category) {
        return;
    }
    const auto amount = read_amount("Лимит на месяц, руб.: ", std::nullopt);
    if (!amount) {
        return;
    }
    const Status status = set_limit(state_, *category, *amount);
    if (!status.ok()) {
        out_ << status.error << "\n";
        return;
    }
    out_ << "Лимит на " << quoted(*category) << ": " << format_rubles(*amount) << " в месяц.\n";
    const auto current = check_limit(state_, *category, clock_().substr(0, 7));
    if (current && current->exceeded) {
        out_ << "Внимание: в этом месяце уже потрачено " << format_rubles(current->spent) << ", перерасход "
             << format_rubles(-current->left) << "\n";
    }
}

void App::clear_limit_flow() {
    if (state_.limits.empty()) {
        out_ << "Лимиты не заданы.\n";
        return;
    }
    const auto category = read_category(Kind::Expense, "");
    if (!category) {
        return;
    }
    if (clear_limit(state_, *category)) {
        out_ << "Лимит на " << quoted(*category) << " снят.\n";
    } else {
        out_ << "На " << quoted(*category) << " лимита не было.\n";
    }
}

void App::categories_menu() {
    while (!input_closed_) {
        out_ << "\n--- Категории ---\n";
        print_categories();
        out_ << "\n1. Добавить категорию\n"
                "2. Удалить категорию\n"
                "0. Назад\n";
        const auto choice = read_choice("Выберите пункт: ", 0, 2);
        if (!choice || *choice == 0) {
            return;
        }
        if (*choice == 1) {
            add_category_flow();
        } else {
            remove_category_flow();
        }
    }
}

void App::add_category_flow() {
    const auto kind = read_kind(std::nullopt);
    if (!kind) {
        return;
    }
    while (const auto name = read_line("Название категории (Enter: отмена): ")) {
        if (name->empty()) {
            return;
        }
        const Status status = add_category(state_, *kind, *name);
        if (status.ok()) {
            out_ << "Категория " << quoted(*name) << " добавлена.\n";
            return;
        }
        out_ << status.error << "\n";
    }
}

void App::remove_category_flow() {
    const auto kind = read_kind(std::nullopt);
    if (!kind) {
        return;
    }
    const auto name = read_category(*kind, "");
    if (!name) {
        return;
    }
    const auto confirmed = read_yes_no("Удалить категорию " + quoted(*name) + "? (д/н): ");
    if (!confirmed || !*confirmed) {
        out_ << "Удаление отменено.\n";
        return;
    }
    const Status status = remove_category(state_, *kind, *name);
    out_ << (status.ok() ? "Категория удалена." : status.error) << "\n";
}

}  // namespace xpay
