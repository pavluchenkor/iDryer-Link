#include "uart_bridge.h"

#include <string.h>

namespace DryerUart {
namespace {

#ifdef DEBUG_SERIAL
#define DEBUG_LOG(...) DEBUG_SERIAL.printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

constexpr uint8_t HEADER_SIZE = sizeof(FrameHeader);

} // namespace

void UartBridge::begin(HardwareSerial &serial, uint32_t baudRate) {
  serial_ = &serial;
  baudRate_ = baudRate;
  serial_->begin(baudRate_);
  reset();
}

void UartBridge::reset() {
  resetParser();
  pending_.active = false;
  pending_.retries = 0;
  pending_.lastSendAt = 0;
  sequenceCounter_ = 0;
}

void UartBridge::loop() {
  if (!serial_) {
    return;
  }
  while (serial_->available() > 0) {
    const uint8_t byte = static_cast<uint8_t>(serial_->read());
    processIncomingByte(byte);
  }
  resendPendingIfNeeded();
}

bool UartBridge::sendHello(const HelloPayload &payload, bool ackRequired) {
  const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
  return transmit(MessageKind::Hello,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), flags, ackRequired);
}

bool UartBridge::sendHelloAck(const HelloPayload &payload) {
  return transmit(MessageKind::HelloAck,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), FLAG_IS_ACK, false);
}

bool UartBridge::sendTelemetry(const TelemetryPayload &payload,
                               bool ackRequired) {
  const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
  return transmit(MessageKind::Telemetry,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), flags, ackRequired);
}

bool UartBridge::sendCommand(const CommandPayload &payload, bool ackRequired) {
  const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
  return transmit(MessageKind::Command,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), flags, ackRequired);
}

bool UartBridge::sendConfigPush(const ConfigPayload &payload,
                                bool ackRequired) {
  const uint8_t flags = ackRequired ? FLAG_ACK_REQUIRED : 0;
  return transmit(MessageKind::ConfigPush,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), flags, ackRequired);
}

bool UartBridge::sendHeartbeat(const HeartbeatPayload &payload) {
  return transmit(MessageKind::Heartbeat,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), 0, false);
}

bool UartBridge::sendError(const ErrorPayload &payload) {
  return transmit(MessageKind::Error,
                  reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), FLAG_ERROR, false);
}

bool UartBridge::sendTelemetryAck(uint8_t sequence, ErrorCode status) {
  return sendAckFrame(MessageKind::TelemetryAck, sequence, status);
}

bool UartBridge::sendCommandAck(uint8_t sequence, ErrorCode status) {
  return sendAckFrame(MessageKind::CommandAck, sequence, status);
}

bool UartBridge::sendConfigAck(uint8_t sequence, ErrorCode status) {
  return sendAckFrame(MessageKind::ConfigAck, sequence, status);
}

bool UartBridge::sendLog(const uint8_t *message, uint8_t length) {
  if (!message || length > MAX_PAYLOAD_SIZE) {
    return false;
  }
  return transmit(MessageKind::Log, message, length, 0, false);
}

bool UartBridge::sendLog(const char *cstr) {
  if (!cstr) {
    return false;
  }
  size_t len = strlen(cstr);
  if (len > MAX_PAYLOAD_SIZE) {
    len = MAX_PAYLOAD_SIZE;
  }
  return sendLog(reinterpret_cast<const uint8_t *>(cstr),
                 static_cast<uint8_t>(len));
}

void UartBridge::setHelloHandler(const HelloHandler &handler) {
  helloHandler_ = handler;
}

void UartBridge::setTelemetryHandler(const TelemetryHandler &handler) {
  telemetryHandler_ = handler;
}

void UartBridge::setCommandHandler(const CommandHandler &handler) {
  commandHandler_ = handler;
}

void UartBridge::setConfigHandler(const ConfigHandler &handler) {
  configHandler_ = handler;
}

void UartBridge::setCommandAckHandler(const CommandAckHandler &handler) {
  commandAckHandler_ = handler;
}

void UartBridge::setConfigAckHandler(const ConfigAckHandler &handler) {
  configAckHandler_ = handler;
}

void UartBridge::setHeartbeatHandler(const HeartbeatHandler &handler) {
  heartbeatHandler_ = handler;
}

void UartBridge::setErrorHandler(const ErrorHandler &handler) {
  errorHandler_ = handler;
}

void UartBridge::setLogHandler(const LogHandler &handler) {
  logHandler_ = handler;
}

