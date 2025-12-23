/**
 * @file idryer_types.h
 * @brief Core enums and type definitions
 *
 * @see docs/mqtt-api-kit/02-api-reference/types.md
 */

#ifndef IDRYER_TYPES_H
#define IDRYER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define IDRYER_UNIT_ID_LEN          4    // "U1\0\0", "U2\0\0"
#define IDRYER_SENSOR_ID_LEN        4    // "W1\0\0", "R1\0\0"
#define IDRYER_SOURCE_LEN           16   // "HEATER", "THERMISTOR"
#define IDRYER_EVENT_LEN            24   // "SENSOR_SHORT", "NO_RESPONSE"
#define IDRYER_MESSAGE_LEN          64   // Human-readable message
#define IDRYER_TAG_LEN              16   // RFID tag hex string
#define IDRYER_VERSION_LEN          16   // "1.2.3", "v1.0"

#define IDRYER_MAX_UNITS            4    // Max units per device
#define IDRYER_MAX_WEIGHTS          4    // Max weight sensors
#define IDRYER_MAX_PROFILE_STAGES   10   // Max stages in profile

#define IDRYER_DURATION_INFINITE    0xFFFF  // For STORAGE mode

// ============================================================================
// Enums
// ============================================================================

/**
 * Unit operating mode
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-mode
 */
typedef enum {
    IDRYER_MODE_IDLE    = 0,  ///< Unit is idle, nothing running
    IDRYER_MODE_DRYING  = 1,  ///< Simple drying (single stage, fixed time)
    IDRYER_MODE_STORAGE = 2,  ///< Long-term storage (infinite, maintains humidity)
    IDRYER_MODE_PROFILE = 3,  ///< Profile drying (multiple stages)
    IDRYER_MODE_FAULT   = 4   ///< Fault state (critical error, stopped)
} idryer_mode_t;

/**
 * Event severity level
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-severity
 */
typedef enum {
    IDRYER_SEVERITY_INFO     = 0,  ///< Informational message
    IDRYER_SEVERITY_WARNING  = 1,  ///< Warning (non-critical)
    IDRYER_SEVERITY_ERROR    = 2,  ///< Error (needs attention)
    IDRYER_SEVERITY_CRITICAL = 3   ///< Critical (automatic stop)
} idryer_severity_t;

/**
 * RFID event type
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-rfideventtype
 */
typedef enum {
    IDRYER_RFID_TAG_DETECTED = 0,  ///< Tag detected in reader
    IDRYER_RFID_TAG_REMOVED  = 1   ///< Tag removed from reader
} idryer_rfid_event_t;

// ============================================================================
// String conversion helpers
// ============================================================================

/**
 * Convert mode enum to string
 * @param mode Mode value
 * @return String representation ("IDLE", "DRYING", etc.)
 */
static inline const char* idryer_mode_to_string(idryer_mode_t mode) {
    switch (mode) {
        case IDRYER_MODE_IDLE:    return "IDLE";
        case IDRYER_MODE_DRYING:  return "DRYING";
        case IDRYER_MODE_STORAGE: return "STORAGE";
        case IDRYER_MODE_PROFILE: return "PROFILE";
        case IDRYER_MODE_FAULT:   return "FAULT";
        default:                  return "UNKNOWN";
    }
}

/**
 * Convert string to mode enum
 * @param str String representation
 * @return Mode value (IDRYER_MODE_IDLE if unknown)
 */
static inline idryer_mode_t idryer_mode_from_string(const char* str) {
    if (!str) return IDRYER_MODE_IDLE;
    if (strcmp(str, "IDLE") == 0)    return IDRYER_MODE_IDLE;
    if (strcmp(str, "DRYING") == 0)  return IDRYER_MODE_DRYING;
    if (strcmp(str, "STORAGE") == 0) return IDRYER_MODE_STORAGE;
    if (strcmp(str, "PROFILE") == 0) return IDRYER_MODE_PROFILE;
    if (strcmp(str, "FAULT") == 0)   return IDRYER_MODE_FAULT;
    return IDRYER_MODE_IDLE;
}

/**
 * Convert severity enum to string
 * @param sev Severity value
 * @return String representation ("info", "warning", etc.)
 */
static inline const char* idryer_severity_to_string(idryer_severity_t sev) {
    switch (sev) {
        case IDRYER_SEVERITY_INFO:     return "info";
        case IDRYER_SEVERITY_WARNING:  return "warning";
        case IDRYER_SEVERITY_ERROR:    return "error";
        case IDRYER_SEVERITY_CRITICAL: return "critical";
        default:                       return "info";
    }
}

/**
 * Convert string to severity enum
 * @param str String representation
 * @return Severity value (IDRYER_SEVERITY_INFO if unknown)
 */
static inline idryer_severity_t idryer_severity_from_string(const char* str) {
    if (!str) return IDRYER_SEVERITY_INFO;
    if (strcmp(str, "info") == 0)     return IDRYER_SEVERITY_INFO;
    if (strcmp(str, "warning") == 0)  return IDRYER_SEVERITY_WARNING;
    if (strcmp(str, "error") == 0)    return IDRYER_SEVERITY_ERROR;
    if (strcmp(str, "critical") == 0) return IDRYER_SEVERITY_CRITICAL;
    return IDRYER_SEVERITY_INFO;
}

/**
 * Convert RFID event enum to string
 * @param evt Event value
 * @return String representation
 */
static inline const char* idryer_rfid_event_to_string(idryer_rfid_event_t evt) {
    switch (evt) {
        case IDRYER_RFID_TAG_DETECTED: return "tag_detected";
        case IDRYER_RFID_TAG_REMOVED:  return "tag_removed";
        default:                       return "tag_detected";
    }
}

#ifdef __cplusplus
}
#endif

#endif // IDRYER_TYPES_H
