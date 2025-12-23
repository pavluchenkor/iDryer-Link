/**
 * @file basic_usage.ino
 * @brief Basic usage example of idryer-protocol library
 *
 * This example demonstrates:
 *   - Using type definitions
 *   - Building MQTT topics
 *   - Creating UART frames
 */

#include <idryer_protocol.h>

// Your device serial number
const char* serialNumber = "DEVICE_abc123_1234567";

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Print library version
    Serial.println("=== iDryer Protocol Example ===");
    Serial.printf("Library version: %s\n", IDRYER_PROTOCOL_VERSION);
    Serial.printf("API version: %s\n", IDRYER_API_VERSION);
    Serial.printf("Sync date: %s\n", IDRYER_API_SYNC_DATE);
    Serial.println();

    // Example: Create telemetry
    exampleTelemetry();

    // Example: Create status
    exampleStatus();

    // Example: Build MQTT topics
    exampleTopics();

    // Example: Build UART frame
    exampleUartFrame();

    // Example: Handle command
    exampleCommand();
}

void loop() {
    // Nothing here
}

// ============================================================================
// Example: Telemetry
// ============================================================================

void exampleTelemetry() {
    Serial.println("--- Telemetry Example ---");

    idryer_telemetry_unit_t telemetry = {
        .unit_id = "U1",
        .temperature = IDRYER_TEMP_FROM_FLOAT(50.1f),  // 501
        .humidity = 12,
        .heater_power = 85,
        .fan_status = 1
    };

    Serial.printf("Unit: %s\n", telemetry.unit_id);
    Serial.printf("Temperature: %.1f°C\n", IDRYER_TEMP_TO_FLOAT(telemetry.temperature));
    Serial.printf("Humidity: %d%%\n", telemetry.humidity);
    Serial.printf("Heater: %d%%\n", telemetry.heater_power);
    Serial.printf("Fan: %s\n", telemetry.fan_status ? "ON" : "OFF");
    Serial.println();
}

// ============================================================================
// Example: Status
// ============================================================================

void exampleStatus() {
    Serial.println("--- Status Example ---");

    idryer_status_unit_t status = {
        .unit_id = "U1",
        .mode = IDRYER_MODE_DRYING,
        .target_temp = IDRYER_TEMP_FROM_FLOAT(50.0f),
        .duration = 240,  // 4 hours
        .target_humidity = 10,
        .elapsed_time = 3600,  // 1 hour elapsed
    };

    Serial.printf("Unit: %s\n", status.unit_id);
    Serial.printf("Mode: %s\n", idryer_mode_to_string((idryer_mode_t)status.mode));
    Serial.printf("Target: %.1f°C\n", IDRYER_TEMP_TO_FLOAT(status.target_temp));
    Serial.printf("Duration: %d min\n", status.duration);
    Serial.printf("Elapsed: %lu sec\n", status.elapsed_time);

    // STORAGE mode example
    idryer_status_unit_t storage = {
        .unit_id = "U2",
        .mode = IDRYER_MODE_STORAGE,
        .target_temp = IDRYER_TEMP_FROM_FLOAT(25.0f),
        .duration = IDRYER_DURATION_INFINITE,  // Infinite
    };

    Serial.printf("\nStorage mode - infinite: %s\n",
                  IDRYER_DURATION_IS_INFINITE(storage.duration) ? "YES" : "NO");
    Serial.println();
}

// ============================================================================
// Example: MQTT Topics
// ============================================================================

void exampleTopics() {
    Serial.println("--- MQTT Topics Example ---");

    char topic[64];

    // Telemetry topic
    idryer_make_topic(topic, sizeof(topic), serialNumber, IDRYER_TOPIC_TELEMETRY);
    Serial.printf("Telemetry: %s (QoS %d, retained: %d)\n",
                  topic, IDRYER_QOS_TELEMETRY, IDRYER_RETAINED_TELEMETRY);

    // Status topic
    idryer_make_topic(topic, sizeof(topic), serialNumber, IDRYER_TOPIC_STATUS);
    Serial.printf("Status: %s (QoS %d, retained: %d)\n",
                  topic, IDRYER_QOS_STATUS, IDRYER_RETAINED_STATUS);

    // Commands subscription
    idryer_make_cmd_subscribe_topic(topic, sizeof(topic), serialNumber);
    Serial.printf("Subscribe: %s\n", topic);

    // Get topic info
    const idryer_topic_info_t* info = idryer_get_topic_info(IDRYER_TOPIC_TELEMETRY);
    if (info) {
        Serial.printf("Telemetry interval: %lu ms\n", info->interval_ms);
    }

    Serial.println();
}

// ============================================================================
// Example: UART Frame
// ============================================================================

void exampleUartFrame() {
    Serial.println("--- UART Frame Example ---");

    // Create telemetry data
    idryer_telemetry_unit_t telemetry = {
        .unit_id = "U1",
        .temperature = 501,
        .humidity = 12,
        .heater_power = 85,
        .fan_status = 1
    };

    // Build UART frame
    uint8_t frame[128];
    size_t len = idryer_uart_build_frame(
        frame,
        IDRYER_UART_MSG_TELEMETRY,
        IDRYER_UART_FLAG_NONE,
        &telemetry,
        sizeof(telemetry)
    );

    Serial.printf("Frame length: %d bytes\n", len);
    Serial.print("Frame hex: ");
    for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    // Parse frame back
    idryer_uart_header_t header;
    const uint8_t* payload;
    size_t payload_len;

    if (idryer_uart_parse_frame(frame, len, &header, &payload, &payload_len)) {
        Serial.printf("Parsed: type=0x%02X, len=%d\n", header.msg_type, payload_len);
    } else {
        Serial.println("Parse failed!");
    }

    Serial.println();
}

// ============================================================================
// Example: Command handling
// ============================================================================

void exampleCommand() {
    Serial.println("--- Command Example ---");

    // Create START command
    idryer_cmd_start_t cmd;
    idryer_cmd_start_init(&cmd, "U1", IDRYER_MODE_DRYING);
    cmd.temperature = IDRYER_TEMP_FROM_FLOAT(50.0f);
    cmd.duration = 240;
    cmd.humidity = 10;

    Serial.printf("START command:\n");
    Serial.printf("  Unit: %s\n", cmd.unit_id);
    Serial.printf("  Mode: %s\n", idryer_mode_to_string((idryer_mode_t)cmd.mode));
    Serial.printf("  Temp: %.1f°C\n", IDRYER_TEMP_TO_FLOAT(cmd.temperature));

    // Create PROFILE command
    idryer_cmd_start_t profile;
    idryer_cmd_start_init(&profile, "U1", IDRYER_MODE_PROFILE);
    idryer_cmd_start_add_stage(&profile, 40.0f, 30, 15);   // Stage 1
    idryer_cmd_start_add_stage(&profile, 50.0f, 120, 10);  // Stage 2
    idryer_cmd_start_add_stage(&profile, 45.0f, 180, 10);  // Stage 3

    Serial.printf("\nPROFILE command:\n");
    Serial.printf("  Stages: %d\n", profile.stage_count);
    for (int i = 0; i < profile.stage_count; i++) {
        Serial.printf("  Stage %d: %.1f°C for %d min\n",
                      i + 1,
                      IDRYER_TEMP_TO_FLOAT(profile.stages[i].temperature),
                      profile.stages[i].duration);
    }

    Serial.println();
    Serial.println("=== Done ===");
}
