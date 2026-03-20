# HA MQTT Integration — Plan

## Цель
Настройка подключения к локальному MQTT брокеру (Home Assistant) через портал.
Анонимный доступ работает без настройки. Авторизация — опционально.

---

## Прошивка (`idryer-link`)

- [x] **NVS**: сохранять `ha_host`, `ha_user`, `ha_pass` (namespace `"ha"`, `src/IdryerDevice.cpp`)
- [x] **`initHomeAssistant(host, user, pass)`**: читать из NVS при `CloudState::Online`, дефолт mDNS (`src/IdryerDevice.cpp`)
- [x] **Новая MQTT команда `configure_ha`**: принять `{host, user, pass}`, сохранить в NVS, переподключиться к HA (`src/IdryerDevice.cpp::handleMqttCommand`)

---

## Backend (`iDryerPortal/backend`)

- [x] **Новый MQTT command**: публикация `configure_ha` на топик `idryer/${mqttKey}/commands/configure_ha` (`src/devices/devices.service.ts::configureHA`)
- [x] **Новый endpoint**: `POST /devices/:id/configure-ha` с `{host, user, pass}` (`src/devices/devices.controller.ts`)

---

## Frontend (`iDryerPortal/frontend`)

- [x] **Шаг `'ha'` в `DeviceClaimDialog.tsx`**: после `device:activated` → "Завершить" → шаг HA
  Поля: Host (default: `homeassistant.local`), Username, Password
  Кнопки: "Пропустить" / "Настроить"

- [x] **Кнопка "Настроить Home Assistant" в `LinkModuleBlock` (`DeviceShow.tsx`)**
  Открывает диалог с формой (host/user/pass) для повторной настройки

---

## Заметки
- Без авторизации: форму можно пропустить, всё работает
- Host обязателен (есть дефолт), user/pass опциональны
- После сохранения устройство само переподключается к брокеру (или после перезагрузки, как проще, удобнее и безошибочнее)
