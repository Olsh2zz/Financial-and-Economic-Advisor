// Файл: api.hpp
// Назначение: вход в ядро для мини-приложения Telegram. Запрос и ответ
// передаются строками JSON, потому что через границу WebAssembly проходят
// только числа и байты.

#pragma once

#include <string>
#include <string_view>

#include "xpay/model.hpp"

namespace xpay {

/// Выполняет команду из запроса вида {"cmd": "add", ...} над состоянием state.
/// Ответ: {"ok": true, ...} или {"ok": false, "error": "текст для пользователя"}.
/// При ошибке состояние не меняется. Список команд описан в api.cpp.
std::string handle_request(State& state, std::string_view request);

}  // namespace xpay
