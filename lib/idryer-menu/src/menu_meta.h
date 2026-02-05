// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu metadata only (no pointers to data or callbacks).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MENU_META_COUNT 200
#define MENU_LANG_COUNT 2

typedef enum
{
  META_SUBMENU = 0,
  META_ACTION = 1,
  META_VALUE = 2,
  META_TOGGLE = 3
} MenuMetaType;

typedef enum
{
  META_VT_F32 = 0,
  META_VT_U16 = 1,
  META_VT_U8 = 2,
  META_VT_I32 = 3,
  META_VT_BOOL = 4,
  META_VT_U32 = 5
} MenuMetaValueType;

typedef enum
{
  META_SCOPE_GLOBAL = 0,
  META_SCOPE_PER_UNIT = 1
} MenuMetaScope;

typedef struct
{
  uint16_t id;
  const char *title[MENU_LANG_COUNT];
  const char *unit[MENU_LANG_COUNT];
  MenuMetaType type;
  int16_t parent;
  int16_t first_child;
  uint16_t child_count;
  // Value/Toggle fields (ignored for submenu/action)
  MenuMetaValueType vtype;
  float min_val;
  float max_val;
  float step;
  MenuMetaScope scope;
} MenuMeta;

static const MenuMeta g_menu_meta[MENU_META_COUNT] = {
    // [0] root
    {0, {"МЕНЮ", "MENU"}, {nullptr, nullptr}, META_SUBMENU, -1, 1, 8, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [1] controller_choice
    {1, {"КОНТРОЛЛЕР", "CONTROLLER"}, {nullptr, nullptr}, META_VALUE, 0, -1, 0, META_VT_U8, 0.0f, 2.0f, 1.0f, META_SCOPE_GLOBAL},
    // [2] drying
    {2, {"СУШКА", "DRYING"}, {nullptr, nullptr}, META_SUBMENU, 0, 3, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [3] dry_temp
    {3, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 2, -1, 0, META_VT_F32, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [4] dry_time
    {4, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 2, -1, 0, META_VT_U16, 0.0f, 600.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [5] dry_start
    {5, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 2, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [6] storage
    {6, {"ХРАНЕНИЕ", "STORAGE"}, {nullptr, nullptr}, META_SUBMENU, 0, 7, 4, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [7] storage_temp
    {7, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 6, -1, 0, META_VT_U8, 35.0f, 90.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [8] storage_hum
    {8, {"ВЛАЖНОСТЬ", "HUMIDITY"}, {"%RH", "%RH"}, META_VALUE, 6, -1, 0, META_VT_U8, 5.0f, 30.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [9] storage_hum_priority
    {9, {"ПО ВЛАЖНОСТИ", "BY HUMIDITY"}, {nullptr, nullptr}, META_TOGGLE, 6, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [10] storage_start
    {10, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 6, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [11] profile
    {11, {"ПРОФИЛЬ", "PROFILE"}, {nullptr, nullptr}, META_SUBMENU, 0, 12, 11, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [12] profile_start
    {12, {"СТАРТ ПРОФИЛЯ", "PROFILE START"}, {nullptr, nullptr}, META_SUBMENU, 11, 13, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [13] start_stage_by_number
    {13, {"ПЕРВЫЙ ЭТАП", "FIRST STAGE"}, {"", ""}, META_VALUE, 12, -1, 0, META_VT_U8, 1.0f, 10.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [14] start_stage
    {14, {"СТАРТ ПРОФИЛЯ", "PROFILE START"}, {nullptr, nullptr}, META_ACTION, 12, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [15] stage_01
    {15, {"ЭТАП 01", "STAGE 01"}, {nullptr, nullptr}, META_SUBMENU, 11, 16, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [16] stage_01_temp
    {16, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 15, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [17] stage_01_ramps
    {17, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 15, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [18] stage_01_hold
    {18, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 15, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [19] stage_02
    {19, {"ЭТАП 02", "STAGE 02"}, {nullptr, nullptr}, META_SUBMENU, 11, 20, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [20] stage_02_temp
    {20, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 19, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [21] stage_02_ramps
    {21, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 19, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [22] stage_02_hold
    {22, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 19, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [23] stage_03
    {23, {"ЭТАП 03", "STAGE 03"}, {nullptr, nullptr}, META_SUBMENU, 11, 24, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [24] stage_03_temp
    {24, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 23, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [25] stage_03_ramps
    {25, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 23, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [26] stage_03_hold
    {26, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 23, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [27] stage_04
    {27, {"ЭТАП 04", "STAGE 04"}, {nullptr, nullptr}, META_SUBMENU, 11, 28, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [28] stage_04_temp
    {28, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 27, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [29] stage_04_ramps
    {29, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 27, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [30] stage_04_hold
    {30, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 27, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [31] stage_05
    {31, {"ЭТАП 05", "STAGE 05"}, {nullptr, nullptr}, META_SUBMENU, 11, 32, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [32] stage_05_temp
    {32, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 31, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [33] stage_05_ramps
    {33, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 31, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [34] stage_05_hold
    {34, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 31, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [35] stage_06
    {35, {"ЭТАП 06", "STAGE 06"}, {nullptr, nullptr}, META_SUBMENU, 11, 36, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [36] stage_06_temp
    {36, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 35, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [37] stage_06_ramps
    {37, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 35, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [38] stage_06_hold
    {38, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 35, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [39] stage_07
    {39, {"ЭТАП 07", "STAGE 07"}, {nullptr, nullptr}, META_SUBMENU, 11, 40, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [40] stage_07_temp
    {40, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 39, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [41] stage_07_ramps
    {41, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 39, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [42] stage_07_hold
    {42, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 39, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [43] stage_08
    {43, {"ЭТАП 08", "STAGE 08"}, {nullptr, nullptr}, META_SUBMENU, 11, 44, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [44] stage_08_temp
    {44, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 43, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [45] stage_08_ramps
    {45, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 43, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [46] stage_08_hold
    {46, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 43, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [47] stage_09
    {47, {"ЭТАП 09", "STAGE 09"}, {nullptr, nullptr}, META_SUBMENU, 11, 48, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [48] stage_09_temp
    {48, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 47, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [49] stage_09_ramps
    {49, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 47, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [50] stage_09_hold
    {50, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 47, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [51] stage_10
    {51, {"ЭТАП 10", "STAGE 10"}, {nullptr, nullptr}, META_SUBMENU, 11, 52, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [52] stage_10_temp
    {52, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 51, -1, 0, META_VT_U8, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [53] stage_10_ramps
    {53, {"ПОДЪЕМ", "RAMP"}, {"мин", "min"}, META_VALUE, 51, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [54] stage_10_hold
    {54, {"УДЕРЖАНИЕ", "HOLD"}, {"мин", "min"}, META_VALUE, 51, -1, 0, META_VT_U16, 0.0f, 1200.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [55] presets
    {55, {"ПРЕСЕТЫ", "PRESETS"}, {nullptr, nullptr}, META_SUBMENU, 0, 56, 17, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [56] preset_pla
    {56, {"PLA", "PLA"}, {nullptr, nullptr}, META_SUBMENU, 55, 57, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [57] preset_pla_temp
    {57, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 56, -1, 0, META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL},
    // [58] preset_pla_time
    {58, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 56, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [59] preset_pla_start
    {59, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 56, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [60] preset_pla_cf
    {60, {"PLA-CF", "PLA-CF"}, {nullptr, nullptr}, META_SUBMENU, 55, 61, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [61] preset_pla_cf_temp
    {61, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 60, -1, 0, META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL},
    // [62] preset_pla_cf_time
    {62, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 60, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [63] preset_pla_cf_start
    {63, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 60, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [64] preset_pla_gf
    {64, {"PLA-GF", "PLA-GF"}, {nullptr, nullptr}, META_SUBMENU, 55, 65, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [65] preset_pla_gf_temp
    {65, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 64, -1, 0, META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL},
    // [66] preset_pla_gf_time
    {66, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 64, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [67] preset_pla_gf_start
    {67, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 64, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [68] preset_petg
    {68, {"PETG", "PETG"}, {nullptr, nullptr}, META_SUBMENU, 55, 69, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [69] preset_petg_temp
    {69, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 68, -1, 0, META_VT_F32, 50.0f, 70.0f, 1.0f, META_SCOPE_GLOBAL},
    // [70] preset_petg_time
    {70, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 68, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [71] preset_petg_start
    {71, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 68, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [72] preset_petg_cf
    {72, {"PETG-CF", "PETG-CF"}, {nullptr, nullptr}, META_SUBMENU, 55, 73, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [73] preset_petg_cf_temp
    {73, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 72, -1, 0, META_VT_F32, 55.0f, 75.0f, 1.0f, META_SCOPE_GLOBAL},
    // [74] preset_petg_cf_time
    {74, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 72, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [75] preset_petg_cf_start
    {75, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 72, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [76] preset_petg_gf
    {76, {"PETG-GF", "PETG-GF"}, {nullptr, nullptr}, META_SUBMENU, 55, 77, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [77] preset_petg_gf_temp
    {77, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 76, -1, 0, META_VT_F32, 50.0f, 70.0f, 1.0f, META_SCOPE_GLOBAL},
    // [78] preset_petg_gf_time
    {78, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 76, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [79] preset_petg_gf_start
    {79, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 76, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [80] preset_abs
    {80, {"ABS", "ABS"}, {nullptr, nullptr}, META_SUBMENU, 55, 81, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [81] preset_abs_temp
    {81, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 80, -1, 0, META_VT_F32, 70.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL},
    // [82] preset_abs_time
    {82, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 80, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [83] preset_abs_start
    {83, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 80, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [84] preset_abs_cf
    {84, {"ABS-CF", "ABS-CF"}, {nullptr, nullptr}, META_SUBMENU, 55, 85, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [85] preset_abs_cf_temp
    {85, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 84, -1, 0, META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL},
    // [86] preset_abs_cf_time
    {86, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 84, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [87] preset_abs_cf_start
    {87, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 84, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [88] preset_abs_gf
    {88, {"ABS-GF", "ABS-GF"}, {nullptr, nullptr}, META_SUBMENU, 55, 89, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [89] preset_abs_gf_temp
    {89, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 88, -1, 0, META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL},
    // [90] preset_abs_gf_time
    {90, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 88, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [91] preset_abs_gf_start
    {91, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 88, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [92] preset_pa
    {92, {"PA", "PA"}, {nullptr, nullptr}, META_SUBMENU, 55, 93, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [93] preset_pa_temp
    {93, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 92, -1, 0, META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL},
    // [94] preset_pa_time
    {94, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 92, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [95] preset_pa_start
    {95, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 92, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [96] preset_pa_cf
    {96, {"PA-CF", "PA-CF"}, {nullptr, nullptr}, META_SUBMENU, 55, 97, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [97] preset_pa_cf_temp
    {97, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 96, -1, 0, META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL},
    // [98] preset_pa_cf_time
    {98, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 96, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [99] preset_pa_cf_start
    {99, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 96, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [100] preset_pa_gf
    {100, {"PA-GF", "PA-GF"}, {nullptr, nullptr}, META_SUBMENU, 55, 101, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [101] preset_pa_gf_temp
    {101, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 100, -1, 0, META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL},
    // [102] preset_pa_gf_time
    {102, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 100, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [103] preset_pa_gf_start
    {103, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 100, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [104] preset_pc
    {104, {"PC", "PC"}, {nullptr, nullptr}, META_SUBMENU, 55, 105, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [105] preset_pc_temp
    {105, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 104, -1, 0, META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL},
    // [106] preset_pc_time
    {106, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 104, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [107] preset_pc_start
    {107, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 104, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [108] preset_pc_cf
    {108, {"PC-CF", "PC-CF"}, {nullptr, nullptr}, META_SUBMENU, 55, 109, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [109] preset_pc_cf_temp
    {109, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 108, -1, 0, META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL},
    // [110] preset_pc_cf_time
    {110, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 108, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [111] preset_pc_cf_start
    {111, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 108, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [112] preset_my1
    {112, {"MY1", "MY1"}, {nullptr, nullptr}, META_SUBMENU, 55, 113, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [113] preset_my1_temp
    {113, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 112, -1, 0, META_VT_F32, 60.0f, 80.0f, 1.0f, META_SCOPE_GLOBAL},
    // [114] preset_my1_time
    {114, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 112, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [115] preset_my1_start
    {115, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 112, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [116] preset_my2
    {116, {"MY2", "MY2"}, {nullptr, nullptr}, META_SUBMENU, 55, 117, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [117] preset_my2_temp
    {117, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 116, -1, 0, META_VT_F32, 70.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL},
    // [118] preset_my2_time
    {118, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 116, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [119] preset_my2_start
    {119, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 116, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [120] preset_my3
    {120, {"MY3", "MY3"}, {nullptr, nullptr}, META_SUBMENU, 55, 121, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [121] preset_my3_temp
    {121, {"ТЕМПЕРАТУРА", "TEMPERATURE"}, {"°C", "°C"}, META_VALUE, 120, -1, 0, META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL},
    // [122] preset_my3_time
    {122, {"ВРЕМЯ", "TIME"}, {"мин", "min"}, META_VALUE, 120, -1, 0, META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL},
    // [123] preset_my3_start
    {123, {"СТАРТ", "START"}, {nullptr, nullptr}, META_ACTION, 120, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [124] scales
    {124, {"ВЕСЫ", "SCALES"}, {nullptr, nullptr}, META_SUBMENU, 0, 125, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [125] scales_count
    {125, {"КОЛ-ВО МОДУЛЕЙ", "NUMBER OF MODULES"}, {nullptr, nullptr}, META_VALUE, 124, -1, 0, META_VT_U8, 0.0f, 4.0f, 1.0f, META_SCOPE_GLOBAL},
    // [126] tare
    {126, {"ТАРА", "TARE"}, {nullptr, nullptr}, META_SUBMENU, 124, 127, 4, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [127] tare_spool1
    {127, {"КАТУШКА 1", "SPOOL 1"}, {"г", "g"}, META_VALUE, 126, -1, 0, META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL},
    // [128] tare_spool2
    {128, {"КАТУШКА 2", "SPOOL 2"}, {"г", "g"}, META_VALUE, 126, -1, 0, META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL},
    // [129] tare_spool3
    {129, {"КАТУШКА 3", "SPOOL 3"}, {"г", "g"}, META_VALUE, 126, -1, 0, META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL},
    // [130] tare_spool4
    {130, {"КАТУШКА 4", "SPOOL 4"}, {"г", "g"}, META_VALUE, 126, -1, 0, META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL},
    // [131] calibration
    {131, {"КАЛИБРОВКА", "CALIBRATION"}, {nullptr, nullptr}, META_SUBMENU, 124, 132, 4, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [132] calib_spool1
    {132, {"КАТУШКА 1", "SPOOL 1"}, {nullptr, nullptr}, META_SUBMENU, 131, 133, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [133] calib_zero1
    {133, {"ПРИНЯТЬ НОЛЬ", "SET ZERO"}, {nullptr, nullptr}, META_ACTION, 132, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [134] calib_kg_1
    {134, {"ПРИНЯТЬ 1000 Г", "SET 1000 G"}, {nullptr, nullptr}, META_ACTION, 132, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [135] calib_spool2
    {135, {"КАТУШКА 2", "SPOOL 2"}, {nullptr, nullptr}, META_SUBMENU, 131, 136, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [136] calib_zero2
    {136, {"ПРИНЯТЬ НОЛЬ", "SET ZERO"}, {nullptr, nullptr}, META_ACTION, 135, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [137] calib_kg_2
    {137, {"ПРИНЯТЬ 1000 Г", "SET 1000 G"}, {nullptr, nullptr}, META_ACTION, 135, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [138] calib_spool3
    {138, {"КАТУШКА 3", "SPOOL 3"}, {nullptr, nullptr}, META_SUBMENU, 131, 139, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [139] calib_zero3
    {139, {"ПРИНЯТЬ НОЛЬ", "SET ZERO"}, {nullptr, nullptr}, META_ACTION, 138, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [140] calib_kg_3
    {140, {"ПРИНЯТЬ 1000 Г", "SET 1000 G"}, {nullptr, nullptr}, META_ACTION, 138, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [141] calib_spool4
    {141, {"КАТУШКА 4", "SPOOL 4"}, {nullptr, nullptr}, META_SUBMENU, 131, 142, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [142] calib_zero4
    {142, {"ПРИНЯТЬ НОЛЬ", "SET ZERO"}, {nullptr, nullptr}, META_ACTION, 141, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [143] calib_kg_4
    {143, {"ПРИНЯТЬ 1000 Г", "SET 1000 G"}, {nullptr, nullptr}, META_ACTION, 141, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [144] settings
    {144, {"НАСТРОЙКИ", "SETTINGS"}, {nullptr, nullptr}, META_SUBMENU, 0, 145, 6, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [145] storage_logic
    {145, {"ХРАНЕНИЕ", "STORAGE"}, {nullptr, nullptr}, META_SUBMENU, 144, 146, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [146] storage_auto
    {146, {"АВТО ХРАНЕНИЕ", "AUTO STORAGE"}, {nullptr, nullptr}, META_TOGGLE, 145, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [147] storage_auto_dry
    {147, {"АВТО СУШКА", "AUTO DRY"}, {nullptr, nullptr}, META_TOGGLE, 145, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [148] storage_rh
    {148, {"ПО ВЛАЖНОСТИ", "BY HUMID"}, {nullptr, nullptr}, META_SUBMENU, 145, 149, 1, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [149] storage_rh_hyst
    {149, {"ГИСТЕРЕЗИС %", "HYSTERESIS %"}, {"%", "%"}, META_VALUE, 148, -1, 0, META_VT_U8, 1.0f, 10.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [150] storage_temp_setup
    {150, {"ПО ТЕМПЕРАТУРЕ", "BY TEMP"}, {nullptr, nullptr}, META_SUBMENU, 145, 151, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [151] storage_dry_temp_mode
    {151, {"АБС/%СУШКИ", "ABS/%DRY"}, {nullptr, nullptr}, META_TOGGLE, 150, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [152] storage_dry_temp_by_dry_temp
    {152, {"% ОТ T СУШКИ", "% OF DRY TEMP"}, {"%", "%"}, META_VALUE, 150, -1, 0, META_VT_U8, 50.0f, 100.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [153] storage_common
    {153, {"ОБЩЕЕ", "COMMON"}, {nullptr, nullptr}, META_SUBMENU, 145, 154, 1, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [154] storage_min_hold_sec
    {154, {"МИН. СОСТОЯНИЕ", "MIN HOLD"}, {"с", "s"}, META_VALUE, 153, -1, 0, META_VT_U16, 5.0f, 600.0f, 5.0f, META_SCOPE_PER_UNIT},
    // [155] PID_heater
    {155, {"ПИД НАГРЕВАТЕЛЬ", "PID HEATER"}, {nullptr, nullptr}, META_SUBMENU, 144, 156, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [156] pid_kp_heater
    {156, {"КП", "Kp"}, {nullptr, nullptr}, META_VALUE, 155, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT},
    // [157] pid_ki_heater
    {157, {"КИ", "Ki"}, {nullptr, nullptr}, META_VALUE, 155, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.01f, META_SCOPE_PER_UNIT},
    // [158] pid_kd_heater
    {158, {"КД", "Kd"}, {nullptr, nullptr}, META_VALUE, 155, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT},
    // [159] pid_gain_heater
    {159, {"GAIN", "GAIN"}, {"×", "×"}, META_VALUE, 155, -1, 0, META_VT_F32, 0.1f, 50.0f, 0.01f, META_SCOPE_PER_UNIT},
    // [160] pid_autotune_heater
    {160, {"Автопид", "Autopid"}, {nullptr, nullptr}, META_ACTION, 155, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [161] PID_chamber
    {161, {"ПИД ВОЗДУХ", "PID CHAMBER"}, {nullptr, nullptr}, META_SUBMENU, 144, 162, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [162] pid_kp_chamber
    {162, {"КП", "Kp"}, {nullptr, nullptr}, META_VALUE, 161, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT},
    // [163] pid_ki_chamber
    {163, {"КИ", "Ki"}, {nullptr, nullptr}, META_VALUE, 161, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.01f, META_SCOPE_PER_UNIT},
    // [164] pid_kd_chamber
    {164, {"КД", "Kd"}, {nullptr, nullptr}, META_VALUE, 161, -1, 0, META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT},
    // [165] pid_gain_chamber
    {165, {"GAIN", "GAIN"}, {"×", "×"}, META_VALUE, 161, -1, 0, META_VT_F32, 0.1f, 200.0f, 0.01f, META_SCOPE_PER_UNIT},
    // [166] pid_autotune_chamber
    {166, {"Автопид", "Autopid"}, {nullptr, nullptr}, META_ACTION, 161, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [167] heating
    {167, {"НАГРЕВ", "HEATING"}, {nullptr, nullptr}, META_SUBMENU, 144, 168, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [168] heater_max_temp
    {168, {"МАКС.НАГРЕВАТЕЛЬ", "MAX TEMP"}, {"°C", "°C"}, META_VALUE, 167, -1, 0, META_VT_F32, 30.0f, 150.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [169] air_max_temp
    {169, {"МАКС.ТЕМП.ВОЗДУХ", "MAX AIR TEMP"}, {"°C", "°C"}, META_VALUE, 167, -1, 0, META_VT_F32, 30.0f, 120.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [170] delta
    {170, {"ДЕЛЬТА", "DELTA"}, {"°C", "°C"}, META_VALUE, 167, -1, 0, META_VT_U8, 0.0f, 45.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [171] heater_delta_is_percent
    {171, {"ДЕЛЬТА АБС/%", "DELTA ABS/%"}, {nullptr, nullptr}, META_TOGGLE, 167, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [172] heating_check
    {172, {"ПРОВЕРКА", "CHECK"}, {nullptr, nullptr}, META_SUBMENU, 167, 173, 4, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [173] vh_max_err
    {173, {"ПОРОГ ОШИБКИ", "ERROR LIMIT"}, {nullptr, nullptr}, META_VALUE, 172, -1, 0, META_VT_F32, 0.0f, 500.0f, 10.0f, META_SCOPE_PER_UNIT},
    // [174] vh_heating_gain
    {174, {"ПРИРОСТ T", "TEMP GAIN"}, {"°C", "°C"}, META_VALUE, 172, -1, 0, META_VT_F32, 0.1f, 10.0f, 0.1f, META_SCOPE_PER_UNIT},
    // [175] vh_check_gain_time
    {175, {"ОКНО ПРОВЕРКИ", "CHECK WINDOW"}, {"с", "s"}, META_VALUE, 172, -1, 0, META_VT_U16, 5.0f, 120.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [176] vh_arm_pwm_min
    {176, {"ШИМ СТАРТ %", "PWM START %"}, {"%", "%"}, META_VALUE, 172, -1, 0, META_VT_U8, 10.0f, 100.0f, 5.0f, META_SCOPE_PER_UNIT},
    // [177] fan
    {177, {"ВЕНТИЛЯТОР", "FAN"}, {nullptr, nullptr}, META_SUBMENU, 144, 178, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [178] fan_temp_on
    {178, {"ПОРОГ T ВКЛ", "ON THRESHOLD"}, {"°C", "°C"}, META_VALUE, 177, -1, 0, META_VT_F32, 40.0f, 70.0f, 0.5f, META_SCOPE_PER_UNIT},
    // [179] fan_hyst
    {179, {"ГИСТЕРЕЗИС", "HYSTERESIS"}, {"°C", "°C"}, META_VALUE, 177, -1, 0, META_VT_F32, 5.0f, 20.0f, 0.5f, META_SCOPE_PER_UNIT},
    // [180] servo
    {180, {"СЕРВО", "SERVO"}, {nullptr, nullptr}, META_SUBMENU, 144, 181, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [181] servo_closed_angle
    {181, {"ЗАКРЫТО УГОЛ", "CLOSED ANGLE"}, {"°", "°"}, META_VALUE, 180, -1, 0, META_VT_U8, 0.0f, 180.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [182] servo_open_angle
    {182, {"ОТКРЫТО УГОЛ", "OPEN ANGLE"}, {"°", "°"}, META_VALUE, 180, -1, 0, META_VT_U8, 0.0f, 180.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [183] servo_time_closed
    {183, {"ВРЕМЯ ЗАКРЫТО", "CLOSED TIME"}, {"с", "s"}, META_VALUE, 180, -1, 0, META_VT_U16, 0.0f, 3600.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [184] servo_time_open
    {184, {"ВРЕМЯ ОТКРЫТО", "OPEN TIME"}, {"с", "s"}, META_VALUE, 180, -1, 0, META_VT_U16, 0.0f, 600.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [185] servo_smart_mode
    {185, {"УМНЫЙ РЕЖИМ", "SMART MODE"}, {nullptr, nullptr}, META_TOGGLE, 180, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT},
    // [186] global_settings
    {186, {"ОБЩИЕ", "GLOBAL"}, {nullptr, nullptr}, META_SUBMENU, 0, 187, 5, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [187] piortal
    {187, {"ПОРТАЛ", "PORTAL"}, {nullptr, nullptr}, META_SUBMENU, 186, 188, 2, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [188] start_claim
    {188, {"СВЯЗАТЬ", "CLAIM"}, {nullptr, nullptr}, META_ACTION, 187, -1, 0, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [189] cmd_ignore_external
    {189, {"ИГНОР КОМАНД", "IGNOR EXT CMD"}, {nullptr, nullptr}, META_TOGGLE, 187, -1, 0, META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL},
    // [190] session_count
    {190, {"СЧЕТЧИК СЕССИЙ", "SESSION COUNTER"}, {nullptr, nullptr}, META_SUBMENU, 186, 191, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT},
    // [191] drying_session_count
    {191, {"СУШКА", "DRYING"}, {nullptr, nullptr}, META_VALUE, 190, -1, 0, META_VT_U16, 0.0f, 65535.0f, 1.0f, META_SCOPE_GLOBAL},
    // [192] storage_session_count
    {192, {"ХРАНЕНИЕ", "STORAGE"}, {nullptr, nullptr}, META_VALUE, 190, -1, 0, META_VT_U16, 0.0f, 65535.0f, 1.0f, META_SCOPE_GLOBAL},
    // [193] profile_session_count
    {193, {"ПРОФИЛЬ", "PROFILE"}, {nullptr, nullptr}, META_VALUE, 190, -1, 0, META_VT_U16, 0.0f, 65535.0f, 1.0f, META_SCOPE_GLOBAL},
    // [194] hardware_config
    {194, {"КОНФИГУРАЦИЯ ПОРТОВ", "PORT CONFIG"}, {nullptr, nullptr}, META_SUBMENU, 186, 195, 3, META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL},
    // [195] port1_mode
    {195, {"ПОРТ 1", "PORT 1"}, {nullptr, nullptr}, META_VALUE, 194, -1, 0, META_VT_U8, 0.0f, 3.0f, 1.0f, META_SCOPE_GLOBAL},
    // [196] port2_mode
    {196, {"ПОРТ 2", "PORT 2"}, {nullptr, nullptr}, META_VALUE, 194, -1, 0, META_VT_U8, 0.0f, 3.0f, 1.0f, META_SCOPE_GLOBAL},
    // [197] port3_mode
    {197, {"ПОРТ 3", "PORT 3"}, {nullptr, nullptr}, META_VALUE, 194, -1, 0, META_VT_U8, 0.0f, 3.0f, 1.0f, META_SCOPE_GLOBAL},
    // [198] units_count
    {198, {"КОЛ-ВО ЮНИТОВ", "UNITS"}, {nullptr, nullptr}, META_VALUE, 186, -1, 0, META_VT_U8, 1.0f, 3.0f, 1.0f, META_SCOPE_GLOBAL},
    // [199] language
    {199, {"ЯЗЫК", "LANGUAGE"}, {nullptr, nullptr}, META_VALUE, 186, -1, 0, META_VT_U8, 0.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL},
};

// Get menu item by id
static inline const MenuMeta *menu_meta_get(uint16_t id)
{
  if (id < MENU_META_COUNT)
    return &g_menu_meta[id];
  return nullptr;
}
