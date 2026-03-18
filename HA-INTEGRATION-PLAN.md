# HA MQTT Integration — Plan

## Цель
Настройка подключения к локальному MQTT брокеру (Home Assistant) через портал.
Анонимный доступ работает без настройки. Авторизация — опционально.

---

## Прошивка (`idryer-link`)

- [ ] **NVS**: сохранять `ha_host`, `ha_user`, `ha_pass` (`src/main.cpp` / Preferences)
- [ ] **`initHomeAssistant()`**: читать host из NVS, дефолт `homeassistant.local`  (`src/IdryerDevice.cpp`)
- [ ] **Новая MQTT команда `configure_ha`**: принять `{host, user, pass}`, сохранить в NVS, переподключиться к HA (`lib/idryer-protocol/src/cloud/command_handler.cpp`)

---

## Backend (`iDryerPortal/backend`)

- [ ] **Новый MQTT command**: публикация `configure_ha` на топик устройства (`src/devices/devices.service.ts`)
- [ ] **Новый endpoint**: `POST /devices/:id/configure-ha` с `{host, user, pass}` (`src/devices/devices.controller.ts`)

---

## Frontend (`iDryerPortal/frontend`)

- [ ] **Шаг в `DeviceClaimDialog.tsx`**: после `device:activated` — опциональный шаг с формой
  Поля: Host (default: `homeassistant.local`), Username, Password
  Кнопки: "Настроить" / "Пропустить"

- [ ] **Кнопка в `LinkModuleBlock` (`DeviceShow.tsx`)**: "Configure Home Assistant"
  Открывает тот же диалог с формой (для повторной настройки)

---

## Заметки
- Без авторизации: форму можно пропустить, всё работает
- Host обязателен (есть дефолт), user/pass опциональны
- После сохранения устройство само переподключается к брокеру (или после перезагрузки, как проще, удобнее и безошибочнее)
