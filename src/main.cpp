#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include "touch.h"

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

uint16_t u16_Time = 0;

TFT_eSPI tft = TFT_eSPI();

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    //tft.startWrite();
    tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)color_p);
    //tft.endWrite();

    lv_disp_flush_ready(disp);
}

void setup()
{
    Serial.begin(115200);

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.println("I am LVGL_Arduino");

    lv_init();
    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    tft.begin();
    tft.setRotation(0);
    tft.initDMA();

    digitalWrite(27, HIGH);
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_BLACK);
    delay(500);

    buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 200 , MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);//screenWidth * screenHeight/2
    buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 200 , MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 200);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    touch_init();

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("Setup done");
    tft.startWrite();
}

void loop_100ms() {
   
}

void loop_1s() {
    // Add 1s tasks if needed
}

void loop_8s() {
    // Add 8s tasks if needed
}

void loop() {
    u16_Time++;
    u16_Time = u16_Time & 8191;

    lv_tick_inc(10);
    lv_timer_handler();
    //Serial.println("lv_timer_handler");

    if ((u16_Time % 800) == 0) { // 8s
        loop_8s();
        u16_Time = 0;
    } else if ((u16_Time % 100) == 0) { // 1s
        loop_1s();
    } else if ((u16_Time % 10) == 0) { // 100ms
        loop_100ms();
    }
    delay(10);
}