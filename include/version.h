#pragma once

// =============================================================================
// ВЕРСИЯ ПРОШИВКИ iDryer Link
// =============================================================================

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

// Строка версии "1.0.0"
#define VERSION_STRING       \
    STRINGIFY(VERSION_MAJOR) \
    "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Число версии 0x010000 (для UART протокола)
#define VERSION_NUMBER \
    ((VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | VERSION_PATCH)

// Вспомогательный макрос для stringify
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
