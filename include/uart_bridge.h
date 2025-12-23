#pragma once

#include <Arduino.h>
#include <functional>

#include "uart_protocol.h"

namespace DryerUart {

class UartBridge {
 public:
  using HelloHandler =
      std::function<void(const HelloPayload &, const FrameHeader &)>;
  using TelemetryHandler =
      std::function<void(const TelemetryPayload &, const FrameHeader &)>;
  using CommandHandler =
      std::function<void(const CommandPayload &, const FrameHeader &)>;
  using ConfigHandler =
      std::function<void(const ConfigPayload &, const FrameHeader &)>;
  using CommandAckHandler =
      std::function<void(const AckPayload &, const FrameHeader &)>;
  using ConfigAckHandler =
      std::function<void(const AckPayload &, const FrameHeader &)>;
  using HeartbeatHandler =
      std::function<void(const HeartbeatPayload &, const FrameHeader &)>;
  using ErrorHandler =
      std::function<void(const ErrorPayload &, bool remote)>;
  using LogHandler =
      std::function<void(const uint8_t *payload, uint8_t length)>;

  void begin(HardwareSerial &serial, uint32_t baudRate = 115200);
  void loop();
  void reset();

  bool sendHello(const HelloPayload &payload, bool ackRequired = false);
  bool sendHelloAck(const HelloPayload &payload);
  bool sendTelemetry(const TelemetryPayload &payload,
                     bool ackRequired = false);
  bool sendCommand(const CommandPayload &payload, bool ackRequired = true);
  bool sendConfigPush(const ConfigPayload &payload, bool ackRequired = true);
  bool sendHeartbeat(const HeartbeatPayload &payload);
  bool sendError(const ErrorPayload &payload);
  bool sendTelemetryAck(uint8_t sequence, ErrorCode status = ErrorCode::None);
  bool sendCommandAck(uint8_t sequence, ErrorCode status = ErrorCode::None);
  bool sendConfigAck(uint8_t sequence, ErrorCode status = ErrorCode::None);
  bool sendLog(const uint8_t *message, uint8_t length);
  bool sendLog(const char *cstr);

  void setHelloHandler(const HelloHandler &handler);
  void setTelemetryHandler(const TelemetryHandler &handler);
  void setCommandHandler(const CommandHandler &handler);
  void setConfigHandler(const ConfigHandler &handler);
  void setCommandAckHandler(const CommandAckHandler &handler);
  void setConfigAckHandler(const ConfigAckHandler &handler);
  void setHeartbeatHandler(const HeartbeatHandler &handler);
  void setErrorHandler(const ErrorHandler &handler);
  void setLogHandler(const LogHandler &handler);

 private:
  enum class ParserState : uint8_t { WaitForSof, Header, Payload, Crc };

  struct ParserContext {
    ParserState state = ParserState::WaitForSof;
    Frame frame{};
    uint8_t headerIndex = 0;
    uint8_t payloadIndex = 0;
    uint8_t crcIndex = 0;
  };

  struct PendingFrame {
    Frame frame{};
    bool active = false;
    uint8_t retries = 0;
    uint32_t lastSendAt = 0;
  };

  void processIncomingByte(uint8_t byte);
  void handleFrame(const Frame &frame);
  void handleAckFrame(const Frame &frame);
  bool transmit(MessageKind kind, const uint8_t *payload, uint8_t length,
                uint8_t flags, bool trackPending, int forcedSequence = -1);
  bool sendAckFrame(MessageKind kind, uint8_t sequence, ErrorCode status);
  void resendPendingIfNeeded();
  bool validateLength(MessageKind kind, uint8_t length) const;
  void emitError(ErrorCode code, uint8_t sequence, uint16_t detail,
                 bool remote);
  void resetParser();
  uint8_t nextSequence();

  HardwareSerial *serial_ = nullptr;
  uint32_t baudRate_ = 0;
  ParserContext parser_{};
  PendingFrame pending_{};
  uint8_t sequenceCounter_ = 0;

  HelloHandler helloHandler_;
  TelemetryHandler telemetryHandler_;
  CommandHandler commandHandler_;
  ConfigHandler configHandler_;
  CommandAckHandler commandAckHandler_;
  ConfigAckHandler configAckHandler_;
  HeartbeatHandler heartbeatHandler_;
  ErrorHandler errorHandler_;
  LogHandler logHandler_;
};

} // namespace DryerUart
