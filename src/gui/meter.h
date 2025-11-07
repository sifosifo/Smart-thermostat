// src/gui/meter.h
#pragma once
#include <lvgl.h>

lv_obj_t* create_temperature_meter(
    lv_obj_t *parent,
    int16_t x, int16_t y,
    uint16_t radius,
    float min_temp, float max_temp,
    float min_angle, float max_angle
);

void gui_UpdateIndicators(bool saf_relay, bool work_relay, bool ac_sense);