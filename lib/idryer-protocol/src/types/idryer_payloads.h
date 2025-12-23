/**
 * @file idryer_payloads.h
 * @brief Data payload structures (packed for UART/binary transfer)
 *
 * @see docs/mqtt-api-kit/02-api-reference/types.md
 * @see docs/mqtt-api-kit/02-api-reference/device-to-backend.md
 */

#ifndef IDRYER_PAYLOADS_H
#define IDRYER_PAYLOADS_H

#include "idryer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Telemetry Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-telemetrypayload
// ============================================================================

/**
 * Telemetry data for a single unit
 * Published to: idryer/{serialNumber}/telemetry
 * Frequency: every 5 seconds
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];  ///< Unit ID ("U1", "U2")
    int16_t  temperature;    ///< Actual temperature in °C * 10 (e.g., 501 = 50.1°C)
    uint8_t  humidity;       ///< Actual humidity in % (0-100)
    uint8_t  heater_power;   ///< Heater power in % (0-100), 0 if off
    uint8_t  fan_status;     ///< Fan status (0 = off, 1 = on)
} idryer_telemetry_unit_t;

// ============================================================================
// Status Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-unitstatus
// ============================================================================

/**
 * Status data for a single unit
 * Published to: idryer/{serialNumber}/status (retained)
 * Frequency: on change
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];  ///< Unit ID
    uint8_t  mode;              ///< Operating mode (idryer_mode_t)

    // Target parameters (valid when mode != IDLE && mode != FAULT)
    int16_t  target_temp;       ///< Target temperature in °C * 10
    uint16_t duration;          ///< Duration in minutes (IDRYER_DURATION_INFINITE for STORAGE)
    uint8_t  target_humidity;   ///< Target humidity in %
    uint32_t elapsed_time;      ///< Seconds since unit started

    // Profile mode fields (valid when mode == PROFILE)
    uint8_t  current_stage;     ///< Current stage number (1-based)
    uint8_t  total_stages;      ///< Total number of stages
    uint32_t stage_elapsed;     ///< Seconds elapsed on current stage
    uint32_t stage_remaining;   ///< Seconds remaining on current stage
    uint32_t total_remaining;   ///< Seconds remaining for entire program
} idryer_status_unit_t;

// ============================================================================
// Weights Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-weightspayload
// ============================================================================

/**
 * Weight sensor data
 * Published to: idryer/{serialNumber}/weights
 * Frequency: every 10 seconds or on change > 1g
 */
typedef struct __attribute__((packed)) {
    char     sensor_id[IDRYER_SENSOR_ID_LEN];  ///< Sensor ID ("W1", "W2")
    int32_t  value;             ///< Weight in grams * 10 (e.g., 8123 = 812.3g)
    char     unit_id[IDRYER_UNIT_ID_LEN];      ///< Associated unit ID
} idryer_weight_t;

// ============================================================================
// RFID Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-rfideventpayload
// ============================================================================

/**
 * RFID event data
 * Published to: idryer/{serialNumber}/rfid (retained)
 * Frequency: on change
 */
typedef struct __attribute__((packed)) {
    uint8_t  event;             ///< Event type (idryer_rfid_event_t)
    char     reader_id[IDRYER_SENSOR_ID_LEN];  ///< Reader ID ("R1", "R2")
    char     tag[IDRYER_TAG_LEN];              ///< Tag hex string or empty if removed
    char     unit_id[IDRYER_UNIT_ID_LEN];      ///< Associated unit ID
} idryer_rfid_payload_t;

// ============================================================================
// Event Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-eventpayload
// ============================================================================

/**
 * Event/error data
 * Published to: idryer/{serialNumber}/events
 * Frequency: on event
 */
typedef struct __attribute__((packed)) {
    uint8_t  severity;          ///< Severity level (idryer_severity_t)
    char     source[IDRYER_SOURCE_LEN];    ///< Event source ("HEATER", "SHT", "THERMISTOR")
    char     event[IDRYER_EVENT_LEN];      ///< Event type ("SENSOR_SHORT", "NO_RESPONSE")
    char     unit_id[IDRYER_UNIT_ID_LEN];  ///< Associated unit ID (if applicable)
    char     message[IDRYER_MESSAGE_LEN];  ///< Human-readable message
} idryer_event_payload_t;

// ============================================================================
// Info Payload
// @see docs/mqtt-api-kit/02-api-reference/types.md#type-infopayload
// ============================================================================

/**
 * Device info data
 * Published to: idryer/{serialNumber}/info (retained)
 * Frequency: on boot
 */
typedef struct __attribute__((packed)) {
    char     hw_version[IDRYER_VERSION_LEN];   ///< Hardware version ("v1.0", "v1.1")
    char     fw_version[IDRYER_VERSION_LEN];   ///< Firmware version ("1.2.3")
    uint32_t work_time_counter;  ///< Total work time in seconds
} idryer_info_payload_t;

// ============================================================================
// Config Payload (JSON passthrough)
// @see docs/mqtt-api-kit/03-features/remote-config.md
// ============================================================================

/**
 * Maximum config JSON size (~8KB MQTT limit)
 */
#define IDRYER_CONFIG_MAX_SIZE      8192

/**
 * Config header for UART transmission
 *
 * Config is transmitted as JSON string (not binary struct).
 * This header precedes the JSON payload in UART frames.
 *
 * Published to: idryer/{serialNumber}/config
 * Frequency: on get_config command
 *
 * @note The actual config structure is dynamic JSON with nested menu items.
 *       It cannot be represented as a packed struct.
 *       See remote-config.md for JSON format.
 */
typedef struct __attribute__((packed)) {
    uint32_t config_version;     ///< Config structure version (increments on firmware change)
    uint16_t json_length;        ///< Length of following JSON string (excluding null terminator)
    // Followed by: char json_data[json_length]
} idryer_config_header_t;

// ============================================================================
// Helper macros
// ============================================================================

/**
 * Convert packed temperature to float
 * @param temp_x10 Temperature in °C * 10
 * @return Temperature as float
 */
#define IDRYER_TEMP_TO_FLOAT(temp_x10)  ((float)(temp_x10) / 10.0f)

/**
 * Convert float temperature to packed
 * @param temp_f Temperature as float
 * @return Temperature in °C * 10
 */
#define IDRYER_TEMP_FROM_FLOAT(temp_f)  ((int16_t)((temp_f) * 10.0f))

/**
 * Convert packed weight to float
 * @param weight_x10 Weight in grams * 10
 * @return Weight as float
 */
#define IDRYER_WEIGHT_TO_FLOAT(weight_x10)  ((float)(weight_x10) / 10.0f)

/**
 * Check if duration is infinite (STORAGE mode)
 * @param duration Duration value
 * @return true if infinite
 */
#define IDRYER_DURATION_IS_INFINITE(duration)  ((duration) == IDRYER_DURATION_INFINITE)

#ifdef __cplusplus
}
#endif

#endif // IDRYER_PAYLOADS_H
