// Файл: telegram.js
// Назначение: обёртка над Telegram WebApp API. Весь код, который знает
// о Telegram, собран здесь; вне Telegram те же функции работают через браузер.

// Версии Bot API, в которых появились используемые методы.
const VERSION_COLORS = '6.1';
const VERSION_DIALOGS = '6.2';
const VERSION_CLOUD_STORAGE = '6.9';

const LOCAL_PREFIX = 'xpay:';

const webApp = globalThis.Telegram?.WebApp ?? null;

// Скрипт Telegram создаёт объект WebApp и в обычном браузере,
// но строка initData заполнена только при запуске внутри Telegram.
export const insideTelegram = webApp !== null && webApp.initData !== '';

function supports(version) {
  return insideTelegram && webApp.isVersionAtLeast(version);
}

// Имя пользователя Telegram для приветствия. Вне Telegram пустая строка.
// Данные из initDataUnsafe не проверены подписью, поэтому годятся только для показа.
export const userName = insideTelegram ? (webApp.initDataUnsafe?.user?.first_name ?? '') : '';

// Сообщает Telegram, что страница готова, разворачивает её на всю высоту
// и красит шапку клиента в цвет фона приложения.
export function start(backgroundColor) {
  if (!insideTelegram) {
    return;
  }
  webApp.ready();
  webApp.expand();
  if (webApp.isVersionAtLeast(VERSION_COLORS)) {
    webApp.setHeaderColor(backgroundColor);
    webApp.setBackgroundColor(backgroundColor);
  }
}

export function showAlert(message) {
  return new Promise((resolve) => {
    if (supports(VERSION_DIALOGS)) {
      webApp.showAlert(message, resolve);
    } else {
      globalThis.alert(message);
      resolve();
    }
  });
}

export function showConfirm(message) {
  return new Promise((resolve) => {
    if (supports(VERSION_DIALOGS)) {
      webApp.showConfirm(message, resolve);
    } else {
      resolve(globalThis.confirm(message));
    }
  });
}

// Короткая вибрация: kind равен 'success', 'warning' или 'error'.
export function haptic(kind) {
  if (insideTelegram) {
    webApp.HapticFeedback?.notificationOccurred(kind);
  }
}

function cloudCall(method, ...args) {
  return new Promise((resolve, reject) => {
    webApp.CloudStorage[method](...args, (error, result) => {
      if (error) {
        reject(new Error(String(error)));
      } else {
        resolve(result);
      }
    });
  });
}

// Облачное хранилище Telegram: данные привязаны к паре «пользователь, бот»
// и видны на всех устройствах пользователя.
const cloudStore = {
  // Для отсутствующего ключа Telegram возвращает пустую строку.
  get: async (key) => (await cloudCall('getItem', key)) || null,
  set: (key, value) => cloudCall('setItem', key, value),
  remove: (keys) => cloudCall('removeItems', keys),
};

// Хранилище браузера: для запуска вне Telegram и для старых клиентов.
const localStore = {
  get: async (key) => globalThis.localStorage.getItem(LOCAL_PREFIX + key),
  set: async (key, value) => globalThis.localStorage.setItem(LOCAL_PREFIX + key, value),
  remove: async (keys) => keys.forEach((key) => globalThis.localStorage.removeItem(LOCAL_PREFIX + key)),
};

export const usesCloud = supports(VERSION_CLOUD_STORAGE);

export const store = usesCloud ? cloudStore : localStore;
