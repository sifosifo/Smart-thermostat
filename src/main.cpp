// src/main.cpp
#include <Arduino.h>
#include <lvgl.h>
#include "gui/gui.h"
#include "output.h"

/* ----- Thermostat state (kept here) -------------------------------------- */
float TemperatureRoom        = 18.0f;
float TemperatureRoomTarget  = 19.0f;
const float TemperatureHysteresis = 2.0f;
float TemperatureFloor = 20.0;

uint16_t u16_Time = 0;

/* ------------------------------------------------------------------------ */
void setup()
{
    Serial.begin(115200);
    lv_init();

    gui_init();

    out_Init();

    Serial.println("Setup done");
}

/* ------------------------------------------------------------------------ */
void loop_100ms()
{
    AdjustLCDBrightness();
}

void loop_1s()
{
    /* simulate sensor */
    TemperatureRoom += 0.1f;
    if (TemperatureRoom > 25.0f) TemperatureRoom = 18.0f;

    gui_update_temperature(TemperatureRoom, TemperatureRoomTarget, TemperatureHysteresis, TemperatureFloor);
    
}

void loop_8s() { /* long-term tasks */ }

/* ------------------------------------------------------------------------ */
void loop()
{
    u16_Time = (u16_Time + 1) & 8191;

    lv_tick_inc(10);
    lv_timer_handler();

    if (u16_Time % 800 == 0) {          // 8 s
        loop_8s();
        u16_Time = 0;
    } else if (u16_Time % 100 == 0) {   // 1 s
        loop_1s();
    } else if (u16_Time % 10 == 0) {    // 100 ms
        loop_100ms();
    }

    delay(10);
}