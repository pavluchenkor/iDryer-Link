# REVIEW PLAN: аудит качества кода и комментариев

Дата: 2026-03-03
Область анализа:
- `src`
- `lib/idryer-protocol` (включая `src`, `examples`, `docs`, `docs_old`, метаданные)

Важно: по вашему запросу сборка/тесты не запускались; вывод основан на статическом ревью кода и структуры файлов.

## 1) Итоговое заключение
- Качество кода: **6.8/10**
- Качество комментариев: **6.2/10**
- Баланс комментариев: **перекос в избыточность в заголовках**, при этом в нескольких рискованных местах не хватает точных комментариев про ограничения/контракты.

Сильные стороны:
- Хорошая модульность (`cloud`, `mqtt`, `uart`, `platform`, `hal`).
- Внятные интерфейсы абстракций (`IWifiManager`, `IHttpClient`, `ICredentialStore`).
- Много защит от переполнений (`strncpy` + `\0`, проверки размеров).
- UART слой достаточно зрелый: CRC, ACK/retry, декодер кадров.

Ключевые риски:
- Есть функциональные ошибки в runtime-логике (время UTC, claim-ветвление, IP packing).
- Есть безопасность/transport риск (`setInsecure()` в HTTP клиенте).
- Примеры `examples/*.ino` частично не синхронизированы с актуальным API (риск ложных интеграций).
- Часть комментариев устарела или дублирует код, усложняя сопровождение.

---

## 2) Критичные и высокие замечания

1. `src/IdryerDevice.cpp`
- `parseIpAddress()` формирует `uint32_t` не в network byte order, хотя комментарий утверждает обратное.
- Риск: RP2040/GUI может получать неверный IP в `HelloAckPayload.ipAddress`.

2. `src/IdryerDevice.cpp`
- `syncTimeFromBackend()` парсит UTC (`...Z`), но использует `mktime()` (локальное время), что может дать смещение.
- Нужен UTC-safe путь (`timegm`/эквивалент).

3. `src/main.cpp` + `src/IdryerDevice.cpp`
- В `handleWebSerialCommand()` логика ожидает, что `requestClaimProcess()` вернёт `true` даже когда устройство уже claimed.
- Фактически `CloudStateMachine::requestClaim()` возвращает `false` в случае already claimed.
- Следствие: возможный ложный ответ `CLAIM_STARTED:ERROR` вместо явного статуса already claimed.

4. `src/WsServer.cpp`
- `sendConfig()` парсит config в `DynamicJsonDocument(2048)`.
- Полный config часто больше 2KB, что приведёт к parse error и потере WS-публикации конфига.

5. `lib/idryer-protocol/src/platform/arduino/ArduinoHttpClient.cpp`
- Используется `WiFiClientSecure::setInsecure()` (явно отмечено TODO).
- Это отключает проверку TLS сертификата для API-запросов.

6. `lib/idryer-protocol/src/mqtt/mqtt_client.cpp`
- Библиотека включает `secrets.h` напрямую (для library-кода это плохая связка с приложением).
- Внешняя интеграция библиотеки без этого файла может ломаться.

7. `lib/idryer-protocol/examples/rp2040_dryer_controller/rp2040_dryer_controller.ino`
- Пример не синхронизирован с актуальными enum/полями (`Role::RpController`, `CommandCode::Pause`, `ErrorCode::InvalidState`, `humidityPct`).
- Риск: пример вводит в заблуждение и, вероятно, не компилируется в текущем API.

8. `lib/idryer-protocol/src/config/config_manager.h`
- `ConfigReceiver` не валидирует `totalSize` на момент `LAST_FRAGMENT` (нет проверки `received == total`).
- Возможен приём неполного/лишнего набора фрагментов как complete.

---

## 3) Качество комментариев: где избыток, где недостаток

Избыточно:
- `lib/idryer-protocol/src/mqtt/root_ca.h` — очень длинные пояснения про PEM/PROGMEM, несоразмерно полезности.
- `lib/idryer-protocol/src/hal/hal_types.h`, `hal_arduino.h`, `src/IdryerDevice.h` — много комментариев повторяют очевидное из сигнатур.
- В ряде хедеров большие блоки «примеров» увеличивают шум и цену поддержки.

