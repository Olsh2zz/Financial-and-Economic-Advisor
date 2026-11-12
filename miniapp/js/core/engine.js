// Файл: engine.js
// Назначение: загружает ядро приложения, собранное из C++ в WebAssembly
// (wasm/xpay.wasm), и передаёт ему команды. Все расчёты и проверки ввода
// выполняет ядро, тот же код работает в консольной программе.

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

// Функции, которые модуль ждёт от окружения. Обе нужны только
// стандартной библиотеке C++, ядро их напрямую не вызывает.
const IMPORTS = {
  env: {
    emscripten_notify_memory_growth: () => {},
  },
  wasi_snapshot_preview1: {
    proc_exit: (code) => {
      throw new Error(`Ядро аварийно завершилось с кодом ${code}.`);
    },
  },
};

// Создаёт ядро из байтов файла xpay.wasm.
// Возвращает объект с методом call(cmd, args): ответ ядра как объект
// { ok: true, ... } или { ok: false, error }. Список команд описан в src/api.cpp.
export async function createEngine(wasmBytes) {
  const { instance } = await WebAssembly.instantiate(wasmBytes, IMPORTS);
  const { memory, malloc, free, xpay_call: xpayCall, _initialize: initialize } = instance.exports;
  // Запускает конструкторы глобальных объектов C++.
  initialize?.();

  return {
    call(cmd, args = {}) {
      const request = textEncoder.encode(JSON.stringify({ ...args, cmd }));
      const requestPointer = malloc(request.length + 1);
      // Память модуля может вырасти при любом вызове, и прежний буфер
      // станет недействительным, поэтому memory.buffer читается заново.
      const input = new Uint8Array(memory.buffer);
      input.set(request, requestPointer);
      input[requestPointer + request.length] = 0;

      let responsePointer;
      try {
        responsePointer = xpayCall(requestPointer);
      } finally {
        free(requestPointer);
      }

      // Ответ лежит строкой с нулевым байтом в конце.
      const output = new Uint8Array(memory.buffer);
      let end = responsePointer;
      while (output[end] !== 0) {
        end += 1;
      }
      return JSON.parse(textDecoder.decode(output.subarray(responsePointer, end)));
    },
  };
}

// Загружает ядро по адресу файла .wasm (в браузере).
export async function loadEngine(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Не удалось загрузить ядро: ответ ${response.status}.`);
  }
  return createEngine(await response.arrayBuffer());
}