void UartBridge::processIncomingByte(uint8_t byte) {
  switch (parser_.state) {
    case ParserState::WaitForSof:
      if (byte == SOF) {
        parser_.frame.header.sof = byte;
        parser_.headerIndex = 1;
        parser_.state = ParserState::Header;
      }
      break;
    case ParserState::Header: {
      uint8_t *raw = reinterpret_cast<uint8_t *>(&parser_.frame.header);
      raw[parser_.headerIndex++] = byte;
      if (parser_.headerIndex >= HEADER_SIZE) {
        if (parser_.frame.header.payloadLength > MAX_PAYLOAD_SIZE) {
          emitError(ErrorCode::InvalidPayload, parser_.frame.header.sequence,
                    parser_.frame.header.payloadLength, false);
          resetParser();
          break;
        }
        parser_.payloadIndex = 0;
        parser_.state = parser_.frame.header.payloadLength > 0
                            ? ParserState::Payload
                            : ParserState::Crc;
      }
      break;
    }
    case ParserState::Payload:
      parser_.frame.payload[parser_.payloadIndex++] = byte;
      if (parser_.payloadIndex >= parser_.frame.header.payloadLength) {
        parser_.state = ParserState::Crc;
        parser_.crcIndex = 0;
      }
      break;
    case ParserState::Crc:
      if (parser_.crcIndex == 0) {
        parser_.frame.crc = byte;
        parser_.crcIndex = 1;
      } else {
        parser_.frame.crc |= static_cast<uint16_t>(byte) << 8;
        Frame frame = parser_.frame;
        resetParser();

        uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
        memcpy(buffer, &frame.header, HEADER_SIZE);
        if (frame.header.payloadLength > 0) {
          memcpy(buffer + HEADER_SIZE, frame.payload,
                 frame.header.payloadLength);
        }
        const uint16_t computed =
            calculateCrc(buffer, HEADER_SIZE + frame.header.payloadLength);
        if (computed != frame.crc) {
          emitError(ErrorCode::CrcMismatch, frame.header.sequence,
                    frame.header.payloadLength, false);
          break;
        }
        handleFrame(frame);
      }
      break;
  }
}

