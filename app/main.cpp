// Файл: main.cpp
// Назначение: точка входа консольной программы: загружает данные из файла,
// запускает меню и сохраняет данные при выходе.

#include <iostream>
#include <string>

#include "xpay/app.hpp"
#include "xpay/console.hpp"
#include "xpay/datetime.hpp"
#include "xpay/storage.hpp"

namespace {

// Файл данных лежит в папке, из которой запущена программа.
constexpr const char* kDefaultDataFile = "finance.json";

}  // namespace

int main(int argc, char* argv[]) {
    xpay::setup_console();
    // Первым аргументом можно передать путь к другому файлу данных.
    const std::string path = argc > 1 ? argv[1] : kDefaultDataFile;

    xpay::LoadOutcome loaded = xpay::load_file(path);
    if (!loaded.error.empty()) {
        // Работать дальше нельзя: при выходе пустое состояние затёрло бы файл.
        std::cerr << loaded.error << "\nИсправьте файл или уберите его из папки и запустите программу снова.\n";
        return 1;
    }
    std::cout << (loaded.existed ? "Данные загружены из файла «" : "Файла данных пока нет, он появится при выходе: «")
              << path << "».\n";

    xpay::App app(std::cin, std::cout, loaded.state, xpay::current_stamp);
    app.run();

    const xpay::Status saved = xpay::save_file(path, loaded.state);
    if (!saved.ok()) {
        std::cerr << saved.error << "\n";
        return 1;
    }
    std::cout << "Данные сохранены в файл «" << path << "». До свидания!\n";
    return 0;
}
