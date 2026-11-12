// Файл: console.cpp
// Назначение: включает UTF-8 в консоли Windows. Без этого русский текст
// программы выводится в кодировке консоли и не читается.

#include "xpay/console.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace xpay {

void setup_console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

}  // namespace xpay
