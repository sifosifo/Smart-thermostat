// src/gui/gui.cpp
#include "gui.h"
#include <TFT_eSPI.h>
#include "touch.h"
#include "gui/meter.h"
#include "gui/settings.h"
#include <Preferences.h>

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static const char* build_date = __DATE__;
static const char* build_time = __TIME__;

TFT_eSPI tft = TFT_eSPI();

/* ----- LVGL buffers ------------------------------------------------------ */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

bool display_timeout_enabled = true;
bool thermostat_enabled = true;

Preferences prefs;

// ---------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------
lv_obj_t *scr_main;
static lv_obj_t *scr_settings;
static lv_obj_t *checkbox_timeout;

/* ----- Long-press detection for the top button (+1.0) ----- */
static uint32_t longpress_btn_id = LV_BTNMATRIX_BTN_NONE;

static void create_settings_screen(void);
static void btnmatrix_event_cb(lv_event_t);
static void OFF_event_cb();

/* ----- Helper ----------------------------------------------------------- */
static void set_float_with_comma(lv_obj_t *label, float value, int decimals)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    for (char *p = buf; *p; ++p) if (*p == '.') *p = ',';
    lv_label_set_text(label, buf);
}

/* ----- Flush callback --------------------------------------------------- */
static void my_disp_flush(lv_disp_drv_t *disp,
                          const lv_area_t *area,
                          lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    lv_disp_flush_ready(disp);
}

/* ----- Button events ---------------------------------------------------- */
static void btn_plus_event(lv_event_t *)  { gui_increase_target(); }
static void btn_minus_event(lv_event_t *) { gui_decrease_target(); }
static void btnmatrix_event_cb(lv_event_t *e);

/* ------------------------------------------------------------------------ */
void gui_init()
{
    /* ---- Display ---- */
    tft.begin();
    tft.setRotation(1);
    tft.initDMA();
    tft.setSwapBytes(true);     // lv_color_hex() will work with RGB order

    pinMode(27, OUTPUT);
    digitalWrite(27, LOW);
    delay(10);
    digitalWrite(27, HIGH);

    /* ---- LVGL buffers ---- */
    buf1 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * screenWidth * 100,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * screenWidth * 100,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 100);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* ---- Touch ---- */
    touch_init();

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    /* ---- Screen ---- */
    scr_main = lv_scr_act();

    lv_obj_t *bdate = lv_label_create(scr_main);
    lv_obj_set_style_text_font(bdate, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bdate, lv_color_black(), 0);
    lv_obj_align(bdate, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(bdate, build_date);

    lv_obj_t *btime = lv_label_create(scr_main);
    lv_obj_set_style_text_font(btime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btime, lv_color_black(), 0);
    lv_obj_align(btime, LV_ALIGN_TOP_LEFT, 0, 15);
    lv_label_set_text(btime, build_time);

    lv_obj_t *version = lv_label_create(scr_main);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, lv_color_black(), 0);
    lv_obj_align(version, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_label_set_text(version, "v0.4");

 // === 4 VERTICAL BUTTONS — 1 COLUMN, 4 ROWS ===
#define SPACES "             "

 static const char *btn_map[] =
{
    SPACES LV_SYMBOL_SETTINGS, "\n",
    SPACES LV_SYMBOL_PLUS, "\n",
    SPACES LV_SYMBOL_MINUS, "\n",
    SPACES LV_SYMBOL_POWER, "",
};

lv_obj_t *btnmatrix = lv_btnmatrix_create(scr_main);
lv_btnmatrix_set_map(btnmatrix, btn_map);

lv_obj_set_size(btnmatrix, 220, screenHeight);  // Narrow width → no wrap
lv_obj_align(btnmatrix, LV_ALIGN_RIGHT_MID, 0, 0);

// Style
lv_obj_set_style_pad_all(btnmatrix, 2, 0);
lv_obj_set_style_pad_row(btnmatrix, 8, 0);
lv_obj_set_style_text_font(btnmatrix, &lv_font_montserrat_42, 0);
lv_obj_set_style_radius(btnmatrix, 8, LV_PART_ITEMS);
lv_obj_set_style_bg_color(btnmatrix, lv_color_hex(0x0077FF), LV_PART_ITEMS | LV_STATE_PRESSED);

// Event
lv_obj_add_event_cb(btnmatrix, btnmatrix_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

     // === CREATE METER WITH NAMED PARAMS ===
    create_temperature_meter(
        scr_main,
        0, 0,                    // x, y
        120,                     // radius
        18.0f, 25.0f,            // min/max temp
        135.0f, 405.0f           // min/max angle (270° arc)
    );

    settings_screen_create(nullptr);

    tft.startWrite();
}

void gui_update_state(char *buf)
{
    lv_label_set_text(state, buf);
}

/* ------------------------------------------------------------------------ */
void gui_update_temperature(float current, float target, float hyst, float TemperatureFloor, float TemperatureFloorTarget)
{
    lv_meter_set_indicator_value(meter, indic, current * 10);
    lv_meter_set_indicator_value(meter, indic2, TemperatureFloor * 10);

    float start = target - hyst/2;
    float end   = target + hyst/2;
    lv_meter_set_indicator_start_value(meter, hysteresis, start*10);
    lv_meter_set_indicator_end_value  (meter, hysteresis, end*10);

    set_float_with_comma(temperature_big, current, 1);
    set_float_with_comma(temperature_target, target, 1);
}

/* ------------------------------------------------------------------------ */
void gui_increase_target()
{
    extern float f_RoomTempTarget;
    extern float f_RoomTemperature;
    extern float f_TempHysteresis;

    f_RoomTempTarget += 0.5f;

    // IMMEDIATE GUI UPDATE
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, 22, 25);
}

