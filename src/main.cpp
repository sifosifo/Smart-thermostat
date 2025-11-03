// src/main.cpp
#include <Arduino.h>
#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include <lvgl.h>
#include "gui/gui.h"
#include "gui/settings.h"
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

/* ------------------------------------------------------------------------ */
void setup()
{
    Serial.begin(115200);
    Serial.println("Init LVGL");
    lv_init();
    Serial.println("Init GUI");
    gui_init();
    Serial.println("Init IO");
    out_Init();
    Serial.println("Init temperature sensors");
    temp_Init();
    Serial.println("Setup done");
}

/* ------------------------------------------------------------------------ */
void loop_100ms()
{
    AdjustLCDBrightness();
}

void loop_1s()
{
    if(gui_check_if_enabled())
    {

        /* simulate sensor */
        f_RoomTemperature += 0.1f;
        if (f_RoomTemperature > 25.0f) f_RoomTemperature = 18.0f;

        CurrentOutState = out_Get();
        f_RoomTemperature = temp_GetTemperature(TEMP_SENSOR_ROOM);
        f_FloorTemperature = temp_GetTemperature(TEMP_SENSOR_FLOOR);

        if((f_RoomTemperature == TEMP_SENSOR_NOT_CONNECTED) || (f_FloorTemperature == TEMP_SENSOR_NOT_CONNECTED))
        {
            out_EnterDeadState();
        }else
        {
        if((f_RoomTemperature < f_RoomTempTarget) && (f_FloorTemperature < f_FloorTempTarget))
        {
            out_TurnOnHeatingElement();
        }else if((f_RoomTemperature > (f_RoomTempTarget + f_TempHysteresis)) || (f_FloorTemperature > f_FloorTempTarget))
        {
            out_TurnOffHeatingElement();
        }
        }

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
        
    //   tft.setCursor(0,0,4);
    //   tft.setTextColor(TFT_WHITE);    
    //   tft.print ("Izba="); tft.setCursor(120,0,4); tft.print(f_RoomTemperature); tft.print (" C / ");  tft.print(f_RoomTempTarget); tft.print (" C");
        
    //   tft.setCursor(0,30,4);
    //   tft.print ("Podlaha="); tft.setCursor(120,30,4); tft.print(f_FloorTemperature); tft.print (" C / "); tft.print(f_FloorTempTarget); tft.print (" C");
        
    //   tft.setCursor(0,60,4);
    //   tft.print ("Stav="); tft.setCursor(120,60,4);
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

    //   tft.setCursor(0,90,4);
    //   tft.print ("Sekvencia="); tft.setCursor(120,90,4);
  
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
}

void loop_8s()
{
    CurrentSequenceState = out_ControlRelays(); 
}

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