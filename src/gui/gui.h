// src/gui/gui.h
#pragma once
#include <lvgl.h>
#include <Preferences.h>

extern lv_obj_t *meter;
extern lv_meter_indicator_t *indic;
extern lv_meter_indicator_t *indic2;
extern lv_meter_indicator_t *hysteresis;
extern lv_obj_t *temperature_big;
extern lv_obj_t *temperature_target;
extern lv_obj_t *state;
extern Preferences prefs;
extern lv_obj_t *scr_main;
extern lv_obj_t *Tcount_l;

/* ----- GUI lifecycle ----------------------------------------------------- */
void gui_init();                     // call once from setup()
void gui_update_state(char *buf);
void gui_update_temperature(float current, float target, float hyst, float TemperatureFloor, float TemperatureFloorTarget);
bool gui_check_if_enabled(void);
void gui_UpdateSensorsCount(uint8_t count, bool two_sensors_required);

/* ----- Callbacks used by the thermostat logic --------------------------- */
void temp_increase_target();          // called from main when "+" pressed
void temp_decrease_target();          // called from main when "-" pressed