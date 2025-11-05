#pragma once
#include <lvgl.h>
#include <Preferences.h>

extern bool thermostat_enabled;          // from gui.cpp
extern Preferences prefs;                // already declared in gui.cpp

/* ----- Public API ------------------------------------------------------- */
void settings_screen_create(lv_obj_t *parent);   // call from gui_init()
void settings_screen_save_and_exit();           // called from Back button
lv_obj_t *settings_get_screen(void);

float   settings_hysteresis(void);
uint8_t settings_max_brightness(void);
bool settings_auto_brightness(void);
bool settings_require_ac_sense(void);
bool settings_swap_sensors(void);
bool settings_require_two_sensors(void);