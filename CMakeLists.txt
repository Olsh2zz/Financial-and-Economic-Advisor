# Файл: CMakeLists.txt
# Назначение: сборка проекта: библиотека с логикой, консольная программа и тесты.
# Модуль WebAssembly для мини-приложения собирается отдельно: tools/build_wasm.sh.

cmake_minimum_required(VERSION 3.16)
project(xpay VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Строгие предупреждения. /utf-8 сообщает компилятору Microsoft, что исходники
# и строки программы записаны в UTF-8: без него русский текст выводится неверно.
function(xpay_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
  endif()
endfunction()

add_library(xpay_core STATIC
  src/text.cpp
  src/money.cpp
  src/datetime.cpp
  src/model.cpp
  src/ledger.cpp
  src/report.cpp
  src/budget.cpp
  src/json.cpp
  src/storage.cpp
  src/file_store.cpp
  src/api.cpp
  src/app.cpp
  src/console.cpp
)
target_include_directories(xpay_core PUBLIC include)
xpay_warnings(xpay_core)

add_executable(xpay app/main.cpp)
target_link_libraries(xpay PRIVATE xpay_core)
xpay_warnings(xpay)

add_executable(xpay_tests
  tests/test_main.cpp
  tests/test_money.cpp
  tests/test_ledger.cpp
  tests/test_report.cpp
  tests/test_storage.cpp
  tests/test_app.cpp
)
target_link_libraries(xpay_tests PRIVATE xpay_core)
xpay_warnings(xpay_tests)

enable_testing()
add_test(NAME xpay_tests COMMAND xpay_tests)
