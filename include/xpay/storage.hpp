// Файл: storage.hpp
// Назначение: сохранение состояния: перевод в JSON и обратно с проверкой
// и работа с файлом данных на диске.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xpay/json.hpp"
#include "xpay/model.hpp"
#include "xpay/result.hpp"

namespace xpay {

inline constexpr int kFormatVersion = 1;

/// Операция и список названий в виде JSON. Тот же вид у них в файле данных
/// и в ответах ядра мини-приложению.
Json to_json(const Operation& item);
Json to_json(const std::vector<std::string>& names);

/// Записывает состояние в JSON. pretty включает отступы для чтения глазами.
std::string serialize(const State& state, bool pretty = false);

/// Разбирает JSON и проверяет каждое поле. Испорченные данные не чинит,
/// а возвращает ошибку, чтобы программа не затёрла их пустым состоянием.
Result<State> deserialize(std::string_view text);

struct LoadOutcome {
    State state;
    bool existed = false;  ///< false: файла нет, state содержит состояние первого запуска.
    std::string error;     ///< Непустая строка: файл есть, но прочитать его не удалось.
};

LoadOutcome load_file(const std::string& path);

/// Пишет во временный файл и переименовывает его: при сбое посреди записи
/// прежний файл данных остаётся целым.
Status save_file(const std::string& path, const State& state);

/// Записывает произвольный текст в файл (для экспорта отчёта).
Status write_text_file(const std::string& path, std::string_view text);

}  // namespace xpay
