// Файл: app.hpp
// Назначение: консольное приложение: меню, ввод с проверкой и вывод таблиц.
// Расчётов здесь нет, они в остальных модулях.

#pragma once

#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpay/ledger.hpp"
#include "xpay/model.hpp"

namespace xpay {

class App {
public:
    /// Возвращает текущий момент «ГГГГ-ММ-ДДTЧЧ:ММ».
    using Clock = std::function<std::string()>;

    /// Потоки и часы передаются снаружи, чтобы сценарии можно было
    /// проверять в тестах без клавиатуры и без зависимости от даты запуска.
    App(std::istream& in, std::ostream& out, State& state, Clock clock);

    /// Показывает главное меню, пока пользователь не выберет выход
    /// или не закончится ввод.
    void run();

private:
    // Функции ввода возвращают пустое значение, когда ввод закончился
    // или пользователь отменил действие.
    std::optional<std::string> read_line(std::string_view prompt);
    std::optional<int> read_choice(std::string_view prompt, int min, int max);
    std::optional<Money> read_amount(std::string_view prompt, std::optional<Money> fallback);
    std::optional<std::string> read_date(std::string_view prompt, const std::string& fallback);
    std::optional<std::string> read_time(const std::string& fallback);
    std::optional<bool> read_yes_no(std::string_view prompt);
    std::optional<Kind> read_kind(std::optional<Kind> fallback);
    std::optional<std::string> read_category(Kind kind, const std::string& fallback);
    std::optional<Filter> read_period();
    std::optional<Filter> read_filter();
    std::optional<Draft> read_draft(const Operation* current);

    void print_operations(const std::vector<Operation>& operations);
    void print_limit_warning(const Operation& operation);
    void print_limits();
    void print_categories();

    void operations_menu();
    void add_operation_flow();
    void edit_operation_flow();
    void remove_operation_flow();
    void reports_menu();
    void statistics_flow();
    void forecast_flow();
    void export_flow();
    void settings_menu();
    void set_limit_flow();
    void clear_limit_flow();
    void categories_menu();
    void add_category_flow();
    void remove_category_flow();

    std::istream& in_;
    std::ostream& out_;
    State& state_;
    Clock clock_;
    /// Ввод закончился: все меню должны завершиться, а не спрашивать снова.
    bool input_closed_ = false;
};

}  // namespace xpay
