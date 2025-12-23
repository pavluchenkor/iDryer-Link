/**
 * @file idryer_uart_frame.h
 * @brief UART frame format and CRC for ESP32 <-> RP2040 communication
 *
 * Frame format:
 *   [START] [VERSION] [FLAGS] [MSG_TYPE] [LENGTH_L] [LENGTH_H] [PAYLOAD...] [CRC_L] [CRC_H]
 *
 * @see work-plan/task-01-uart-architecture.md
 */

#ifndef IDRYER_UART_FRAME_H
#define IDRYER_UART_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Frame constants
// ============================================================================

#define IDRYER_UART_START_BYTE      0xAA    ///< Frame start marker
#define IDRYER_UART_PROTOCOL_VER    0x01    ///< Protocol version
#define IDRYER_UART_MAX_PAYLOAD     256     ///< Maximum payload size
#define IDRYER_UART_HEADER_SIZE     6       ///< Header size (start + ver + flags + type + len)
#define IDRYER_UART_CRC_SIZE        2       ///< CRC16 size
#define IDRYER_UART_MIN_FRAME_SIZE  (IDRYER_UART_HEADER_SIZE + IDRYER_UART_CRC_SIZE)

// ============================================================================
// Frame flags
// ============================================================================

#define IDRYER_UART_FLAG_NONE       0x00    ///< No flags
#define IDRYER_UART_FLAG_ACK_REQ    0x01    ///< Request acknowledgment
#define IDRYER_UART_FLAG_IS_ACK     0x02    ///< This is an acknowledgment
#define IDRYER_UART_FLAG_IS_NACK    0x04    ///< This is a negative acknowledgment
#define IDRYER_UART_FLAG_FRAGMENT   0x08    ///< Fragmented message (not last)
#define IDRYER_UART_FLAG_LAST_FRAG  0x10    ///< Last fragment

// ============================================================================
// Message types
// ============================================================================

/**
 * UART message types
 * Maps to MQTT topics and commands
 */
typedef enum {
    // RP2040 -> ESP32 (data for MQTT publish)
    IDRYER_UART_MSG_TELEMETRY     = 0x01,  ///< -> telemetry topic
    IDRYER_UART_MSG_WEIGHTS       = 0x02,  ///< -> weights topic
    IDRYER_UART_MSG_STATUS        = 0x03,  ///< -> status topic
    IDRYER_UART_MSG_EVENT         = 0x04,  ///< -> events topic
    IDRYER_UART_MSG_RFID          = 0x05,  ///< -> rfid topic
    IDRYER_UART_MSG_INFO          = 0x06,  ///< -> info topic
    IDRYER_UART_MSG_CONFIG        = 0x07,  ///< -> config topic

    // ESP32 -> RP2040 (commands from MQTT)
    IDRYER_UART_MSG_CMD_START     = 0x10,  ///< <- commands/start
    IDRYER_UART_MSG_CMD_STOP      = 0x11,  ///< <- commands/stop
    IDRYER_UART_MSG_CMD_GET_CFG   = 0x12,  ///< <- commands/get_config
    IDRYER_UART_MSG_CMD_SET_CFG   = 0x13,  ///< <- commands/set_config

    // Bidirectional control
    IDRYER_UART_MSG_ACK           = 0x20,  ///< Acknowledgment
    IDRYER_UART_MSG_NACK          = 0x21,  ///< Negative acknowledgment

    // ESP32 -> RP2040 (network status)
    IDRYER_UART_MSG_HEARTBEAT     = 0x30,  ///< Heartbeat/keepalive
    IDRYER_UART_MSG_WIFI_STATUS   = 0x31,  ///< Wi-Fi connection status
    IDRYER_UART_MSG_MQTT_STATUS   = 0x32,  ///< MQTT connection status

    // RP2040 -> ESP32 (diagnostics)
    IDRYER_UART_MSG_LOG           = 0x40,  ///< Debug log message
} idryer_uart_msg_t;

// ============================================================================
// Frame header structure
// ============================================================================

/**
 * UART frame header (packed)
 */
typedef struct __attribute__((packed)) {
    uint8_t  start;         ///< Start byte (IDRYER_UART_START_BYTE)
    uint8_t  version;       ///< Protocol version
    uint8_t  flags;         ///< Frame flags
    uint8_t  msg_type;      ///< Message type (idryer_uart_msg_t)
    uint16_t length;        ///< Payload length (little-endian)
} idryer_uart_header_t;

