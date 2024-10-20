// LCD zobrazuje hodnoty z premennych
// Jeden senzor meria teplotu
#include <TFT_eSPI.h>    // TFT display library
#include <lvgl.h>        // LVGL library
#include "../.pio/libdeps/esp32doit-devkit-v1/lvgl/examples/lv_examples.h"
#include "temperature.h"
#include "output.h"

TFT_eSPI tft = TFT_eSPI();  // Create TFT object

#define DRAW_BUF_SIZE (320 * 240 / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

float f_RoomTemperature = 0.5;
float f_FloorTemperature = 0.5;
float f_RoomTempTarget = 25.0;
float f_FloorTempTarget = 25.0;

OutputState CurrentOutState;
uint16_t u16_Time = 0;

// Tick handler for LVGL
void lv_tick_handler(void) {
  lv_tick_inc(5);
}

void setup()
{
  // start serial port
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,4);
  tft.setTextColor(TFT_WHITE);
  tft.println ("B");  

  pinMode(17, OUTPUT);    // sets the digital pin 13 as output  

  temp_Init();
  //tft.println ("1");
}

void loop_100ms(void)
{
  
}

void loop_1s(void)
{
    f_RoomTemperature = temp_GetTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetTemperature(TEMP_SENSOR_FLOOR);
    
    if((f_RoomTemperature == TEMP_SENSOR_NOT_CONNECTED) || (f_FloorTemperature == TEMP_SENSOR_NOT_CONNECTED))
    {
        out_Set(OFF_LOCKED);
    }else
    {
      if((f_RoomTemperature < f_RoomTempTarget) && (f_FloorTemperature < f_FloorTempTarget))
      {
        out_Set(ON);
      }else
      {
        out_Set(OFF);
      }
    }

    if (CurrentOutState == OFF)
    {
      tft.fillScreen(TFT_BLACK);
    }else if(CurrentOutState == ON)
    {
      tft.fillScreen(TFT_GREEN);
    }else if(CurrentOutState == OFF_LOCKED)
    {
      tft.fillScreen(TFT_RED);
    }else
    {
      tft.fillScreen(TFT_BLUE);
    }
    tft.setCursor(0,0,4);
    tft.setTextColor(TFT_WHITE);
    tft.print ("Podlaha="); tft.setCursor(120,0,4); tft.print(f_RoomTemperature); tft.print (" C");
    tft.setCursor(0,30,4);
    tft.print ("Izba="); tft.setCursor(120,30,4); tft.print(f_FloorTemperature); tft.print (" C");
    tft.setCursor(0,60,4);
    tft.print ("Ciel="); tft.setCursor(120,60,4); tft.print(f_RoomTempTarget); tft.print (" C");
    tft.setCursor(0,90,4);
    tft.print ("Stav="); tft.setCursor(120,90,4);
    if (CurrentOutState==OFF)
    {    
      tft.print("Vypnute");
    }else if(CurrentOutState==ON)
    {    
      tft.print("Zapnute");
    }else if(CurrentOutState==OFF_LOCKED)
    {
      tft.print("Chyba");
    }else
    {
      tft.print("Zapinam");
      if(CurrentOutState == SET1ON) tft.print("SET1ON");
      if(CurrentOutState == ON1_WAIT) tft.print("ON1_WAIT");
      if(CurrentOutState == SET2ON) tft.print("SET2ON");
      if(CurrentOutState == ON2_WAIT) tft.print("ON2_WAIT");
    }
//    delay(1000);            // Give the processor some time
//    digitalWrite(17, HIGH); // sets the digital pin 13 on
//    delay(1000);            // waits for a second
//    digitalWrite(17, LOW);  // sets the digital pin 13 off  
}

void loop_8s(void)
{
  CurrentOutState = out_Tick();
}

void loop()
{
  u16_Time++;
  //u16_Time = u16_Time & ((2^13)-1);
  u16_Time = u16_Time & 8191;

  if((u16_Time % 100) == 0)    // 100ms
  {
    loop_100ms();
  }

  if((u16_Time % 1000) == 0)   // 1s
  {
    loop_1s();
    //Serial.println("11");
  }

  if((u16_Time % 8000) == 0)   // 8s
  {
    //Serial.println("88");
    loop_8s();
    u16_Time = 0;
  }

  delay(1);
}