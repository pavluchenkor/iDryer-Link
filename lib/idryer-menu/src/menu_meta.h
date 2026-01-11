// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu metadata only (no pointers to data or callbacks).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MENU_META_COUNT 146
#define MENU_LANG_COUNT 2

typedef enum {
    META_SUBMENU = 0,
    META_ACTION = 1,
    META_VALUE = 2,
    META_TOGGLE = 3
} MenuMetaType;

typedef enum {
    META_VT_F32 = 0,
    META_VT_U16 = 1,
    META_VT_U8 = 2,
    META_VT_I32 = 3,
    META_VT_BOOL = 4,
    META_VT_U32 = 5
} MenuMetaValueType;

typedef enum {
    META_SCOPE_GLOBAL = 0,
    META_SCOPE_PER_UNIT = 1
} MenuMetaScope;

typedef struct {
    uint16_t id;
    const char* title[MENU_LANG_COUNT];
    const char* unit[MENU_LANG_COUNT];
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
    { 0, { "МЕНЮ", "MENU" }, { nullptr, nullptr },
      META_SUBMENU, -1, 1, 7,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [1] controller_choice
    { 1, { "КОНТРОЛЛЕР", "CONTROLLER" }, { nullptr, nullptr },
      META_VALUE, 0, -1, 0,
      META_VT_U8, 0.0f, 2.0f, 1.0f, META_SCOPE_GLOBAL },
    // [2] drying
    { 2, { "СУШКА", "DRYING" }, { nullptr, nullptr },
      META_SUBMENU, 0, 3, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [3] dry_temp
    { 3, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 2, -1, 0,
      META_VT_F32, 30.0f, 110.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [4] dry_time
    { 4, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 2, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [5] dry_start
    { 5, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 2, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [6] storage
    { 6, { "ХРАНЕНИЕ", "STORAGE" }, { nullptr, nullptr },
      META_SUBMENU, 0, 7, 4,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [7] storage_temp
    { 7, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 6, -1, 0,
      META_VT_U8, 35.0f, 90.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [8] storage_hum
    { 8, { "ВЛАЖНОСТЬ", "HUMIDITY" }, { "%RH", "%RH" },
      META_VALUE, 6, -1, 0,
      META_VT_U8, 5.0f, 30.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [9] storage_hum_priority
    { 9, { "ПО ВЛАЖНОСТИ", "BY HUMIDITY" }, { nullptr, nullptr },
      META_TOGGLE, 6, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [10] storage_start
    { 10, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 6, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [11] presets
    { 11, { "ПРЕСЕТЫ", "PRESETS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 12, 17,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [12] preset_pla
    { 12, { "PLA", "PLA" }, { nullptr, nullptr },
      META_SUBMENU, 11, 13, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [13] preset_pla_temp
    { 13, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 12, -1, 0,
      META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL },
    // [14] preset_pla_time
    { 14, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 12, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [15] preset_pla_start
    { 15, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 12, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [16] preset_pla_cf
    { 16, { "PLA-CF", "PLA-CF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 17, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [17] preset_pla_cf_temp
    { 17, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 16, -1, 0,
      META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL },
    // [18] preset_pla_cf_time
    { 18, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 16, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [19] preset_pla_cf_start
    { 19, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 16, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [20] preset_pla_gf
    { 20, { "PLA-GF", "PLA-GF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 21, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [21] preset_pla_gf_temp
    { 21, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 20, -1, 0,
      META_VT_F32, 35.0f, 55.0f, 1.0f, META_SCOPE_GLOBAL },
    // [22] preset_pla_gf_time
    { 22, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 20, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [23] preset_pla_gf_start
    { 23, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 20, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [24] preset_petg
    { 24, { "PETG", "PETG" }, { nullptr, nullptr },
      META_SUBMENU, 11, 25, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [25] preset_petg_temp
    { 25, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 24, -1, 0,
      META_VT_F32, 50.0f, 70.0f, 1.0f, META_SCOPE_GLOBAL },
    // [26] preset_petg_time
    { 26, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 24, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [27] preset_petg_start
    { 27, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 24, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [28] preset_petg_cf
    { 28, { "PETG-CF", "PETG-CF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 29, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [29] preset_petg_cf_temp
    { 29, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 28, -1, 0,
      META_VT_F32, 55.0f, 75.0f, 1.0f, META_SCOPE_GLOBAL },
    // [30] preset_petg_cf_time
    { 30, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 28, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [31] preset_petg_cf_start
    { 31, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 28, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [32] preset_petg_gf
    { 32, { "PETG-GF", "PETG-GF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 33, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [33] preset_petg_gf_temp
    { 33, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 32, -1, 0,
      META_VT_F32, 50.0f, 70.0f, 1.0f, META_SCOPE_GLOBAL },
    // [34] preset_petg_gf_time
    { 34, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 32, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [35] preset_petg_gf_start
    { 35, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 32, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [36] preset_abs
    { 36, { "ABS", "ABS" }, { nullptr, nullptr },
      META_SUBMENU, 11, 37, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [37] preset_abs_temp
    { 37, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 36, -1, 0,
      META_VT_F32, 70.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [38] preset_abs_time
    { 38, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 36, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [39] preset_abs_start
    { 39, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 36, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [40] preset_abs_cf
    { 40, { "ABS-CF", "ABS-CF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 41, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [41] preset_abs_cf_temp
    { 41, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 40, -1, 0,
      META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL },
    // [42] preset_abs_cf_time
    { 42, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 40, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [43] preset_abs_cf_start
    { 43, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 40, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [44] preset_abs_gf
    { 44, { "ABS-GF", "ABS-GF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 45, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [45] preset_abs_gf_temp
    { 45, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 44, -1, 0,
      META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL },
    // [46] preset_abs_gf_time
    { 46, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 44, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [47] preset_abs_gf_start
    { 47, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 44, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [48] preset_pa
    { 48, { "PA", "PA" }, { nullptr, nullptr },
      META_SUBMENU, 11, 49, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [49] preset_pa_temp
    { 49, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 48, -1, 0,
      META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL },
    // [50] preset_pa_time
    { 50, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 48, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [51] preset_pa_start
    { 51, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 48, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [52] preset_pa_cf
    { 52, { "PA-CF", "PA-CF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 53, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [53] preset_pa_cf_temp
    { 53, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 52, -1, 0,
      META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL },
    // [54] preset_pa_cf_time
    { 54, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 52, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [55] preset_pa_cf_start
    { 55, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 52, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [56] preset_pa_gf
    { 56, { "PA-GF", "PA-GF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 57, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [57] preset_pa_gf_temp
    { 57, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 56, -1, 0,
      META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL },
    // [58] preset_pa_gf_time
    { 58, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 56, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [59] preset_pa_gf_start
    { 59, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 56, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [60] preset_pc
    { 60, { "PC", "PC" }, { nullptr, nullptr },
      META_SUBMENU, 11, 61, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [61] preset_pc_temp
    { 61, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 60, -1, 0,
      META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL },
    // [62] preset_pc_time
    { 62, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 60, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [63] preset_pc_start
    { 63, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 60, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [64] preset_pc_cf
    { 64, { "PC-CF", "PC-CF" }, { nullptr, nullptr },
      META_SUBMENU, 11, 65, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [65] preset_pc_cf_temp
    { 65, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 64, -1, 0,
      META_VT_F32, 90.0f, 110.0f, 1.0f, META_SCOPE_GLOBAL },
    // [66] preset_pc_cf_time
    { 66, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 64, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [67] preset_pc_cf_start
    { 67, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 64, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [68] preset_my1
    { 68, { "MY1", "MY1" }, { nullptr, nullptr },
      META_SUBMENU, 11, 69, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [69] preset_my1_temp
    { 69, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 68, -1, 0,
      META_VT_F32, 60.0f, 80.0f, 1.0f, META_SCOPE_GLOBAL },
    // [70] preset_my1_time
    { 70, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 68, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [71] preset_my1_start
    { 71, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 68, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [72] preset_my2
    { 72, { "MY2", "MY2" }, { nullptr, nullptr },
      META_SUBMENU, 11, 73, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [73] preset_my2_temp
    { 73, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 72, -1, 0,
      META_VT_F32, 70.0f, 90.0f, 1.0f, META_SCOPE_GLOBAL },
    // [74] preset_my2_time
    { 74, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 72, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [75] preset_my2_start
    { 75, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 72, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [76] preset_my3
    { 76, { "MY3", "MY3" }, { nullptr, nullptr },
      META_SUBMENU, 11, 77, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [77] preset_my3_temp
    { 77, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
      META_VALUE, 76, -1, 0,
      META_VT_F32, 80.0f, 100.0f, 1.0f, META_SCOPE_GLOBAL },
    // [78] preset_my3_time
    { 78, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
      META_VALUE, 76, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 5.0f, META_SCOPE_GLOBAL },
    // [79] preset_my3_start
    { 79, { "СТАРТ", "START" }, { nullptr, nullptr },
      META_ACTION, 76, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [80] scales
    { 80, { "ВЕСЫ", "SCALES" }, { nullptr, nullptr },
      META_SUBMENU, 0, 81, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [81] scales_count
    { 81, { "КОЛ-ВО МОДУЛЕЙ", "NUMBER OF MODULES" }, { nullptr, nullptr },
      META_VALUE, 80, -1, 0,
      META_VT_U8, 0.0f, 4.0f, 1.0f, META_SCOPE_GLOBAL },
    // [82] tare
    { 82, { "ТАРА", "TARE" }, { nullptr, nullptr },
      META_SUBMENU, 80, 83, 4,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [83] tare_spool1
    { 83, { "КАТУШКА 1", "SPOOL 1" }, { "г", "g" },
      META_VALUE, 82, -1, 0,
      META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL },
    // [84] tare_spool2
    { 84, { "КАТУШКА 2", "SPOOL 2" }, { "г", "g" },
      META_VALUE, 82, -1, 0,
      META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL },
    // [85] tare_spool3
    { 85, { "КАТУШКА 3", "SPOOL 3" }, { "г", "g" },
      META_VALUE, 82, -1, 0,
      META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL },
    // [86] tare_spool4
    { 86, { "КАТУШКА 4", "SPOOL 4" }, { "г", "g" },
      META_VALUE, 82, -1, 0,
      META_VT_F32, 0.0f, 2000.0f, 1.0f, META_SCOPE_GLOBAL },
    // [87] calibration
    { 87, { "КАЛИБРОВКА", "CALIBRATION" }, { nullptr, nullptr },
      META_SUBMENU, 80, 88, 4,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [88] calib_spool1
    { 88, { "КАТУШКА 1", "SPOOL 1" }, { nullptr, nullptr },
      META_SUBMENU, 87, 89, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [89] calib_zero1
    { 89, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
      META_ACTION, 88, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [90] calib_kg_1
    { 90, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
      META_ACTION, 88, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [91] calib_spool2
    { 91, { "КАТУШКА 2", "SPOOL 2" }, { nullptr, nullptr },
      META_SUBMENU, 87, 92, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [92] calib_zero2
    { 92, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
      META_ACTION, 91, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [93] calib_kg_2
    { 93, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
      META_ACTION, 91, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [94] calib_spool3
    { 94, { "КАТУШКА 3", "SPOOL 3" }, { nullptr, nullptr },
      META_SUBMENU, 87, 95, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [95] calib_zero3
    { 95, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
      META_ACTION, 94, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [96] calib_kg_3
    { 96, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
      META_ACTION, 94, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [97] calib_spool4
    { 97, { "КАТУШКА 4", "SPOOL 4" }, { nullptr, nullptr },
      META_SUBMENU, 87, 98, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [98] calib_zero4
    { 98, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
      META_ACTION, 97, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [99] calib_kg_4
    { 99, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
      META_ACTION, 97, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_GLOBAL },
    // [100] settings
    { 100, { "НАСТРОЙКИ", "SETTINGS" }, { nullptr, nullptr },
      META_SUBMENU, 0, 101, 6,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [101] storage_logic
    { 101, { "ХРАНЕНИЕ", "STORAGE" }, { nullptr, nullptr },
      META_SUBMENU, 100, 102, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [102] storage_auto
    { 102, { "АВТО ХРАНЕНИЕ", "AUTO STORAGE" }, { nullptr, nullptr },
      META_TOGGLE, 101, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [103] storage_auto_dry
    { 103, { "АВТО СУШКА", "AUTO DRY" }, { nullptr, nullptr },
      META_TOGGLE, 101, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [104] storage_rh
    { 104, { "ПО ВЛАЖНОСТИ", "BY HUMID" }, { nullptr, nullptr },
      META_SUBMENU, 101, 105, 1,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [105] storage_rh_hyst
    { 105, { "ГИСТЕРЕЗИС %", "HYSTERESIS %" }, { "%", "%" },
      META_VALUE, 104, -1, 0,
      META_VT_U8, 1.0f, 10.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [106] storage_temp_setup
    { 106, { "ПО ТЕМПЕРАТУРЕ", "BY TEMP" }, { nullptr, nullptr },
      META_SUBMENU, 101, 107, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [107] storage_dry_temp_mode
    { 107, { "АБС/%СУШКИ", "ABS/%DRY" }, { nullptr, nullptr },
      META_TOGGLE, 106, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [108] storage_dry_temp_by_dry_temp
    { 108, { "% ОТ T СУШКИ", "% OF DRY TEMP" }, { "%", "%" },
      META_VALUE, 106, -1, 0,
      META_VT_U8, 50.0f, 100.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [109] storage_common
    { 109, { "ОБЩЕЕ", "COMMON" }, { nullptr, nullptr },
      META_SUBMENU, 101, 110, 1,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [110] storage_min_hold_sec
    { 110, { "МИН. СОСТОЯНИЕ", "MIN HOLD" }, { "с", "s" },
      META_VALUE, 109, -1, 0,
      META_VT_U16, 5.0f, 600.0f, 5.0f, META_SCOPE_PER_UNIT },
    // [111] PID_heater
    { 111, { "ПИД НАГРЕВАТЕЛЬ", "PID HEATER" }, { nullptr, nullptr },
      META_SUBMENU, 100, 112, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [112] pid_kp_heater
    { 112, { "КП", "Kp" }, { nullptr, nullptr },
      META_VALUE, 111, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [113] pid_ki_heater
    { 113, { "КИ", "Ki" }, { nullptr, nullptr },
      META_VALUE, 111, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.01f, META_SCOPE_PER_UNIT },
    // [114] pid_kd_heater
    { 114, { "КД", "Kd" }, { nullptr, nullptr },
      META_VALUE, 111, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [115] pid_gain_heater
    { 115, { "GAIN", "GAIN" }, { "×", "×" },
      META_VALUE, 111, -1, 0,
      META_VT_F32, 0.1f, 20.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [116] pid_autotune_heater
    { 116, { "Автопид", "Autopid" }, { nullptr, nullptr },
      META_ACTION, 111, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [117] PID_chamber
    { 117, { "ПИД ВОЗДУХ", "PID CHAMBER" }, { nullptr, nullptr },
      META_SUBMENU, 100, 118, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [118] pid_kp_chamber
    { 118, { "КП", "Kp" }, { nullptr, nullptr },
      META_VALUE, 117, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [119] pid_ki_chamber
    { 119, { "КИ", "Ki" }, { nullptr, nullptr },
      META_VALUE, 117, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.01f, META_SCOPE_PER_UNIT },
    // [120] pid_kd_chamber
    { 120, { "КД", "Kd" }, { nullptr, nullptr },
      META_VALUE, 117, -1, 0,
      META_VT_F32, 0.0f, 1000.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [121] pid_gain_chamber
    { 121, { "GAIN", "GAIN" }, { "×", "×" },
      META_VALUE, 117, -1, 0,
      META_VT_F32, 0.1f, 100.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [122] pid_autotune_chamber
    { 122, { "Автопид", "Autopid" }, { nullptr, nullptr },
      META_ACTION, 117, -1, 0,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [123] heating
    { 123, { "НАГРЕВ", "HEATING" }, { nullptr, nullptr },
      META_SUBMENU, 100, 124, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [124] heater_max_temp
    { 124, { "МАКС.НАГРЕВАТЕЛЬ", "MAX TEMP" }, { "°C", "°C" },
      META_VALUE, 123, -1, 0,
      META_VT_F32, 30.0f, 150.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [125] air_max_temp
    { 125, { "МАКС.ТЕМП.ВОЗДУХ", "MAX AIR TEMP" }, { "°C", "°C" },
      META_VALUE, 123, -1, 0,
      META_VT_F32, 30.0f, 120.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [126] delta
    { 126, { "ДЕЛЬТА", "DELTA" }, { "°C", "°C" },
      META_VALUE, 123, -1, 0,
      META_VT_U8, 0.0f, 45.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [127] heater_delta_is_percent
    { 127, { "ДЕЛЬТА АБС/%", "DELTA ABS/%" }, { nullptr, nullptr },
      META_TOGGLE, 123, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [128] heating_check
    { 128, { "ПРОВЕРКА", "CHECK" }, { nullptr, nullptr },
      META_SUBMENU, 123, 129, 4,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [129] vh_max_err
    { 129, { "ПОРОГ ОШИБКИ", "ERROR LIMIT" }, { nullptr, nullptr },
      META_VALUE, 128, -1, 0,
      META_VT_F32, 0.0f, 500.0f, 10.0f, META_SCOPE_PER_UNIT },
    // [130] vh_heating_gain
    { 130, { "ПРИРОСТ T", "TEMP GAIN" }, { "°C", "°C" },
      META_VALUE, 128, -1, 0,
      META_VT_F32, 0.1f, 10.0f, 0.1f, META_SCOPE_PER_UNIT },
    // [131] vh_check_gain_time
    { 131, { "ОКНО ПРОВЕРКИ", "CHECK WINDOW" }, { "с", "s" },
      META_VALUE, 128, -1, 0,
      META_VT_U16, 5.0f, 120.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [132] vh_arm_pwm_min
    { 132, { "ШИМ СТАРТ %", "PWM START %" }, { "%", "%" },
      META_VALUE, 128, -1, 0,
      META_VT_U8, 10.0f, 100.0f, 5.0f, META_SCOPE_PER_UNIT },
    // [133] fan
    { 133, { "ВЕНТИЛЯТОР", "FAN" }, { nullptr, nullptr },
      META_SUBMENU, 100, 134, 2,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [134] fan_temp_on
    { 134, { "ПОРОГ T ВКЛ", "ON THRESHOLD" }, { "°C", "°C" },
      META_VALUE, 133, -1, 0,
      META_VT_F32, 40.0f, 70.0f, 0.5f, META_SCOPE_PER_UNIT },
    // [135] fan_hyst
    { 135, { "ГИСТЕРЕЗИС", "HYSTERESIS" }, { "°C", "°C" },
      META_VALUE, 133, -1, 0,
      META_VT_F32, 5.0f, 20.0f, 0.5f, META_SCOPE_PER_UNIT },
    // [136] servo
    { 136, { "СЕРВО", "SERVO" }, { nullptr, nullptr },
      META_SUBMENU, 100, 137, 5,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [137] servo_closed_angle
    { 137, { "ЗАКРЫТО УГОЛ", "CLOSED ANGLE" }, { "°", "°" },
      META_VALUE, 136, -1, 0,
      META_VT_U8, 0.0f, 180.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [138] servo_open_angle
    { 138, { "ОТКРЫТО УГОЛ", "OPEN ANGLE" }, { "°", "°" },
      META_VALUE, 136, -1, 0,
      META_VT_U8, 0.0f, 180.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [139] servo_time_closed
    { 139, { "ВРЕМЯ ЗАКРЫТО", "CLOSED TIME" }, { "с", "s" },
      META_VALUE, 136, -1, 0,
      META_VT_U16, 0.0f, 3600.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [140] servo_time_open
    { 140, { "ВРЕМЯ ОТКРЫТО", "OPEN TIME" }, { "с", "s" },
      META_VALUE, 136, -1, 0,
      META_VT_U16, 0.0f, 600.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [141] servo_smart_mode
    { 141, { "УМНЫЙ РЕЖИМ", "SMART MODE" }, { nullptr, nullptr },
      META_TOGGLE, 136, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_PER_UNIT },
    // [142] global_settings
    { 142, { "ОБЩИЕ", "GLOBAL" }, { nullptr, nullptr },
      META_SUBMENU, 0, 143, 3,
      META_VT_F32, 0.0f, 0.0f, 0.0f, META_SCOPE_PER_UNIT },
    // [143] units_count
    { 143, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
      META_VALUE, 142, -1, 0,
      META_VT_U8, 1.0f, 3.0f, 1.0f, META_SCOPE_GLOBAL },
    // [144] cmd_ignore_external
    { 144, { "Игнор команд", "Ignor ext cmd" }, { nullptr, nullptr },
      META_TOGGLE, 142, -1, 0,
      META_VT_BOOL, 0.0f, 0.0f, 1.0f, META_SCOPE_GLOBAL },
    // [145] language
    { 145, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
      META_VALUE, 142, -1, 0,
      META_VT_U8, 0.0f, 1.0f, 1.0f, META_SCOPE_GLOBAL },
};

// Get menu item by id
static inline const MenuMeta* menu_meta_get(uint16_t id) {
    if (id < MENU_META_COUNT) return &g_menu_meta[id];
    return nullptr;
}
