/**
 * @file idryer_protocol.h
 * @brief iDryer Protocol Library - Main include file
 *
 * This library provides types, payloads, MQTT topics, and UART frame
 * definitions for iDryer filament dryer devices.
 *
 * Compatible with:
 *   - ESP32-C3, ESP32-S3 (Arduino, ESP-IDF)
 *   - RP2040 (Arduino, Pico SDK)
 *   - Any platform with C99 support
 *
 * @version 2.5.0
 * @see https://github.com/idryer/idryer-protocol
 * @see docs/mqtt-api-kit/README.md
 *
 * @example
 *   #include <idryer_protocol.h>
 *
 *   // Use types
 *   idryer_telemetry_unit_t telemetry = {
 *       .unit_id = "U1",
 *       .temperature = 501,  // 50.1°C
 *       .humidity = 12,
 *       .heater_power = 85,
 *       .fan_status = 1
 *   };
 *
 *   // Build MQTT topic
 *   char topic[64];
 *   idryer_make_topic(topic, sizeof(topic), serial, IDRYER_TOPIC_TELEMETRY);
 *
 *   // Build UART frame
 *   uint8_t frame[128];
 *   size_t len = idryer_uart_build_frame(frame, IDRYER_UART_MSG_TELEMETRY,
 *                                         IDRYER_UART_FLAG_NONE,
 *                                         &telemetry, sizeof(telemetry));
 */

#ifndef IDRYER_PROTOCOL_H
#define IDRYER_PROTOCOL_H

// Version info
#include "version.h"

// Core types and enums
#include "types/idryer_types.h"

// Data payloads
#include "types/idryer_payloads.h"

// Command structures
#include "types/idryer_commands.h"

// MQTT topics
#include "mqtt/idryer_topics.h"

// UART frame format
#include "uart/idryer_uart_frame.h"

#endif // IDRYER_PROTOCOL_H
