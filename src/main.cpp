#include <Arduino.h>
#include <TFT_eSPI.h>    // TFT display library
#include <lvgl.h>        // LVGL library
#include "temperature.h"
#include "output.h"
#include "touch.h"

TFT_eSPI tft = TFT_eSPI();  // Create TFT object

//#define DISPLAY_WIDTH 240   // Adjusted for rotation 1 (90° clockwise)
//#define DISPLAY_HEIGHT 320
#define DRAW_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8) // Pixels, not bytes

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

// LVGL UI objects
static lv_obj_t* room_label;
static lv_obj_t* floor_label;
static lv_obj_t* state_label;
static lv_obj_t* sequence_label;

// Display flush callback
void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
  //Serial.printf("Flushing area: x1=%d, y1=%d, x2=%d, y2=%d\n", area->x1, area->y1, area->x2, area->y2);

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void setup() {
    uint8_t temperature_sensor_count = 0;

    Serial.begin(115200);

    // Initialize display
    tft.init();
    tft.setRotation(1); // 90° clockwise (240x320)
    tft.fillScreen(TFT_BLACK);

    // Initialize LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, DRAW_BUF_SIZE);

    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
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

    // Create LVGL UI elements
    // Version label
    lv_obj_t* version_label = lv_label_create(lv_scr_act());
    lv_label_set_text(version_label, "v0.3");
    lv_obj_set_pos(version_label, 0, 0);

    // Temperature sensor count
    temperature_sensor_count = temp_Init();
    out_Init();
    char sensor_text[32];
    snprintf(sensor_text, sizeof(sensor_text), "Found %d temperature sensors", temperature_sensor_count);
    lv_obj_t* sensor_label = lv_label_create(lv_scr_act());
    lv_label_set_text(sensor_label, sensor_text);
    lv_obj_set_pos(sensor_label, 30, 0);

    // Room temperature label
    room_label = lv_label_create(lv_scr_act());
    lv_label_set_text(room_label, "Izba:");
    lv_obj_set_pos(room_label, 0, 60);

    // Floor temperature label
    floor_label = lv_label_create(lv_scr_act());
    lv_label_set_text(floor_label, "Podlaha:");
    lv_obj_set_pos(floor_label, 0, 90);

    // State label
    state_label = lv_label_create(lv_scr_act());
    lv_label_set_text(state_label, "Stav:");
    lv_obj_set_pos(state_label, 0, 120);

    // Sequence label
    sequence_label = lv_label_create(lv_scr_act());
    lv_label_set_text(sequence_label, "Sekvencia:");
    lv_obj_set_pos(sequence_label, 0, 150);

    // Test button
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, 100, 180); // Adjusted to avoid overlap
    lv_obj_set_size(btn, 120, 50);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Test Button");
    lv_obj_center(btn_label);

// After creating all labels and button
    lv_obj_t* test_label = lv_label_create(lv_scr_act());
    lv_label_set_text(test_label, "Testing...");
    lv_obj_set_pos(test_label, 50, 50);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, test_label);
    lv_anim_set_values(&a, 50, 150);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
        lv_obj_set_x((lv_obj_t*)var, v);
    });
    lv_anim_start(&a);

    // Force initial screen redraw
    lv_obj_invalidate(lv_scr_act());
}

void loop_100ms(void) {
    // Add 100ms tasks if needed
}

void loop_1s(void) {
    Serial.print("ACsense: ");
    bool ACpresent = measureOutput();
    Serial.println(ACpresent ? "1" : "0");

    CurrentOutState = out_Get();
    f_RoomTemperature = temp_GetTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetTemperature(TEMP_SENSOR_FLOOR);

    if ((f_RoomTemperature == TEMP_SENSOR_NOT_CONNECTED) || (f_FloorTemperature == TEMP_SENSOR_NOT_CONNECTED)) {
        out_EnterDeadState();
    } else {
        if ((f_RoomTemperature < f_RoomTempTarget) && (f_FloorTemperature < f_FloorTempTarget)) {
            out_TurnOnHeatingElement();
        } else if ((f_RoomTemperature > (f_RoomTempTarget + f_TempHysteresis)) || (f_FloorTemperature > f_FloorTempTarget)) {
            out_TurnOffHeatingElement();
        }
    }

    // Update LVGL labels
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Izba: %.1f C / %.1f C", f_RoomTemperature, f_RoomTempTarget);
    lv_label_set_text(room_label, buffer);
    //lv_obj_invalidate(room_label);

    snprintf(buffer, sizeof(buffer), "Podlaha: %.1f C / %.1f C", f_FloorTemperature, f_FloorTempTarget);
    lv_label_set_text(floor_label, buffer);
    //lv_obj_invalidate(floor_label);

    if (CurrentSequenceState == DEAD) {
        lv_label_set_text(state_label, "Stav: Chyba");
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFF0000), LV_PART_MAIN); // Red background
    } else if (CurrentSequenceState != IDLE) {
        lv_label_set_text(state_label, "Stav: Prepinam");
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0000FF), LV_PART_MAIN); // Blue background
    } else if (!CurrentOutState) {
        lv_label_set_text(state_label, "Stav: Vypnute");
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN); // Black background
    } else {
        lv_label_set_text(state_label, "Stav: Zapnute");
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x00FF00), LV_PART_MAIN); // Green background
    }

    switch (CurrentSequenceState) {
        case IDLE: lv_label_set_text(sequence_label, "Sekvencia: IDLE"); break;
        case TURNING_ON_SAFETY: lv_label_set_text(sequence_label, "Sekvencia: TURNING_ON_SAFETY"); break;
        case TURNING_ON_WORK: lv_label_set_text(sequence_label, "Sekvencia: TURNING_ON_WORK"); break;
        case VERIFY_ON: lv_label_set_text(sequence_label, "Sekvencia: VERIFY_ON"); break;
        case TURNING_OFF_WORK: lv_label_set_text(sequence_label, "Sekvencia: TURNING_OFF_WORK"); break;
        case TURNING_OFF_SAFETY: lv_label_set_text(sequence_label, "Sekvencia: TURNING_OFF_SAFETY"); break;
        case VERIFY_OFF: lv_label_set_text(sequence_label, "Sekvencia: VERIFY_OFF"); break;
        case DEAD: lv_label_set_text(sequence_label, "Sekvencia: DEAD"); break;
    }

    // Force screen redraw
    //lv_obj_invalidate(lv_scr_act());
}

void loop_8s(void) {
    CurrentSequenceState = out_ControlRelays();
}

void loop() {
    u16_Time++;
    u16_Time &= 8191; // Bitwise AND for modulo 2^13

    lv_tick_inc(10); // Increment LVGL tick (10ms)
    //Serial.println("Running lv_task_handler");
    lv_task_handler(); // Handle LVGL tasks

    if ((u16_Time % 800) == 0) { // 8s
        loop_8s();
        u16_Time = 0;
    } else if ((u16_Time % 100) == 0) { // 1s
        loop_1s();
    } else if ((u16_Time % 10) == 0) { // 100ms
        loop_100ms();
    }
/*
    if (digitalRead(25) == LOW) {
        Serial.println("INT pin LOW (touch detected)");
    } else {
        Serial.println("INT pin HIGH (no touch)");
    }
*/
    delay(10);
}