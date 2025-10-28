#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include "touch.h"
#include "temperature.h"

static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

uint16_t u16_Time = 0;
float TemperatureRoom = 18.0;
float TemperatureRoomTarget = 19.0;
float TemperatureHysteresis = 2;
lv_obj_t *meter;
lv_meter_indicator_t *indic;
lv_meter_indicator_t *hysteresis;
lv_obj_t * temperature_big;

TFT_eSPI tft = TFT_eSPI();

void set_float_with_comma(lv_obj_t * label, float value, int decimals)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    for (char *p = buf; *p; p++) if (*p == '.') *p = ',';
    lv_label_set_text(label, buf);
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    //tft.startWrite();
    //tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)color_p);
    tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    //tft.endWrite();

    lv_disp_flush_ready(disp);
}

void setup()
{
    Serial.begin(115200);

    //temp_Init();

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.println("I am LVGL_Arduino");

    lv_init();
    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    tft.begin();
    tft.setRotation(1);
    tft.initDMA();

    digitalWrite(27, HIGH);

    buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 100 , MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);//screenWidth * screenHeight/2
    buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 100 , MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 100);

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

    // Create a base screen (LVGL uses screens like pages)
    lv_obj_t *scr = lv_scr_act();

    // Create a label
//    lv_obj_t *label = lv_label_create(scr);
//    lv_label_set_text(label, "Hello CYD!");
//    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    // Create a button
    lv_obj_t *btn_plus = lv_btn_create(scr);
    lv_obj_align(btn_plus, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(btn_plus, 200, 60);

    lv_obj_t *btn_plus_label = lv_label_create(btn_plus);
    lv_label_set_text(btn_plus_label, "+");

    lv_obj_t *btn_minus = lv_btn_create(scr);
    lv_obj_align(btn_minus, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(btn_minus, 200, 60);

    lv_obj_t *btn_minus_label = lv_label_create(btn_minus);
    lv_label_set_text(btn_minus_label, "-");

    // Create a gauge (meter)
    meter = lv_meter_create(scr);
    lv_obj_align(meter, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(meter, 240, 240);

    // Add scale
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 5, lv_color_black());
    lv_meter_set_scale_major_ticks(meter, scale, 10, 4, 20, lv_color_black(), -100); // -100 means, that numbers are out of screen
    lv_meter_set_scale_range(meter, scale, 180, 220, 270, 135);

    // Add scale just for numbers
    lv_meter_scale_t *scale_ = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale_, 41, 0, 0, lv_color_black()); // 0, 0 - invisible
    lv_meter_set_scale_major_ticks(meter, scale_, 10, 0, 0, lv_color_black(), 30); // invisible ticks, but numbers visible
    lv_meter_set_scale_range(meter, scale_, 18, 22, 270, 135);

    // Add outline
    lv_meter_indicator_t * outline;
    outline = lv_meter_add_arc(meter, scale, 2, lv_color_black(), 2);
    lv_meter_set_indicator_start_value(meter, outline, 18);
    lv_meter_set_indicator_end_value(meter, outline, 22);

    // Add hysteresis
    hysteresis = lv_meter_add_arc(meter, scale, 8, lv_color_make(0, 255, 0), 10);
    lv_meter_set_indicator_start_value(meter, hysteresis, (TemperatureRoomTarget - (TemperatureHysteresis / 2)) * 10);
    lv_meter_set_indicator_end_value(meter, hysteresis, (TemperatureRoomTarget + (TemperatureHysteresis / 2)) * 10);

    // Add needle
    indic = lv_meter_add_needle_line(meter, scale, 8, lv_palette_main(LV_PALETTE_RED), -10);
    lv_meter_set_indicator_value(meter, indic, 210);

    // Overwrite middle of meter
    lv_obj_t *inner_circle = lv_obj_create(scr);
    lv_obj_set_size(inner_circle, 130, 130);
    lv_obj_remove_style_all(inner_circle);
    lv_obj_align(inner_circle, LV_ALIGN_LEFT_MID, 120 - 130 / 2, 0);
    lv_obj_set_style_bg_color(inner_circle, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(inner_circle, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inner_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(inner_circle, 2, 0);
    lv_obj_set_style_border_color(inner_circle, lv_color_black(), 0);

    temperature_big = lv_label_create(inner_circle);  // Child of circle
    lv_label_set_text(temperature_big, "22,5");                    // Initial value
    lv_obj_set_style_text_font(temperature_big, &lv_font_montserrat_48, 0);  // Big font
    lv_obj_set_style_text_color(temperature_big, lv_color_black(), 0);
    lv_obj_center(temperature_big);    

    lv_obj_t * temperature_unit = lv_label_create(inner_circle);
    lv_label_set_text(temperature_unit, "°C");
    lv_obj_remove_style_all(temperature_unit);  // <-- THIS IS THE KEY!

    // Now re-apply only what you want
    lv_obj_set_style_text_font(temperature_unit, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(temperature_unit, lv_color_black(), 0);

    // Optional: keep it clickable? No → disable
    lv_obj_clear_flag(temperature_unit, LV_OBJ_FLAG_CLICKABLE);

    // Position
    lv_obj_align(temperature_unit, LV_ALIGN_CENTER, 0, 40);

      // Add event to button
    lv_obj_add_event_cb(btn_plus, [&hysteresis, &meter](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED)
        {
            Serial.println("Increase temperature");
            TemperatureRoomTarget += 0.1;
            lv_meter_set_indicator_start_value(meter, hysteresis, (TemperatureRoomTarget - (TemperatureHysteresis / 2)) * 10);
            lv_meter_set_indicator_end_value(meter, hysteresis, (TemperatureRoomTarget + (TemperatureHysteresis / 2)) * 10);
        }
    }, LV_EVENT_ALL, NULL);

    // Add event to button
    lv_obj_add_event_cb(btn_minus, [&hysteresis, &meter](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED)
        {
            Serial.println("Decrease temperature");
            TemperatureRoomTarget -= 0.1;
            lv_meter_set_indicator_start_value(meter, hysteresis, (TemperatureRoomTarget - (TemperatureHysteresis / 2)) * 10);
            lv_meter_set_indicator_end_value(meter, hysteresis, (TemperatureRoomTarget + (TemperatureHysteresis / 2)) * 10);
        }
    }, LV_EVENT_ALL, NULL);    

}

void loop_100ms()
{
  //int value = 0;
  //value = temp_GetTemperature(0);
  //lv_meter_set_indicator_value(meter, indic, value);   
}

void loop_1s()
{
  TemperatureRoom += 0.1;
  if(TemperatureRoom > 22.0) TemperatureRoom = 18.0;
  lv_meter_set_indicator_value(meter, indic, TemperatureRoom * 10);
  set_float_with_comma(temperature_big, TemperatureRoom, 1);
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