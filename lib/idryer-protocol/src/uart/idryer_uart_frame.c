/**
 * @file idryer_uart_frame.c
 * @brief UART frame functions implementation
 */

#include "idryer_uart_frame.h"
#include <string.h>

// ============================================================================
// CRC16-CCITT implementation
// Polynomial: 0x1021, Init: 0xFFFF
// ============================================================================

uint16_t idryer_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

bool idryer_crc16_verify(const uint8_t* frame, size_t len) {
    if (len < IDRYER_UART_MIN_FRAME_SIZE) {
        return false;
    }

    // CRC is calculated over header + payload (excluding CRC itself)
    size_t data_len = len - IDRYER_UART_CRC_SIZE;
    uint16_t calculated = idryer_crc16(frame, data_len);

    // Extract received CRC (little-endian)
    uint16_t received = frame[data_len] | ((uint16_t)frame[data_len + 1] << 8);

    return calculated == received;
}

// ============================================================================
// Frame building
// ============================================================================

size_t idryer_uart_build_frame(uint8_t* buf,
                                idryer_uart_msg_t msg_type,
                                uint8_t flags,
                                const void* payload,
                                size_t payload_len) {
    if (!buf) return 0;
    if (payload_len > IDRYER_UART_MAX_PAYLOAD) return 0;

    // Build header
    idryer_uart_header_t* header = (idryer_uart_header_t*)buf;
    idryer_uart_header_init(header, msg_type, flags, (uint16_t)payload_len);

    // Copy payload
    if (payload && payload_len > 0) {
        memcpy(buf + IDRYER_UART_HEADER_SIZE, payload, payload_len);
    }

    // Calculate CRC over header + payload
    size_t data_len = IDRYER_UART_HEADER_SIZE + payload_len;
    uint16_t crc = idryer_crc16(buf, data_len);

    // Append CRC (little-endian)
    buf[data_len] = crc & 0xFF;
    buf[data_len + 1] = (crc >> 8) & 0xFF;

    return data_len + IDRYER_UART_CRC_SIZE;
}

// ============================================================================
// Frame parsing
// ============================================================================

bool idryer_uart_parse_frame(const uint8_t* buf,
                              size_t len,
                              idryer_uart_header_t* header,
                              const uint8_t** payload,
                              size_t* payload_len) {
    // Check minimum length
    if (!buf || len < IDRYER_UART_MIN_FRAME_SIZE) {
        return false;
    }

    // Check start byte
    if (buf[0] != IDRYER_UART_START_BYTE) {
        return false;
    }

    // Parse header
    const idryer_uart_header_t* hdr = (const idryer_uart_header_t*)buf;

    // Check version
    if (hdr->version != IDRYER_UART_PROTOCOL_VER) {
        return false;
    }

    // Check payload length
    if (hdr->length > IDRYER_UART_MAX_PAYLOAD) {
        return false;
    }

    // Check total frame length
    size_t expected_len = IDRYER_UART_HEADER_SIZE + hdr->length + IDRYER_UART_CRC_SIZE;
    if (len < expected_len) {
        return false;
    }

    // Verify CRC
    if (!idryer_crc16_verify(buf, expected_len)) {
        return false;
    }

    // Output parsed data
    if (header) {
        *header = *hdr;
    }
    if (payload) {
        *payload = (hdr->length > 0) ? (buf + IDRYER_UART_HEADER_SIZE) : NULL;
    }
    if (payload_len) {
        *payload_len = hdr->length;
    }

    return true;
}