/**
 * Complete UART frame (for small payloads)
 */
typedef struct __attribute__((packed)) {
    idryer_uart_header_t header;
    uint8_t  payload[IDRYER_UART_MAX_PAYLOAD];
    uint16_t crc;           ///< CRC16 (little-endian)
} idryer_uart_frame_t;

// ============================================================================
// Wi-Fi/MQTT status payloads
// ============================================================================

/**
 * Wi-Fi connection status
 */
typedef enum {
    IDRYER_WIFI_DISCONNECTED = 0,
    IDRYER_WIFI_CONNECTING   = 1,
    IDRYER_WIFI_CONNECTED    = 2,
    IDRYER_WIFI_ERROR        = 3
} idryer_wifi_status_t;

/**
 * MQTT connection status
 */
typedef enum {
    IDRYER_MQTT_DISCONNECTED = 0,
    IDRYER_MQTT_CONNECTING   = 1,
    IDRYER_MQTT_CONNECTED    = 2,
    IDRYER_MQTT_ERROR        = 3
} idryer_mqtt_status_t;

/**
 * Wi-Fi status payload
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;        ///< idryer_wifi_status_t
    int8_t   rssi;          ///< Signal strength in dBm
    uint8_t  ip[4];         ///< IP address (if connected)
} idryer_wifi_status_payload_t;

/**
 * MQTT status payload
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;        ///< idryer_mqtt_status_t
    uint8_t  reserved;
} idryer_mqtt_status_payload_t;

// ============================================================================
// CRC16 functions
// ============================================================================

/**
 * Calculate CRC16-CCITT
 *
 * @param data   Data buffer
 * @param len    Data length
 * @return       CRC16 value
 */
uint16_t idryer_crc16(const uint8_t* data, size_t len);

/**
 * Verify CRC16 of received frame
 *
 * @param frame  Complete frame including CRC
 * @param len    Total frame length
 * @return       true if CRC is valid
 */
bool idryer_crc16_verify(const uint8_t* frame, size_t len);

// ============================================================================
// Frame building functions
// ============================================================================

/**
 * Initialize frame header
 *
 * @param header     Header to initialize
 * @param msg_type   Message type
 * @param flags      Frame flags
 * @param length     Payload length
 */
static inline void idryer_uart_header_init(idryer_uart_header_t* header,
                                            idryer_uart_msg_t msg_type,
                                            uint8_t flags,
                                            uint16_t length) {
    header->start = IDRYER_UART_START_BYTE;
    header->version = IDRYER_UART_PROTOCOL_VER;
    header->flags = flags;
    header->msg_type = msg_type;
    header->length = length;
}

/**
 * Build complete frame with CRC
 *
 * @param buf        Output buffer (must be at least header_size + payload_len + 2)
 * @param msg_type   Message type
 * @param flags      Frame flags
 * @param payload    Payload data (can be NULL if payload_len is 0)
 * @param payload_len Payload length
 * @return           Total frame length, or 0 on error
 */
size_t idryer_uart_build_frame(uint8_t* buf,
                                idryer_uart_msg_t msg_type,
                                uint8_t flags,
                                const void* payload,
                                size_t payload_len);

/**
 * Parse received frame
 *
 * @param buf        Input buffer
 * @param len        Buffer length
 * @param header     Output: parsed header
 * @param payload    Output: pointer to payload start
 * @param payload_len Output: payload length
 * @return           true if frame is valid
 */
bool idryer_uart_parse_frame(const uint8_t* buf,
                              size_t len,
                              idryer_uart_header_t* header,
                              const uint8_t** payload,
                              size_t* payload_len);

// ============================================================================
// Timing constants
// ============================================================================

#define IDRYER_UART_BAUD_RATE           115200  ///< Baud rate
#define IDRYER_UART_HEARTBEAT_MS        1000    ///< Heartbeat interval
#define IDRYER_UART_CMD_TIMEOUT_MS      500     ///< Command response timeout
#define IDRYER_UART_LINK_LOSS_MS        5000    ///< Link loss detection timeout
#define IDRYER_UART_MAX_RETRIES         3       ///< Max retries before fault

#ifdef __cplusplus
}
#endif

#endif // IDRYER_UART_FRAME_H
