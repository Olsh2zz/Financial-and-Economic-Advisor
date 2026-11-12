#!/usr/bin/env bash
# Файл: build_wasm.sh
# Назначение: собирает ядро C++ в WebAssembly для мини-приложения Telegram.
# Нужен компилятор Emscripten (команда em++). Запуск из корня проекта:
#   bash tools/build_wasm.sh
# Результат: miniapp/wasm/xpay.wasm. Этот файл лежит в репозитории,
# пересобирать его нужно только после правок в src или include.

set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p miniapp/wasm

# Консольное меню и работа с диском в модуль не входят: в браузере
# нет ни консоли, ни файловой системы.
SOURCES=(
  src/text.cpp src/money.cpp src/datetime.cpp src/model.cpp src/ledger.cpp
  src/report.cpp src/budget.cpp src/json.cpp src/storage.cpp src/api.cpp
  wasm/wasm_main.cpp
)

# STANDALONE_WASM и --no-entry дают один файл .wasm без сгенерированной
# обвязки на JavaScript: загрузчик написан вручную в miniapp/js/core/engine.js.
em++ -std=c++17 -O2 -fno-exceptions -Wall -Wextra -Iinclude "${SOURCES[@]}" \
  -o miniapp/wasm/xpay.wasm \
  -sSTANDALONE_WASM --no-entry \
  -sEXPORTED_FUNCTIONS=_xpay_call,_malloc,_free \
  -sALLOW_MEMORY_GROWTH=1

ls -l miniapp/wasm/xpay.wasm
