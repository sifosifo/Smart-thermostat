// LCD zobrazuje hodnoty z premennych
// Jeden senzor meria teplotu
#include <TFT_eSPI.h>    // TFT display library
#include <lvgl.h>        // LVGL library
#include "../.pio/libdeps/esp32doit-devkit-v1/lvgl/examples/lv_examples.h"
#include "temperature.h"

TFT_eSPI tft = TFT_eSPI();  // Create TFT object

#define DRAW_BUF_SIZE (320 * 240 / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

float f_RoomTemperature = 0.5;
float f_FloorTemperature = 0.5;
float f_RoomTempTarget = 24.0;
float f_FloorTempTarget = 25.0;

uint8_t t8State = 0;

// Tick handler for LVGL
void lv_tick_handler(void) {
  lv_tick_inc(5);
}

void setup()
{
  // start serial port
  Serial.begin(115200);
  Serial.println("Dallas Temperature IC Control Library Demo");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0,4);
  tft.setTextColor(TFT_WHITE);
  tft.println ("BOOT");  

  pinMode(17, OUTPUT);    // sets the digital pin 13 as output  

  temp_Init();
}

void loop()
{
  f_RoomTemperature = temp_GetTemperature(ONE_WIRE_BUS1);

  if((f_RoomTemperature < f_RoomTempTarget) && (f_FloorTemperature < f_FloorTempTarget))
  {
    t8State = 1;
  }else
  {
    t8State = 0;
  }

  if (t8State==0)
  {
    tft.fillScreen(TFT_BLACK);
  }else
  {
    tft.fillScreen(TFT_GREEN);
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
  if (t8State==0)
  {    
    tft.print("Vypnute");
  }else
  {    
    tft.print("Zapnute");
  }
  delay(1000);            // Give the processor some time
  digitalWrite(17, HIGH); // sets the digital pin 13 on
  delay(1000);            // waits for a second
  digitalWrite(17, LOW);  // sets the digital pin 13 off
  delay(1000);            // waits for a second
}