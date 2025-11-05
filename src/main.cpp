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

float f_RoomTemperature = 0.0;
float f_FloorTemperature = 0.0;
float f_RoomTempTarget = 19.0;
float f_FloorTempTarget = 25.0;
float f_TempHysteresis = 2.0;

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
    uint8_t sensor_count = 0;
    uint8_t sensor_count_expected = 0;
    bool two_sensors_required = false;

    CurrentOutState = out_Get();
    
    sensor_count = temp_Init();
    
    sensor_count_expected = two_sensors_required ? 2 : 1;
    
    f_RoomTemperature = temp_GetTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetTemperature(TEMP_SENSOR_FLOOR);

    two_sensors_required = settings_require_two_sensors();

    if(gui_check_if_enabled())
    {
        handleHeatingLogic(two_sensors_required);

        if(CurrentSequenceState == DEAD)
        {
    //      tft.fillScreen(TFT_RED);
        }else if(CurrentSequenceState != IDLE)
        {
    //      tft.fillScreen(TFT_BLUE);
        }else if (CurrentOutState == false)
        {
    //      tft.fillScreen(TFT_BLACK);
        }else if(CurrentOutState == true)
        {
    //      tft.fillScreen(TFT_GREEN);
        }
        
    
        if(CurrentSequenceState==DEAD)
        {
    //     tft.print("Chyba");
        }else if(CurrentSequenceState==IDLE)
        {
        if (CurrentOutState==false)
        {    
    //       tft.print("Vypnute");
        }else if(CurrentOutState==true)
        {    
    //       tft.print("Zapnute");
        }
        }else    
        {
    //     tft.print("Prepinam");
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
    
    gui_update_state(buf);
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, f_FloorTemperature, f_FloorTempTarget);

    f_TempHysteresis = settings_hysteresis();

    gui_UpdateIndicators(out_get_saf_relay(), out_get_work_relay(), measureOutput());

    temp_swap_sensors(settings_swap_sensors());

    gui_UpdateSensorsCount(sensor_count, two_sensors_required);
}

void loop_4s()
{
    CurrentSequenceState = out_ControlRelays(); 
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

void handleHeatingLogic(bool two_sensors_required)
{
    bool sensors_ok = false;

    if (two_sensors_required) {
        sensors_ok = (f_RoomTemperature != TEMP_SENSOR_NOT_CONNECTED) &&
                     (f_FloorTemperature != TEMP_SENSOR_NOT_CONNECTED);
    } else {
        sensors_ok = (f_RoomTemperature != TEMP_SENSOR_NOT_CONNECTED);
    }

    if (!sensors_ok) {
        out_EnterDeadState();
        return;
    }

    if (two_sensors_required) {
        if ((f_RoomTemperature < f_RoomTempTarget) && (f_FloorTemperature < f_FloorTempTarget))
            out_TurnOnHeatingElement();
        else if ((f_RoomTemperature > (f_RoomTempTarget + f_TempHysteresis)) ||
                 (f_FloorTemperature > f_FloorTempTarget))
            out_TurnOffHeatingElement();
    } else {
        if (f_RoomTemperature < f_RoomTempTarget)
            out_TurnOnHeatingElement();
        else if (f_RoomTemperature > (f_RoomTempTarget + f_TempHysteresis))
            out_TurnOffHeatingElement();
    }
}