void UartBridge::handleFrame(const Frame &frame) {
  if (frame.header.version != PROTOCOL_VERSION) {
    emitError(ErrorCode::InvalidPayload, frame.header.sequence,
              frame.header.version, false);
    return;
  }

  switch (frame.header.kind) {
    case MessageKind::Telemetry: {
      if (!validateLength(MessageKind::Telemetry,
                          frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      TelemetryPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      if (telemetryHandler_) {
        telemetryHandler_(payload, frame.header);
      }
      if ((frame.header.flags & FLAG_ACK_REQUIRED) != 0) {
        sendTelemetryAck(frame.header.sequence);
      }
      break;
    }
    case MessageKind::Hello:
    case MessageKind::HelloAck: {
      if (!validateLength(MessageKind::Hello, frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      HelloPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      if (helloHandler_) {
        helloHandler_(payload, frame.header);
      }
      break;
    }
    case MessageKind::TelemetryAck:
    case MessageKind::CommandAck:
    case MessageKind::ConfigAck:
      handleAckFrame(frame);
      break;
    case MessageKind::Command: {
      if (!validateLength(MessageKind::Command,
                          frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      CommandPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      if (commandHandler_) {
        commandHandler_(payload, frame.header);
      }
      break;
    }
    case MessageKind::ConfigPush: {
      if (!validateLength(MessageKind::ConfigPush,
                          frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      ConfigPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      if (configHandler_) {
        configHandler_(payload, frame.header);
      }
      break;
    }
    case MessageKind::Heartbeat: {
      if (!validateLength(MessageKind::Heartbeat,
                          frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      HeartbeatPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      if (heartbeatHandler_) {
        heartbeatHandler_(payload, frame.header);
      }
      break;
    }
    case MessageKind::Error: {
      if (!validateLength(MessageKind::Error, frame.header.payloadLength)) {
        emitError(ErrorCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false);
        return;
      }
      ErrorPayload payload{};
      memcpy(&payload, frame.payload, sizeof(payload));
      emitError(payload.code, payload.lastSequence, payload.detail, true);
      break;
    }
    case MessageKind::Log:
      if (logHandler_) {
        logHandler_(frame.payload, frame.header.payloadLength);
      }
      break;
    default:
      emitError(ErrorCode::UnknownMessage, frame.header.sequence,
                static_cast<uint16_t>(frame.header.kind), false);
      break;
  }
}

void UartBridge::handleAckFrame(const Frame &frame) {
  if (!validateLength(frame.header.kind, frame.header.payloadLength)) {
    emitError(ErrorCode::InvalidPayload, frame.header.sequence,
              frame.header.payloadLength, false);
    return;
  }

  AckPayload payload{};
  memcpy(&payload, frame.payload, sizeof(payload));

  if (pending_.active &&
      pending_.frame.header.sequence == payload.ackSequence) {
    pending_.active = false;
  }

  switch (frame.header.kind) {
    case MessageKind::TelemetryAck:
      break;
    case MessageKind::CommandAck:
      if (commandAckHandler_) {
        commandAckHandler_(payload, frame.header);
      }
      break;
    case MessageKind::ConfigAck:
      if (configAckHandler_) {
        configAckHandler_(payload, frame.header);
      }
      break;
    default:
      break;
  }
}

bool UartBridge::transmit(MessageKind kind, const uint8_t *payload,
                          uint8_t length, uint8_t flags, bool trackPending,
                          int forcedSequence) {
  if (!serial_ || length > MAX_PAYLOAD_SIZE) {
    return false;
  }

  Frame frame{};
  frame.header.sof = SOF;
  frame.header.version = PROTOCOL_VERSION;
  frame.header.flags = flags;
  frame.header.kind = kind;
  frame.header.sequence =
      forcedSequence >= 0 ? static_cast<uint8_t>(forcedSequence)
                          : nextSequence();
  frame.header.payloadLength = length;
  if (length > 0 && payload) {
    memcpy(frame.payload, payload, length);
  }

  uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
  memcpy(buffer, &frame.header, HEADER_SIZE);
  if (length > 0 && payload) {
    memcpy(buffer + HEADER_SIZE, frame.payload, length);
  }
  frame.crc = calculateCrc(buffer, HEADER_SIZE + length);

  size_t written = serial_->write(
      reinterpret_cast<uint8_t *>(&frame.header), HEADER_SIZE);
  if (length > 0) {
    written += serial_->write(frame.payload, length);
  }
  uint8_t crcBytes[2] = {static_cast<uint8_t>(frame.crc & 0xFF),
                         static_cast<uint8_t>((frame.crc >> 8) & 0xFF)};
  written += serial_->write(crcBytes, sizeof(crcBytes));

  if (written != HEADER_SIZE + length + sizeof(uint16_t)) {
    return false;
  }

  if (trackPending) {
    pending_.frame = frame;
    pending_.active = true;
    pending_.retries = 0;
    pending_.lastSendAt = millis();
  }

  return true;
}

bool UartBridge::sendAckFrame(MessageKind kind, uint8_t sequence,
                              ErrorCode status) {
  AckPayload payload{};
  payload.ackSequence = sequence;
  payload.status = status;
  return transmit(kind, reinterpret_cast<const uint8_t *>(&payload),
                  sizeof(payload), FLAG_IS_ACK, false, sequence);
}

void UartBridge::resendPendingIfNeeded() {
  if (!pending_.active || !serial_) {
    return;
  }
  const uint32_t now = millis();
  if (now - pending_.lastSendAt < COMMAND_REPLY_TIMEOUT_MS) {
    return;
  }

  if (pending_.retries + 1 >= MAX_RETRIES) {
    pending_.active = false;
    emitError(ErrorCode::Timeout, pending_.frame.header.sequence,
              pending_.retries, false);
    return;
  }

  pending_.retries++;
  pending_.lastSendAt = now;

  size_t written = serial_->write(
      reinterpret_cast<uint8_t *>(&pending_.frame.header), HEADER_SIZE);
  if (pending_.frame.header.payloadLength > 0) {
    written += serial_->write(pending_.frame.payload,
                              pending_.frame.header.payloadLength);
  }
  uint8_t crcBytes[2] = {static_cast<uint8_t>(pending_.frame.crc & 0xFF),
                         static_cast<uint8_t>((pending_.frame.crc >> 8) & 0xFF)};
  written += serial_->write(crcBytes, sizeof(crcBytes));

  DEBUG_LOG("[UART] Resend seq=%u attempt=%u result=%u\n",
            pending_.frame.header.sequence, pending_.retries, written);
}

bool UartBridge::validateLength(MessageKind kind, uint8_t length) const {
  switch (kind) {
    case MessageKind::Hello:
    case MessageKind::HelloAck:
      return length == sizeof(HelloPayload);
    case MessageKind::Telemetry:
      return length == sizeof(TelemetryPayload);
    case MessageKind::Command:
      return length == sizeof(CommandPayload);
    case MessageKind::ConfigPush:
      return length == sizeof(ConfigPayload);
    case MessageKind::Heartbeat:
      return length == sizeof(HeartbeatPayload);
    case MessageKind::TelemetryAck:
    case MessageKind::CommandAck:
    case MessageKind::ConfigAck:
      return length == sizeof(AckPayload);
    case MessageKind::Error:
      return length == sizeof(ErrorPayload);
    case MessageKind::Log:
      return length <= MAX_PAYLOAD_SIZE;
    default:
      return length <= MAX_PAYLOAD_SIZE;
  }
}

void UartBridge::emitError(ErrorCode code, uint8_t sequence, uint16_t detail,
                           bool remote) {
  ErrorPayload payload{};
  payload.code = code;
  payload.lastSequence = sequence;
  payload.detail = detail;

  if (!remote) {
    sendError(payload);
  }

  if (errorHandler_) {
    errorHandler_(payload, remote);
  }
}

void UartBridge::resetParser() {
  parser_.state = ParserState::WaitForSof;
  parser_.headerIndex = 0;
  parser_.payloadIndex = 0;
  parser_.crcIndex = 0;
}

uint8_t UartBridge::nextSequence() {
  return sequenceCounter_++;
}

} // namespace DryerUart
