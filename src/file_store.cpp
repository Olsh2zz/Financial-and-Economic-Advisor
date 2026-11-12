// Файл: file_store.cpp
// Назначение: чтение и запись файла данных и файлов экспорта на диске.

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "xpay/storage.hpp"

namespace xpay {

namespace {

// std::filesystem::u8path принимает путь в UTF-8 на любой системе:
// в Windows обычная строка была бы прочитана в кодировке консоли.
std::filesystem::path to_path(const std::string& path) {
    return std::filesystem::u8path(path);
}

}  // namespace

LoadOutcome load_file(const std::string& path) {
    LoadOutcome outcome;
    std::error_code code;
    if (!std::filesystem::exists(to_path(path), code)) {
        outcome.state = default_state();
        return outcome;
    }
    outcome.existed = true;
    std::ifstream file(to_path(path), std::ios::binary);
    if (!file) {
        outcome.error = "Не удалось открыть файл данных «" + path + "».";
        return outcome;
    }
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Result<State> parsed = deserialize(text);
    if (!parsed.ok()) {
        outcome.error = "Файл данных «" + path + "» повреждён. " + parsed.error;
        return outcome;
    }
    outcome.state = std::move(*parsed.value);
    return outcome;
}

Status write_text_file(const std::string& path, std::string_view text) {
    std::ofstream file(to_path(path), std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
    if (!file) {
        return {"Не удалось записать файл «" + path + "»."};
    }
    return {};
}

Status save_file(const std::string& path, const State& state) {
    const std::string temporary = path + ".tmp";
    Status written = write_text_file(temporary, serialize(state, true) + "\n");
    if (!written.ok()) {
        return written;
    }
    std::error_code code;
    std::filesystem::rename(to_path(temporary), to_path(path), code);
    if (code) {
        return {"Не удалось заменить файл данных «" + path + "»."};
    }
    return {};
}

}  // namespace xpay
