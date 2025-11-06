// src/main.cpp
#include <Arduino.h>
#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include <lvgl.h>
#include "gui/gui.h"
#include "gui/settings.h"
#include "gui/meter.h"
#include "output.h"
#include "temperature.h"

SequenceState CurrentSequenceState = IDLE;
bool CurrentOutState = false;
uint16_t u16_Time = 0;

void handleHeatingLogic(bool two_sensors_required);

/* ------------------------------------------------------------------------ */
void setup()
{
    out_Init();     // As soon as possible to minimize time when relay driving outputs are not driven
    Serial.begin(115200);
    Serial.println("Init LVGL");
    lv_init();
    Serial.println("Init GUI");
    gui_init();
    Serial.println("Init temperature sensors");
    temp_Init();
    Serial.println("Setup done");
}

/* ------------------------------------------------------------------------ */
void loop_100ms()
{
    AdjustLCDBrightness();
}

void loop_2s()
{
    static uint8_t sensor_count = 0;
    uint8_t sensor_count_expected = 0;
    bool two_sensors_required = false;
    float f_RoomTemperature;
    float f_FloorTemperature;
    float f_RoomTempTarget;
    float f_FloorTempTarget;
    float f_TempHysteresis; 

    two_sensors_required = settings_require_two_sensors();
    sensor_count_expected = two_sensors_required ? 2 : 1;
    f_TempHysteresis = settings_hysteresis();
    f_RoomTempTarget = temp_GetTemperatureTarget(TEMP_SENSOR_ROOM);
    f_FloorTempTarget = temp_GetTemperatureTarget(TEMP_SENSOR_FLOOR);
    f_RoomTemperature = temp_GetTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetTemperature(TEMP_SENSOR_FLOOR);

    if(gui_check_if_enabled())
    {
        sensor_count = temp_Process(two_sensors_required);
        CurrentOutState = out_Get();
        
        if(CurrentSequenceState == DEAD)
        {
          // red
        }else
        {
            //grey or normal
        }                
    
        if(CurrentSequenceState==DEAD)
        {
            Serial.printf("Chyba\n");
        }else if(CurrentSequenceState==IDLE)
        {
        if (CurrentOutState==false)
        {
            Serial.printf("Vypnuté\n");
        }else if(CurrentOutState==true)
        {   
            Serial.printf("Zapnuté\n");
        }
        }else    
        {
            Serial.printf("Prepínam\n");
        }  
    }else
    {
        out_TurnOffHeatingElement();
        CurrentSequenceState = IDLE;
    }

    char buf[16];
    if(CurrentSequenceState == IDLE) lv_snprintf(buf, sizeof(buf), "IDLE");
    if(CurrentSequenceState == TURNING_ON_SAFETY) lv_snprintf(buf, sizeof(buf), "TURNING_ON_SAFETY");
    if(CurrentSequenceState == TURNING_ON_WORK) lv_snprintf(buf, sizeof(buf), "TURNING_ON_WORK");
    if(CurrentSequenceState == VERIFY_ON) lv_snprintf(buf, sizeof(buf), "VERIFY_ON");
    if(CurrentSequenceState == TURNING_OFF_WORK) lv_snprintf(buf, sizeof(buf), "TURNING_OFF_WORK");
    if(CurrentSequenceState == TURNING_OFF_SAFETY) lv_snprintf(buf, sizeof(buf), "TURNING_OFF_SAFETY");
    if(CurrentSequenceState == VERIFY_OFF) lv_snprintf(buf, sizeof(buf), "VERIFY_OFF");
    if(CurrentSequenceState == DEAD) lv_snprintf(buf, sizeof(buf), "DEAD");

    // Update label with state machine
    gui_update_state(buf);
    // Update meter by temperatures
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, f_FloorTemperature, f_FloorTempTarget);
    // Update relay indicators
    gui_UpdateIndicators(out_get_saf_relay(), out_get_work_relay(), measureOutput());    
    // If sensors need to be swapped, do so.
    temp_swap_sensors(settings_swap_sensors());
    // Update sensor count label
    gui_UpdateSensorsCount(sensor_count, two_sensors_required);
    // Update state machine
    CurrentSequenceState = out_ControlRelays();
}

void loop_4s()
{
     
}

/* ------------------------------------------------------------------------ */
void loop()
{
    u16_Time = (u16_Time + 1) & 511;

    lv_tick_inc(10);
    lv_timer_handler();

    if (u16_Time % 400 == 0) {          // 4 s
        loop_4s();
        u16_Time = 0;
    } else if (u16_Time % 200 == 0) {   // 2 s
        loop_2s();
    } else if (u16_Time % 10 == 0) {    // 100 ms
        loop_100ms();
    }

    delay(10);
}