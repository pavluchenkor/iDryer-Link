#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * UART протокол между RP2040 (основной контроллер) и ESP8266 (сетевой мост).
 * Физика: 115200 бод, 8N1, без аппаратного контроля потока. Стороны держат
 * отдельную очередь команд и ожидают подтверждение с тем же номером
 * последовательности.
 *
 * Кадр:
 * byte 0  : стартовый байт 0xAA
 * byte 1  : версия протокола (PROTOCOL_VERSION)
 * byte 2  : флаги (бит0 — требуется ACK, бит1 — это ACK, бит2 — ошибка, бит3 — фрагмент, бит4 — последний фрагмент)
 * byte 3  : тип сообщения (MessageKind)
 * byte 4  : номер последовательности (0..255, инкремент при каждом кадре)
 * byte 5  : длина полезной нагрузки (0..MAX_PAYLOAD_SIZE)
 * payload : данные
 * CRC16   : младший, затем старший байты (polynom 0x1021, init 0xFFFF)
 */
namespace DryerUart
{

  constexpr uint8_t SOF = 0xAA;
  constexpr uint8_t PROTOCOL_VERSION = 1;

  constexpr uint8_t FLAG_ACK_REQUIRED = 0x01;
  constexpr uint8_t FLAG_IS_ACK = 0x02;
  constexpr uint8_t FLAG_ERROR = 0x04;
  constexpr uint8_t FLAG_FRAGMENTED = 0x08;      // Fragment (not last)
  constexpr uint8_t FLAG_LAST_FRAGMENT = 0x10;   // Last fragment

  constexpr uint8_t MAX_PAYLOAD_SIZE = 200;
  constexpr uint8_t MAX_RETRIES = 3;
  constexpr uint16_t HEARTBEAT_INTERVAL_MS = 5000;
  constexpr uint16_t TELEMETRY_ACTIVE_INTERVAL_MS = 1000;
  constexpr uint16_t TELEMETRY_IDLE_INTERVAL_MS = 15000;
  constexpr uint16_t COMMAND_REPLY_TIMEOUT_MS = 700;
  constexpr uint32_t LINK_LOSS_TIMEOUT_MS = 20000;

  enum class Role : uint8_t
  {
    Rp2040Controller = 0x01,
    EspBridge = 0x02,
  };

  enum class MessageKind : uint8_t
  {
    Hello = 0x01,        // RP2040 -> ESP, несёт Role, версию прошивки
    HelloAck = 0x02,     // ESP -> RP2040, подтверждение и сетевой статус
    Telemetry = 0x10,    // RP2040 -> ESP, пакет TelemetryPayload
    TelemetryAck = 0x11, // ESP -> RP2040, подтверждение доставки
    Command = 0x20,      // ESP -> RP2040, пакет CommandPayload
    CommandAck = 0x21,   // RP2040 -> ESP, подтверждение / отказ
    ConfigPush = 0x30,   // ESP -> RP2040, набор целевых параметров
    ConfigAck = 0x31,
    Heartbeat = 0x40, // обе стороны, несёт uptime и уровни RSSI/питания
    Error = 0x50,     // несёт ErrorPayload
    Log = 0x60        // произвольные диагностические сообщения
  };

  enum class DryerState : uint8_t
  {
    Idle = 0,
    Preheat = 1,
    Drying = 2,
    Cooling = 3,
    Fault = 4,
    Service = 5,
  };

  enum class CommandCode : uint8_t
  {
    // MQTT команды (Backend → ESP → RP2040):
    Start = 0x01,      // Запуск режима (DRYING/STORAGE/PROFILE)
    Stop = 0x02,       // Остановка юнита
    GetConfig = 0x05,  // Запрос JSON конфига меню
    SetConfig = 0x06,  // Применить настройки из JSON
    WriteRfid = 0x08,  // Записать OpenPrintTag на RFID метку

    // Служебные команды (только UART):
    ResetFault = 0x10,  // Сброс ошибки
    WifiStatus = 0x11,  // RP2040 запрашивает IP адрес для отображения на экране
    ClearErrors = 0x12, // ESP32 → RP2040: пользователь подтвердил ошибки (сброс EEPROM лога)
  };

