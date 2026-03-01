// Auto-generated. Do not edit.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "menu_ids.h"

#ifndef NUM_UNITS
#define NUM_UNITS 3
#endif

class MenuState {
public:
  uint8_t number_controller = (uint8_t)0;
  float dry_temp[NUM_UNITS] = { 60.0f };
  uint16_t dry_time[NUM_UNITS] = { (uint16_t)240 };
  uint8_t storage_temp[NUM_UNITS] = { (uint8_t)45 };
  uint8_t storage_hum[NUM_UNITS] = { (uint8_t)12 };
  bool storage_hum_priority[NUM_UNITS] = { true };
  uint8_t start_stage_by_number[NUM_UNITS] = { (uint8_t)1 };
  uint8_t profile_stage_01_temp[NUM_UNITS] = { (uint8_t)50 };
  uint16_t profile_stage_01_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_01_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_02_temp[NUM_UNITS] = { (uint8_t)60 };
  uint16_t profile_stage_02_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_02_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_03_temp[NUM_UNITS] = { (uint8_t)70 };
  uint16_t profile_stage_03_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_03_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_04_temp[NUM_UNITS] = { (uint8_t)60 };
  uint16_t profile_stage_04_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_04_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_05_temp[NUM_UNITS] = { (uint8_t)50 };
  uint16_t profile_stage_05_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_05_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_06_temp[NUM_UNITS] = { (uint8_t)70 };
  uint16_t profile_stage_06_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_06_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_07_temp[NUM_UNITS] = { (uint8_t)90 };
  uint16_t profile_stage_07_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_07_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_08_temp[NUM_UNITS] = { (uint8_t)75 };
  uint16_t profile_stage_08_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_08_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_09_temp[NUM_UNITS] = { (uint8_t)65 };
  uint16_t profile_stage_09_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_09_time[NUM_UNITS] = { (uint16_t)1 };
  uint8_t profile_stage_10_temp[NUM_UNITS] = { (uint8_t)55 };
  uint16_t profile_stage_10_ramps_time[NUM_UNITS] = { (uint16_t)5 };
  uint16_t profile_stage_10_time[NUM_UNITS] = { (uint16_t)1 };
  float preset_pla_temp = 45.0f;
  uint16_t preset_pla_time = (uint16_t)240;
  float preset_pla_cf_temp = 45.0f;
  uint16_t preset_pla_cf_time = (uint16_t)240;
  float preset_pla_gf_temp = 45.0f;
  uint16_t preset_pla_gf_time = (uint16_t)240;
  float preset_petg_temp = 60.0f;
  uint16_t preset_petg_time = (uint16_t)180;
  float preset_petg_cf_temp = 65.0f;
  uint16_t preset_petg_cf_time = (uint16_t)180;
  float preset_petg_gf_temp = 60.0f;
  uint16_t preset_petg_gf_time = (uint16_t)180;
  float preset_abs_temp = 80.0f;
  uint16_t preset_abs_time = (uint16_t)240;
  float preset_abs_cf_temp = 90.0f;
  uint16_t preset_abs_cf_time = (uint16_t)240;
  float preset_abs_gf_temp = 90.0f;
  uint16_t preset_abs_gf_time = (uint16_t)240;
  float preset_pa_temp = 90.0f;
  uint16_t preset_pa_time = (uint16_t)240;
  float preset_pa_cf_temp = 100.0f;
  uint16_t preset_pa_cf_time = (uint16_t)240;
  float preset_pa_gf_temp = 100.0f;
  uint16_t preset_pa_gf_time = (uint16_t)240;
  float preset_pc_temp = 100.0f;
  uint16_t preset_pc_time = (uint16_t)360;
  float preset_pc_cf_temp = 100.0f;
  uint16_t preset_pc_cf_time = (uint16_t)360;
  float preset_my1_temp = 70.0f;
  uint16_t preset_my1_time = (uint16_t)360;
  float preset_my2_temp = 80.0f;
  uint16_t preset_my2_time = (uint16_t)360;
  float preset_my3_temp = 90.0f;
  uint16_t preset_my3_time = (uint16_t)360;
  uint8_t scales_count = (uint8_t)0;
  float tare_spool1 = 0.0f;
  float tare_spool2 = 0.0f;
  float tare_spool3 = 0.0f;
  float tare_spool4 = 0.0f;
  bool storage_auto[NUM_UNITS] = { true };
  bool storage_auto_dry[NUM_UNITS] = { true };
  uint8_t storage_rh_hyst[NUM_UNITS] = { (uint8_t)5 };
  bool storage_dry_temp_mode[NUM_UNITS] = { false };
  uint8_t storage_dry_temp_by_dry_temp[NUM_UNITS] = { (uint8_t)90 };
  uint16_t storage_min_hold_sec[NUM_UNITS] = { (uint16_t)30 };
  float pid_kp_heater[NUM_UNITS] = { 9.81f };
  float pid_ki_heater[NUM_UNITS] = { 0.084f };
  float pid_kd_heater[NUM_UNITS] = { 87.825f };
  float pid_gain_heater[NUM_UNITS] = { 9.81f };
  float pid_kp_chamber[NUM_UNITS] = { 5.873f };
  float pid_ki_chamber[NUM_UNITS] = { 0.18f };
  float pid_kd_chamber[NUM_UNITS] = { 50.402f };
  float pid_gain_chamber[NUM_UNITS] = { 5.873f };
  float heater_max_temp[NUM_UNITS] = { 130.0f };
  float air_max_temp[NUM_UNITS] = { 90.0f };
  uint8_t delta_c[NUM_UNITS] = { (uint8_t)35 };
  bool heater_delta_is_percent[NUM_UNITS] = { false };
  float vh_max_err[NUM_UNITS] = { 150.0f };
  float vh_heating_gain[NUM_UNITS] = { 1.0f };
  uint16_t vh_check_gain_time_s[NUM_UNITS] = { (uint16_t)20 };
  uint8_t vh_arm_pwm_min_pct[NUM_UNITS] = { (uint8_t)30 };
  float fan_temp_on_c[NUM_UNITS] = { 55.0f };
  float fan_hyst_c[NUM_UNITS] = { 5.0f };
  uint8_t servo_closed_angle[NUM_UNITS] = { (uint8_t)20 };
  uint8_t servo_open_angle[NUM_UNITS] = { (uint8_t)50 };
  uint16_t servo_time_closed[NUM_UNITS] = { (uint16_t)600 };
  uint16_t servo_time_open[NUM_UNITS] = { (uint16_t)30 };
  bool servo_smart_mode[NUM_UNITS] = { false };
  bool cmd_ignore_external = false;
  uint16_t drying_session_count = (uint16_t)0;
  uint16_t storage_session_count = (uint16_t)0;
  uint16_t profile_session_count = (uint16_t)0;
  uint8_t port1_mode = (uint8_t)0;
  uint8_t port2_mode = (uint8_t)0;
  uint8_t port3_mode = (uint8_t)1;
  uint8_t units_count = (uint8_t)1;
  uint8_t language = (uint8_t)1;

  void initDefaults();   // выставить дефолты (из YAML)
  void loadFromEEPROM(); // подхватить, если MAGIC/VER совпали
  void saveToEEPROM();   // записать только изменённые поля
};

extern MenuState menu;