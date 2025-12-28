# Архитектура библиотеки idryer-protocol

## Принцип: Platform Abstraction Pattern

Библиотека должна работать одинаково на **Arduino Framework** и **ESP-IDF**.

### Структура:

```
lib/idryer-protocol/src/
├── uart/           # UART протокол (универсальный)
├── mqtt/           # MQTT клиент (универсальный)
└── device/         # Оркестратор логики
    ├── idryer_device.h        # Главный класс
    ├── platform_interfaces.h  # Интерфейсы (IWifiManager, IHttpClient, ICredentialStore)
    └── device_identity.h      # Структуры данных

examples/
├── arduino/        # Arduino Framework реализации
│   ├── arduino_platform.h     # ArduinoWifiManager, ArduinoHttpClient, ArduinoCredentials
│   └── full_example.ino
└── espidf/         # ESP-IDF реализации
    ├── espidf_platform.h      # EspIdfWifiManager, EspIdfHttpClient, EspIdfCredentials
    └── main.cpp
```

### Использование:

**Arduino:**
```cpp
#include <idryer_protocol.h>
#include <idryer_arduino_platform.h>

ArduinoWifiManager wifi;
ArduinoHttpClient http;
ArduinoCredentials creds;
IdryerDevice device;

void setup() {
  device.begin(&wifi, &http, &creds);
}

void loop() {
  device.loop();
}
```

**ESP-IDF:**
```cpp
#include <idryer_protocol.h>
#include <idryer_espidf_platform.h>

EspIdfWifiManager wifi;
EspIdfHttpClient http;
EspIdfCredentials creds;
IdryerDevice device;

void app_main() {
  device.begin(&wifi, &http, &creds);
  while(1) { device.loop(); vTaskDelay(10); }
}
```

### Что в библиотеке (универсально):
- UART протокол
- MQTT клиент
- State machine (provision → register → claim → mqtt)
- Интерфейсы платформ
- Структуры данных

### Что в примерах (платформо-специфично):
- WiFi менеджер
- HTTP клиент
- Хранилище credentials (NVS/Preferences)

## TODO:
- [ ] Рефакторинг NetworkManager → IdryerDevice + интерфейсы
- [ ] Перенос CredentialStore/DeviceIdentity в правильные места
- [ ] Создание примеров для Arduino и ESP-IDF