  enum class ErrorCode : uint8_t
  {
    None = 0x00,
    CrcMismatch = 0x01,
    UnknownMessage = 0x02,
    InvalidPayload = 0x03,
    Busy = 0x04,
    Timeout = 0x05,
    SequenceMismatch = 0x06,
  };

#pragma pack(push, 1)

  struct FrameHeader
  {
    uint8_t sof;
    uint8_t version;
    uint8_t flags;
    MessageKind kind;
    uint8_t sequence;
    uint8_t payloadLength;
  };

  struct Frame
  {
    FrameHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
  };

  struct HelloPayload
  {
    Role role;
    uint32_t firmwareVersion; // семантика MAJOR<<16 | MINOR<<8 | PATCH
    uint32_t capabilitiesMask;
  };

  struct TelemetryPayload
  {
    int16_t temperatureC10; // температура *10
    uint8_t humidityPct;
    uint8_t heaterPowerPct;
    uint8_t fanOn; // 0/1
    uint16_t filamentWeightGrams;
    DryerState state;
    uint16_t remainingMinutes;
    uint32_t jobId;
    uint32_t uptimeSeconds;
  };

  struct CommandPayload
  {
    CommandCode command;
    uint8_t targetState; // используется для StartDry/PushConfig
    uint32_t arg0;
    uint32_t arg1;
  };

  struct ConfigPayload
  {
    int16_t targetTemperatureC10;
    uint16_t targetHumidityPct;
    uint16_t durationMinutes;
    uint16_t fanDutyPct;
  };

  struct HeartbeatPayload
  {
    uint32_t uptimeSeconds;
    int16_t wifiRssiDbm; // ESP указывает RSSI, RP2040 передаёт температуру MCU
    uint16_t errorsSinceBoot;
  };

  struct AckPayload
  {
    uint8_t ackSequence;
    ErrorCode status;
  };

  struct ErrorPayload
  {
    ErrorCode code;
    uint8_t lastSequence;
    uint16_t detail; // например, ожидаемая длина, фактическая длина и т. д.
  };

  struct LogPayload
  {
    char severity[10];   // "critical", "error", "warning", "info"
    char source[20];     // "THERMISTOR", "HEATER", "SHT", "SERVO", etc.
    char event[32];      // "SENSOR_SHORT", "OVER_MAX", "NO_RESPONSE", etc.
    char message[100];   // "Thermistor short circuit", "Value over maximum", etc.
    uint8_t unitId;      // 0-3 (controller ID)
    uint8_t _pad;        // выравнивание
  };

#pragma pack(pop)

  static_assert(sizeof(FrameHeader) == 6, "Frame header must remain packed");
  static_assert(sizeof(AckPayload) <= MAX_PAYLOAD_SIZE,
                "Ack payload must fit in frame");
  static_assert(sizeof(HelloPayload) <= MAX_PAYLOAD_SIZE,
                "Hello payload must fit in frame");
  static_assert(sizeof(TelemetryPayload) <= MAX_PAYLOAD_SIZE,
                "Telemetry payload must fit in frame");
  static_assert(sizeof(CommandPayload) <= MAX_PAYLOAD_SIZE,
                "Command payload must fit in frame");
  static_assert(sizeof(ConfigPayload) <= MAX_PAYLOAD_SIZE,
                "Config payload must fit in frame");
  static_assert(sizeof(HeartbeatPayload) <= MAX_PAYLOAD_SIZE,
                "Heartbeat payload must fit in frame");
  static_assert(sizeof(ErrorPayload) <= MAX_PAYLOAD_SIZE,
                "Error payload must fit in frame");
  static_assert(sizeof(LogPayload) <= MAX_PAYLOAD_SIZE,
                "Log payload must fit in frame");

  inline bool requiresAck(uint8_t flags)
  {
    return (flags & FLAG_ACK_REQUIRED) != 0;
  }

  inline uint8_t makeAckFlags()
  {
    return FLAG_IS_ACK;
  }

  /**
   * Расчёт CRC16-CCITT (0x1021). Реализация добавляется при интеграции;
   * интерфейс вынесен для унификации RP2040 и ESP.
   */
  uint16_t calculateCrc(const uint8_t *data, size_t length);

} // namespace DryerUart
