#pragma once

// СИНХРОНИЗИРОВАНО с MCU: iDryerRP2040/src/version.h
// copy_menu.py копирует version.h MCU в lib/idryer-menu/src/ перед сборкой
// VERSION_MAJOR берётся из MCU, MINOR и PATCH - индивидуальны для LINK

#include <Arduino.h>

// Включаем version.h из MCU для получения VERSION_MAJOR
// (файл копируется автоматически при сборке)
#if __has_include("../lib/idryer-menu/src/version.h")
// Временно сохраняем макросы из MCU
#include "../lib/idryer-menu/src/version.h"
// VERSION_MAJOR взят из MCU
// Переопределяем MINOR и PATCH для LINK
#undef VERSION_MINOR
#undef VERSION_PATCH
#undef VERSION_STR
#undef STR
#undef STR_HELPER
#else
// Fallback если файл еще не скопирован
#define VERSION_MAJOR 1
#endif

// Собственные версии LINK
#define VERSION_MINOR 2
#define VERSION_PATCH 0

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)

// Дополнительные форматы версии для совместимости
#define VERSION_STRING VERSION_STR
#define VERSION_NUMBER ((VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | VERSION_PATCH)