Недостаточно:
- `src/IdryerDevice.cpp` (конвертация времени и `vals -> d`) — не описаны ограничения буфера и fallback-поведение.
- `src/WsServer.cpp` (`sendConfig`) — не задокументирован лимит JSON-документа/поведение на oversized payload.
- `lib/idryer-protocol/src/config/config_manager.h` — нет явного комментария про требования строгого соответствия `totalSize`.

Устаревшие/неточные комментарии:
- `lib/idryer-protocol/src/uart/uart_protocol.h`: в шапке упомянут ESP8266, хотя текущий проект — ESP32.
- Часть ссылок в README ведёт на legacy-пути (`docs/mqtt-api-kit/...` вместо актуальной структуры).

---

## 4) Детальный аудит: папка за папкой, файл за файлом

### 4.1 `src`

- `src/main.cpp`
  - Код: **B**
  - Комментарии: **B-**
  - Вывод: архитектурно понятный entrypoint, но часть переменных (`currentClaimPin`, `claimPinExpiresIn`) не используется по назначению.

- `src/IdryerDevice.h`
  - Код: **B**
  - Комментарии: **C+**
  - Вывод: контракт фасада читаемый, но комментариев заметно больше, чем нужно для поддержки.

- `src/IdryerDevice.cpp`
  - Код: **B-**
  - Комментарии: **B-**
  - Вывод: сильная интеграционная логика, но есть важные баги в обработке времени/claim/IP.

- `src/WsServer.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: хороший интерфейс; комментарии в целом по делу.

- `src/WsServer.cpp`
  - Код: **B-**
  - Комментарии: **B-**
  - Вывод: рабочий WS-слой, но есть жёсткие лимиты JSON и недостаток fail-safe поведения.

- `src/version.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: практичный механизм синхронизации версии с MCU, но нужно следить за макро-переопределениями.

### 4.2 `lib/idryer-protocol/src/cloud`

- `lib/idryer-protocol/src/cloud/cloud_state_machine.h`
  - Код: **B**
  - Комментарии: **C+**
  - Вывод: хороший каркас FSM, но комментарии частично избыточны.

- `lib/idryer-protocol/src/cloud/cloud_state_machine.cpp`
  - Код: **B-**
  - Комментарии: **B-**
  - Вывод: рабочая state machine; есть логические шероховатости (`unclaimed` callback может дублироваться).

- `lib/idryer-protocol/src/cloud/command_handler.h`
  - Код: **B**
  - Комментарии: **B-**
  - Вывод: ясный API обработчика команд.

- `lib/idryer-protocol/src/cloud/command_handler.cpp`
  - Код: **B-**
  - Комментарии: **B**
  - Вывод: покрывает ключевые команды, но есть TODO и неполная фрагментация для `set/invoke`.

- `lib/idryer-protocol/src/cloud/http_api.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: чистый контракт API-слоя.

- `lib/idryer-protocol/src/cloud/http_api.cpp`
  - Код: **B-**
  - Комментарии: **B**
  - Вывод: понятная реализация; желательно жёстче валидировать обязательные поля ответа.

- `lib/idryer-protocol/src/cloud/telemetry_publisher.h`
  - Код: **B**
  - Комментарии: **C+**
  - Вывод: интерфейс хороший, но много повторяющих комментариев.

- `lib/idryer-protocol/src/cloud/telemetry_publisher.cpp`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: корректный mapper UART->JSON, читается хорошо.

### 4.3 `lib/idryer-protocol/src/config`

- `lib/idryer-protocol/src/config/config_manager.h`
  - Код: **B+**
  - Комментарии: **B+**
  - Вывод: хороший утилитарный модуль; нужна доп. валидация `totalSize`/целостности на completion.

### 4.4 `lib/idryer-protocol/src/core`

- `lib/idryer-protocol/src/core/config.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: полезный централизованный конфиг, адекватные `static_assert`.

- `lib/idryer-protocol/src/core/types.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: аккуратные value-типы без `String`, хорошая база для embedded.

### 4.5 `lib/idryer-protocol/src/device/interfaces`

- `lib/idryer-protocol/src/device/interfaces/DeviceIdentity.h`
  - Код: **A-**
  - Комментарии: **B**
  - Вывод: корректный compatibility shim.

- `lib/idryer-protocol/src/device/interfaces/ICredentialStore.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: интерфейс минимальный и практичный.

