// Файл: wasm_main.cpp
// Назначение: вход в ядро из JavaScript при сборке в WebAssembly.
// Состояние приложения живёт в памяти модуля, страница шлёт команды строками JSON.

#include <string>

#include "xpay/api.hpp"
#include "xpay/model.hpp"

namespace {

xpay::State& state() {
    static xpay::State instance = xpay::default_state();
    return instance;
}

// Ответ хранится здесь до следующего вызова: JavaScript получает указатель
// и должен успеть скопировать строку.
std::string& response() {
    static std::string buffer;
    return buffer;
}

}  // namespace

extern "C" {

/// Выполняет команду. request указывает на строку JSON в UTF-8 с нулём в конце.
/// Возвращает указатель на строку ответа в том же виде.
const char* xpay_call(const char* request);

const char* xpay_call(const char* request) {
    response() = xpay::handle_request(state(), request);
    return response().c_str();
}

}  // extern "C"
