#include <TFT_eSPI.h>    // TFT display library
#include <lvgl.h>        // LVGL library
//#include "../.pio/libdeps/esp32doit-devkit-v1/lvgl/examples/lv_examples.h"
#include "temperature.h"
#include "output.h"
#include "touch.h"

TFT_eSPI tft = TFT_eSPI();  // Create TFT object

#define DRAW_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10) // Pixels, not bytes

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[DRAW_BUF_SIZE]; // Buffer for 1/10th of screen

float f_RoomTemperature = 0.5;
float f_FloorTemperature = 0.5;
float f_RoomTempTarget = 19.0;
float f_FloorTempTarget = 25.0;
float f_TempHysteresis = 0.5;

Touch touch; // Create touch instance

SequenceState CurrentSequenceState = IDLE;
bool CurrentOutState = false;
uint16_t u16_Time = 0;

// Tick handler for LVGL
void lv_tick_handler(void) {
  lv_tick_inc(5);
}

// Display flush callback
void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void setup()
{
     Serial.begin(115200);

    // Initialize display
    tft.init();
    tft.setRotation(1); // 90° clockwise (240x320)
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 4);
    tft.setTextColor(TFT_WHITE);
    tft.println("v0.3");

    // Initialize LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, DRAW_BUF_SIZE);

    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv); // Corrected from lv_indev_drv_init
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize touch controller
    if (!touch.begin()) {
        Serial.println("Failed to initialize CST820!");
        while (1); // Halt on failure
    }
    Serial.println("CST820 initialized successfully!");

    // Initialize touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = Touch::touchpad_read;
    lv_indev_drv_register(&indev_drv);

        // Example: Create a button to test touch
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, 100, 100);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Test Button");
    lv_obj_center(label);

  uint8_t temperature_sensor_count = temp_Init();
  tft.println ("Found");
  tft.println (temperature_sensor_count);
  tft.println (" temperature sensors");
     // Initialize temperature sensors
    temperature_sensor_count = temp_Init();
    tft.setCursor(0, 30, 4);
    tft.println("Found");
    tft.println(temperature_sensor_count);
    tft.println(" temperature sensors");
}

void loop_100ms(void)
{
  
}

void loop_1s(void)
{
  Serial.print("ACsense: ");
  bool ACpresent = measureOutput();
  if(ACpresent)
  {
//    Serial.print("1\n");
  }else
  {
//    Serial.print("0\n");
  }

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
      tft.fillScreen(TFT_RED);
    }else if(CurrentSequenceState != IDLE)
    {
      tft.fillScreen(TFT_BLUE);
    }else if (CurrentOutState == false)
    {
      tft.fillScreen(TFT_BLACK);
    }else if(CurrentOutState == true)
    {
      tft.fillScreen(TFT_GREEN);
    }
    
    tft.setCursor(0,0,4);
    tft.setTextColor(TFT_WHITE);    
    tft.print ("Izba="); tft.setCursor(120,0,4); tft.print(f_RoomTemperature); tft.print (" C / ");  tft.print(f_RoomTempTarget); tft.print (" C");
    
    tft.setCursor(0,30,4);
    tft.print ("Podlaha="); tft.setCursor(120,30,4); tft.print(f_FloorTemperature); tft.print (" C / "); tft.print(f_FloorTempTarget); tft.print (" C");
    
    tft.setCursor(0,60,4);
    tft.print ("Stav="); tft.setCursor(120,60,4);
    if(CurrentSequenceState==DEAD)
    {
      tft.print("Chyba");
    }else if(CurrentSequenceState==IDLE)
    {
      if (CurrentOutState==false)
      {    
        tft.print("Vypnute");
      }else if(CurrentOutState==true)
      {    
        tft.print("Zapnute");
      }
    }else    
    {
      tft.print("Prepinam");
    }

    tft.setCursor(0,90,4);
    tft.print ("Sekvencia="); tft.setCursor(120,90,4);
    if(CurrentSequenceState == IDLE) tft.print("IDLE");
    if(CurrentSequenceState == TURNING_ON_SAFETY) tft.print("TURNING_ON_SAFETY");
    if(CurrentSequenceState == TURNING_ON_WORK) tft.print("TURNING_ON_WORK");
    if(CurrentSequenceState == VERIFY_ON) tft.print("VERIFY_ON");
    if(CurrentSequenceState == TURNING_OFF_WORK) tft.print("TURNING_OFF_WORK");
    if(CurrentSequenceState == TURNING_OFF_SAFETY) tft.print("TURNING_OFF_SAFETY");
    if(CurrentSequenceState == VERIFY_OFF) tft.print("VERIFY_OFF");
    if(CurrentSequenceState == DEAD) tft.print("DEAD");    

//    delay(1000);            // Give the processor some time
//    digitalWrite(17, HIGH); // sets the digital pin 13 on
//    delay(1000);            // waits for a second
//    digitalWrite(17, LOW);  // sets the digital pin 13 off  
}

void loop_8s(void)
{
  CurrentSequenceState = out_ControlRelays();  
}

void loop()
{
  u16_Time++;
  //u16_Time = u16_Time & ((2^13)-1);
  u16_Time = u16_Time & 8191;

  lv_task_handler(); // Handle LVGL tasks

  if((u16_Time % 8000) == 0)   // 8s
  {
    //Serial.println("88");
    loop_8s();
    u16_Time = 0;
  }else

  if((u16_Time % 1000) == 0)   // 1s
  {
    loop_1s();
    //Serial.println("11");
    /*digitalWrite(OUT_1, LOW);            
    digitalWrite(OUT_2, LOW);
    delay(1000);
    digitalWrite(OUT_1, HIGH);            
    digitalWrite(OUT_2, HIGH);*/
  }else

  if((u16_Time % 100) == 0)    // 100ms
  {
    loop_100ms();
  }
  delay(1);
}