- `lib/idryer-protocol/src/device/interfaces/IHttpClient.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: контракт достаточен для абстракции transport-слоя.

- `lib/idryer-protocol/src/device/interfaces/IWifiManager.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: хороший интерфейс, покрывает основные сценарии.

### 4.6 `lib/idryer-protocol/src/hal`

- `lib/idryer-protocol/src/hal/hal_types.h`
  - Код: **B+**
  - Комментарии: **C+**
  - Вывод: функционально хорошо, но комментарии слишком многословны.

- `lib/idryer-protocol/src/hal/hal_arduino.h`
  - Код: **B**
  - Комментарии: **C+**
  - Вывод: полезные адаптеры, но коммент-слой перегружен.

- `lib/idryer-protocol/src/hal/hal_arduino.cpp`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: чистая реализация, управление lifecycle логгера понятное.

### 4.7 `lib/idryer-protocol/src/mqtt`

- `lib/idryer-protocol/src/mqtt/idryer_topics.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: нормальный набор topic-констант и helper-функций.

- `lib/idryer-protocol/src/mqtt/mqtt_client.h`
  - Код: **B-**
  - Комментарии: **C+**
  - Вывод: интерфейс богатый, но документация местами не синхронизирована с реализацией QoS/flow.

- `lib/idryer-protocol/src/mqtt/mqtt_client.cpp`
  - Код: **C+**
  - Комментарии: **C**
  - Вывод: рабочий, но содержит технический долг (динам. аллокации, include `secrets.h`, фактически неиспользуемый QoS, блокирующие `delay`).

- `lib/idryer-protocol/src/mqtt/root_ca.h`
  - Код: **A-**
  - Комментарии: **D**
  - Вывод: сертификат ок, комментарии сильно избыточны и зашумляют файл.

### 4.8 `lib/idryer-protocol/src/platform/arduino`

- `lib/idryer-protocol/src/platform/arduino/ArduinoCredentialStore.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: простой и понятный интерфейс.

- `lib/idryer-protocol/src/platform/arduino/ArduinoCredentialStore.cpp`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: корректная NVS-обёртка.

- `lib/idryer-protocol/src/platform/arduino/ArduinoHttpClient.h`
  - Код: **B-**
  - Комментарии: **B**
  - Вывод: контракт достаточен, но security caveat прямо в описании.

- `lib/idryer-protocol/src/platform/arduino/ArduinoHttpClient.cpp`
  - Код: **C**
  - Комментарии: **B**
  - Вывод: основная функциональность есть, но `setInsecure()` делает transport небезопасным.