void gui_decrease_target()
{
    extern float f_RoomTempTarget;
    extern float f_RoomTemperature;
    extern float f_TempHysteresis;

    f_RoomTempTarget -= 0.5f;

    // IMMEDIATE GUI UPDATE
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, 22, 25);
}

static void btn_back_event(lv_event_t *e)
{
    lv_scr_load(scr_main);               // return to main
}

static void goto_settings_cb()
{
    lv_obj_t *scr = settings_get_screen();
    if (scr) {
        lv_scr_load(scr);
    }
}

/* ----- Button-matrix event (replace the old one) ----- */
static void btnmatrix_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);

    /* ---- Short press ---- */
    switch (id) {
        case 0: goto_settings_cb(); break;      // Settings
        case 1: gui_increase_target(); break;   // + short
        case 2: gui_decrease_target(); break;   // - short
        case 3: OFF_event_cb(); break;          // ON/OFF
    }
}

/* --------------------------------------------------------------------
 *  Apply a colour filter to a container (or the whole screen)
 *  tint = 0 → no filter
 *  tint = LV_COLOR_MAKE(r,g,b) → colour overlay
 *  opacity = 0-255 (LV_OPA_COVER = 255)
 * -------------------------------------------------------------------- */
static lv_obj_t * tint_layer = NULL;   // one global overlay

void ui_apply_tint(lv_color_t tint, uint8_t opa)
{
    if (!tint_layer) {
        tint_layer = lv_obj_create(scr_main);
        lv_obj_set_size(tint_layer, LV_PCT(100), LV_PCT(100));
        lv_obj_align(tint_layer, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(tint_layer, LV_OPA_TRANSP, 0);

        // CRITICAL: Let touches pass through
        lv_obj_add_flag(tint_layer, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(tint_layer, LV_OBJ_FLAG_CLICKABLE);  // Don't steal clicks
    }

    if (tint.full == 0) {
        lv_obj_add_flag(tint_layer, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(tint_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(tint_layer, tint, 0);
    lv_obj_set_style_bg_opa(tint_layer, opa, 0);
}

/* Greyscale filter – 30 % grey */
#define GREY_TINT   lv_color_make(0x80,0x80,0x80)

/* Red error filter – 40 % red */
#define RED_TINT    lv_color_make(0xFF,0x00,0x00)

static void OFF_event_cb()
{
    thermostat_enabled = !thermostat_enabled;

    if(thermostat_enabled)
    {
        ui_apply_tint(lv_color_make(0,0,0), 0);
        lv_obj_set_style_img_recolor_opa(meter, LV_OPA_TRANSP, 0);
    }else
    {
        ui_apply_tint(GREY_TINT, LV_OPA_80);
        lv_obj_set_style_img_recolor_opa(meter, LV_OPA_20, 0);
        lv_obj_set_style_img_recolor(meter, lv_color_white(), 0);
    }

    Serial.printf("Enabled: %s\n", thermostat_enabled ? "true" : "false");
}

bool gui_check_if_enabled()
{
    return(thermostat_enabled);
}