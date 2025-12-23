*@file idryer_topics.h
/**
 * @brief MQTT topic definitions, QoS levels, and helpers
 *
 * @see docs/mqtt-api-kit/02-api-reference/topics.md
 */

#ifndef IDRYER_TOPICS_H
#define IDRYER_TOPICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
 extern "C"
{
#endif

    // ============================================================================
    // Topic prefix
    // ============================================================================

#define IDRYER_TOPIC_PREFIX "idryer"

    // ============================================================================
    // Device -> Backend topics (publish)
    // ============================================================================

#define IDRYER_TOPIC_INFO "info"
#define IDRYER_TOPIC_TELEMETRY "telemetry"
#define IDRYER_TOPIC_WEIGHTS "weights"
#define IDRYER_TOPIC_RFID "rfid"
#define IDRYER_TOPIC_STATUS "status"
#define IDRYER_TOPIC_EVENTS "events"
#define IDRYER_TOPIC_CONFIG "config"

    // ============================================================================
    // Backend -> Device topics (subscribe)
    // ============================================================================

#define IDRYER_TOPIC_CMD_START "commands/start"
#define IDRYER_TOPIC_CMD_STOP "commands/stop"
#define IDRYER_TOPIC_CMD_GET_CONFIG "commands/get_config"
#define IDRYER_TOPIC_CMD_SET_CONFIG "commands/set_config"
#define IDRYER_TOPIC_CMD_WILDCARD "commands/#"

    // ============================================================================
    // QoS levels
    // @see docs/mqtt-api-kit/02-api-reference/topics.md#qos-quality-of-service
    // ============================================================================

#define IDRYER_QOS_INFO 1
#define IDRYER_QOS_TELEMETRY 0 // Loss is acceptable (next in 5s)
#define IDRYER_QOS_WEIGHTS 1
#define IDRYER_QOS_RFID 1
#define IDRYER_QOS_STATUS 1
#define IDRYER_QOS_EVENTS 1
#define IDRYER_QOS_CONFIG 1
#define IDRYER_QOS_COMMANDS 1

    // ============================================================================
    // Retained flags
    // @see docs/mqtt-api-kit/02-api-reference/topics.md#retained-сообщения
    // ============================================================================

#define IDRYER_RETAINED_INFO 1
#define IDRYER_RETAINED_TELEMETRY 0
#define IDRYER_RETAINED_WEIGHTS 0
#define IDRYER_RETAINED_RFID 1
#define IDRYER_RETAINED_STATUS 1
#define IDRYER_RETAINED_EVENTS 0
#define IDRYER_RETAINED_CONFIG 0

    // ============================================================================
    // Publish intervals (milliseconds)
    // ============================================================================

#define IDRYER_INTERVAL_TELEMETRY_MS 5000 // 5 seconds
#define IDRYER_INTERVAL_WEIGHTS_MS 10000  // 10 seconds
#define IDRYER_INTERVAL_STATUS_MS 0       // On change only
#define IDRYER_INTERVAL_EVENTS_MS 0       // On event only

    // ============================================================================
    // Topic info structure
    // ============================================================================

    /**
     * Topic metadata structure
     */
    typedef struct
    {
        const char *suffix;   ///< Topic suffix (e.g., "telemetry")
        uint8_t qos;          ///< QoS level (0, 1, 2)
        uint8_t retained;     ///< Retained flag (0 or 1)
        uint32_t interval_ms; ///< Publish interval (0 = on event)
    } idryer_topic_info_t;

    /**
     * All topics metadata (for iteration)
     */
    static const idryer_topic_info_t IDRYER_TOPICS[] = {
        {IDRYER_TOPIC_INFO, IDRYER_QOS_INFO, IDRYER_RETAINED_INFO, 0},
        {IDRYER_TOPIC_TELEMETRY, IDRYER_QOS_TELEMETRY, IDRYER_RETAINED_TELEMETRY, IDRYER_INTERVAL_TELEMETRY_MS},
        {IDRYER_TOPIC_WEIGHTS, IDRYER_QOS_WEIGHTS, IDRYER_RETAINED_WEIGHTS, IDRYER_INTERVAL_WEIGHTS_MS},
        {IDRYER_TOPIC_RFID, IDRYER_QOS_RFID, IDRYER_RETAINED_RFID, 0},
        {IDRYER_TOPIC_STATUS, IDRYER_QOS_STATUS, IDRYER_RETAINED_STATUS, 0},
        {IDRYER_TOPIC_EVENTS, IDRYER_QOS_EVENTS, IDRYER_RETAINED_EVENTS, 0},
        {IDRYER_TOPIC_CONFIG, IDRYER_QOS_CONFIG, IDRYER_RETAINED_CONFIG, 0},
    };

#define IDRYER_TOPICS_COUNT (sizeof(IDRYER_TOPICS) / sizeof(IDRYER_TOPICS[0]))

    // ============================================================================
    // Helper functions
    // ============================================================================

    /**
     * Build full topic string
     *
     * @param buf        Output buffer
     * @param buf_size   Buffer size
     * @param serial     Device serial number
     * @param suffix     Topic suffix (use IDRYER_TOPIC_* constants)
     * @return           Pointer to buf, or NULL if buffer too small
     *
     * @example
     *   char topic[64];
     *   idryer_make_topic(topic, sizeof(topic), "DEVICE_abc123", IDRYER_TOPIC_TELEMETRY);
     *   // Result: "idryer/DEVICE_abc123/telemetry"
     */
    static inline char *idryer_make_topic(char *buf, size_t buf_size,
                                          const char *serial, const char *suffix)
    {
        int len = snprintf(buf, buf_size, "%s/%s/%s",
                           IDRYER_TOPIC_PREFIX, serial, suffix);
        return (len > 0 && (size_t)len < buf_size) ? buf : NULL;
    }

    /**
     * Build command subscription topic (with wildcard)
     *
     * @param buf        Output buffer
     * @param buf_size   Buffer size
     * @param serial     Device serial number
     * @return           Pointer to buf
     *
     * @example
     *   char topic[64];
     *   idryer_make_cmd_subscribe_topic(topic, sizeof(topic), "DEVICE_abc123");
     *   // Result: "idryer/DEVICE_abc123/commands/#"
     */
    static inline char *idryer_make_cmd_subscribe_topic(char *buf, size_t buf_size,
                                                        const char *serial)
    {
        return idryer_make_topic(buf, buf_size, serial, IDRYER_TOPIC_CMD_WILDCARD);
    }

    /**
     * Get topic info by suffix
     *
     * @param suffix     Topic suffix
     * @return           Pointer to topic info, or NULL if not found
     */
    static inline const idryer_topic_info_t *idryer_get_topic_info(const char *suffix)
    {
        for (size_t i = 0; i < IDRYER_TOPICS_COUNT; i++)
        {
            if (strcmp(IDRYER_TOPICS[i].suffix, suffix) == 0)
            {
                return &IDRYER_TOPICS[i];
            }
        }
        return NULL;
    }

    /**
     * Extract topic suffix from full topic
     *
     * @param full_topic Full topic string (e.g., "idryer/DEVICE_xxx/telemetry")
     * @return           Pointer to suffix part, or NULL if invalid
     *
     * @example
     *   const char* suffix = idryer_extract_topic_suffix("idryer/DEVICE_abc/commands/start");
     *   // Result: "commands/start"
     */
    static inline const char *idryer_extract_topic_suffix(const char *full_topic)
    {
        // Skip "idryer/"
        const char *p = full_topic;
        if (strncmp(p, IDRYER_TOPIC_PREFIX "/", sizeof(IDRYER_TOPIC_PREFIX)) != 0)
        {
            return NULL;
        }
        p += sizeof(IDRYER_TOPIC_PREFIX); // Skip "idryer/"

        // Skip serial number
        p = strchr(p, '/');
        if (!p)
            return NULL;

        return p + 1; // Return suffix after second '/'
    }

#ifdef __cplusplus
}
#endif

#endif // IDRYER_TOPICS_H