- `lib/idryer-protocol/src/platform/arduino/ArduinoWifiManager.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: нормальный API слоя WiFi.

- `lib/idryer-protocol/src/platform/arduino/ArduinoWifiManager.cpp`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: адекватная реализация; повторные `WiFi.begin()` в retry стоит контролировать по таймингам.

- `lib/idryer-protocol/src/platform/arduino/idryer_arduino.h`
  - Код: **A-**
  - Комментарии: **B**
  - Вывод: удобный агрегирующий include.

### 4.9 `lib/idryer-protocol/src/uart`

- `lib/idryer-protocol/src/uart/uart_protocol.h`
  - Код: **B**
  - Комментарии: **B-**
  - Вывод: хорошая спецификация, но есть устаревшие формулировки/наименования в комментариях.

- `lib/idryer-protocol/src/uart/uart_protocol.cpp`
  - Код: **A-**
  - Комментарии: **B**
  - Вывод: короткая корректная CRC16 реализация.

- `lib/idryer-protocol/src/uart/uart_bridge.h`
  - Код: **B+**
  - Комментарии: **B**
  - Вывод: богатый и полезный API, хорошо структурирован.

- `lib/idryer-protocol/src/uart/uart_bridge.cpp`
  - Код: **B**
  - Комментарии: **B-**
  - Вывод: сильный модуль, но сложный и чувствительный к таймингам; есть workaround-задержки, которые требуют документированного обоснования и тестов.

### 4.10 `lib/idryer-protocol/src/idryer_protocol.h`

- `lib/idryer-protocol/src/idryer_protocol.h`
  - Код: **B**
  - Комментарии: **B**
  - Вывод: удобная «точка входа» библиотеки.

### 4.11 `lib/idryer-protocol/examples`

- `lib/idryer-protocol/examples/mqtt_basic/mqtt_basic.ino`
  - Код: **C+**
  - Комментарии: **B**
  - Вывод: демонстрационный файл полезный, но частично устарел относительно текущих команд/полей.

- `lib/idryer-protocol/examples/mqtt_basic/README.md`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: документация примера полезна, нужна синхронизация с актуальным API.

- `lib/idryer-protocol/examples/uart_esp32_bridge/uart_esp32_bridge.ino`
  - Код: **C**
  - Комментарии: **B**
  - Вывод: содержит несоответствия структурам payload (риск нерабочего примера).

- `lib/idryer-protocol/examples/uart_esp32_bridge/README.md`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: читаемо, но проверить соответствие реальному API.

- `lib/idryer-protocol/examples/rp2040_dryer_controller/rp2040_dryer_controller.ino`
  - Код: **C-**
  - Комментарии: **B-**
  - Вывод: пример требует обновления под текущие enum/структуры и командный протокол.

- `lib/idryer-protocol/examples/rp2040_dryer_controller/README.md`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: информативно, но проверка на синхронность с кодом обязательна.

### 4.12 `lib/idryer-protocol` (корень/метаданные)

- `lib/idryer-protocol/LICENSE`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: корректно.

- `lib/idryer-protocol/README.md`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: хорошее вводное описание, но есть устаревшие пути/ссылки.

- `lib/idryer-protocol/library.json`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: метаданные в целом ок, но URL репозитория/домена стоит сверить с фактическим origin.

- `lib/idryer-protocol/library.properties`
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: минимально и рабоче.

### 4.13 `lib/idryer-protocol/docs` и `docs_old`

- `lib/idryer-protocol/docs/*` (все `.md` + `all-in-one-protocol.csv`)
  - Код: **N/A**
  - Комментарии в коде: **N/A**
  - Вывод: документация объёмная и полезная; пригодна для интеграции, но требует периодической проверки на совпадение с реализацией.

- `lib/idryer-protocol/docs_old/*` (все `.md/.csv` + `docs_old/examples/error_defs.h`)
  - Код: **частично legacy**
  - Комментарии: **смешанное качество**
  - Вывод: папка явно архивная; есть риск, что разработчики будут ориентироваться на устаревшие схемы. Желательно явное маркирование как deprecated + ссылки на актуальные документы.

- `lib/idryer-protocol/docs_old/examples/error_defs.h`
  - Код: **C**
  - Комментарии: **C**
  - Вывод: legacy-шаблон, неполный/закомментированный блок, лучше держать как reference-only.

### 4.14 Технический мусор в дереве

- `lib/idryer-protocol/.DS_Store`, `lib/idryer-protocol/docs/.DS_Store`, `lib/idryer-protocol/docs_old/mqtt-api-kit/.DS_Store`
  - Вывод: лишние артефакты macOS, ухудшают чистоту репозитория.

- `lib/idryer-protocol/.claude/settings.local.json`
  - Вывод: локальный инструментальный файл, должен быть явно исключён из релизного артефакта.

- `lib/idryer-protocol/.git`, `lib/idryer-protocol/.gitignore`
  - Вывод: ожидаемо для submodule.

---

## 5) Рекомендации по улучшению

1. Исправить runtime-баги в `IdryerDevice.cpp`:
- корректное преобразование IP в network order;
- UTC-safe синхронизация времени;
- унификация статусов claim (`already claimed` vs `error`).

2. Усилить безопасность transport-слоя:
- убрать `setInsecure()` в `ArduinoHttpClient.cpp`;
- использовать проверку сертификата/пиннинг.

3. Починить WS config-путь:
- увеличить буфер/потоковый режим для `sendConfig()`;
- явный graceful fallback при oversized JSON.

4. Обновить и синхронизировать примеры в `examples/` с текущим API.

5. Сократить избыточные комментарии в больших хедерах:
- оставить комментарии только для нетривиальной логики и контрактов.

6. Явно развести `docs` и `docs_old`:
- в `docs_old/README` добавить предупреждение, что папка архивная;
- в актуальных документах дать прямые ссылки на “источник истины”.

