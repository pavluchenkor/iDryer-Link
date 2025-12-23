/**
 * @file idryer_commands.h
 * @brief Command structures (Backend -> Device)
 *
 * @see docs/mqtt-api-kit/02-api-reference/commands.md
 */

#ifndef IDRYER_COMMANDS_H
#define IDRYER_COMMANDS_H

#include "idryer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Profile Stage
// ============================================================================

/**
 * Single stage in a drying profile
 * Used in PROFILE mode
 */
typedef struct __attribute__((packed)) {
    int16_t  temperature;   ///< Target temperature in °C * 10
    uint16_t duration;      ///< Stage duration in minutes
    uint8_t  humidity;      ///< Target humidity in % (optional, 0 if not used)
} idryer_profile_stage_t;

// ============================================================================
// Command: START
// @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-start
// ============================================================================

/**
 * START command payload
 * Received from: idryer/{serialNumber}/commands/start
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];  ///< Target unit ID ("U1", "U2")
    uint8_t  mode;              ///< Mode to start (DRYING, STORAGE, PROFILE)

    // For DRYING/STORAGE modes
    int16_t  temperature;       ///< Target temperature in °C * 10
    uint16_t duration;          ///< Duration in minutes (INFINITE for STORAGE)
    uint8_t  humidity;          ///< Target humidity in %

    // For PROFILE mode
    uint8_t  stage_count;       ///< Number of stages (0 if not PROFILE)
    idryer_profile_stage_t stages[IDRYER_MAX_PROFILE_STAGES];
} idryer_cmd_start_t;

// ============================================================================
// Command: STOP
// @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-stop
// ============================================================================

/**
 * STOP command payload
 * Received from: idryer/{serialNumber}/commands/stop
 */
typedef struct __attribute__((packed)) {
    char unit_id[IDRYER_UNIT_ID_LEN];  ///< Target unit ID
} idryer_cmd_stop_t;

// ============================================================================
// Command: GET_CONFIG
// @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-get_config
// ============================================================================

/**
 * GET_CONFIG command payload
 * Received from: idryer/{serialNumber}/commands/get_config
 * Response: publish config to idryer/{serialNumber}/config
 */
typedef struct __attribute__((packed)) {
    uint8_t reserved;  ///< Reserved (command has no parameters)
} idryer_cmd_get_config_t;

// ============================================================================
// Command: SET_CONFIG
// @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-set_config
// ============================================================================

/**
 * SET_CONFIG command header
 * Received from: idryer/{serialNumber}/commands/set_config
 *
 * Note: The actual config payload is variable-length JSON.
 * This structure is just the header for UART transmission.
 */
typedef struct __attribute__((packed)) {
    uint32_t config_version;    ///< Config version for conflict detection
    uint16_t payload_length;    ///< Length of following JSON payload
    // Followed by: char payload[payload_length]
} idryer_cmd_set_config_t;

// ============================================================================
// Command ACK (response)
// ============================================================================

/**
 * Command acknowledgment status
 */
typedef enum {
    IDRYER_CMD_STATUS_SUCCESS = 0,  ///< Command executed successfully
    IDRYER_CMD_STATUS_FAILURE = 1,  ///< Command failed
    IDRYER_CMD_STATUS_INVALID = 2,  ///< Invalid command or parameters
    IDRYER_CMD_STATUS_BUSY    = 3   ///< Unit busy, cannot execute
} idryer_cmd_status_t;

/**
 * Command acknowledgment payload
 * Published to: idryer/{serialNumber}/events as COMMAND_ACK event
 */
typedef struct __attribute__((packed)) {
    uint8_t  command;           ///< Command type that was acknowledged
    uint8_t  status;            ///< Execution status (idryer_cmd_status_t)
    char     unit_id[IDRYER_UNIT_ID_LEN];  ///< Unit ID (if applicable)
} idryer_cmd_ack_t;

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Initialize START command with defaults
 * @param cmd Command structure to initialize
 * @param unit_id Target unit ID
 * @param mode Operating mode
 */
static inline void idryer_cmd_start_init(idryer_cmd_start_t* cmd,
                                          const char* unit_id,
                                          idryer_mode_t mode) {
    memset(cmd, 0, sizeof(*cmd));
    strncpy(cmd->unit_id, unit_id, IDRYER_UNIT_ID_LEN - 1);
    cmd->mode = mode;
    cmd->duration = IDRYER_DURATION_INFINITE;  // Default for STORAGE
}

/**
 * Add a stage to PROFILE command
 * @param cmd Command structure
 * @param temp_c Temperature in °C
 * @param duration_min Duration in minutes
 * @param humidity Target humidity in %
 * @return true if stage added, false if max stages reached
 */
static inline bool idryer_cmd_start_add_stage(idryer_cmd_start_t* cmd,
                                               float temp_c,
                                               uint16_t duration_min,
                                               uint8_t humidity) {
    if (cmd->stage_count >= IDRYER_MAX_PROFILE_STAGES) {
        return false;
    }
    idryer_profile_stage_t* stage = &cmd->stages[cmd->stage_count];
    stage->temperature = IDRYER_TEMP_FROM_FLOAT(temp_c);
    stage->duration = duration_min;
    stage->humidity = humidity;
    cmd->stage_count++;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // IDRYER_COMMANDS_H
