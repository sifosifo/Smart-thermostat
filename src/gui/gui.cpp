// src/gui/gui.cpp
#include "gui.h"
#include <TFT_eSPI.h>
#include "touch.h"
#include "gui/meter.h"
#include "gui/settings.h"
#include <Preferences.h>
#include "temperature.h"

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
// Keep double buffering while reserving more internal heap for Wi-Fi/TLS handshakes.
static const uint16_t drawBufferLines = 40;

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
uint8_t counter_s = 255;

// ---------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------
lv_obj_t *scr_main;
static lv_obj_t *scr_settings;
static lv_obj_t *checkbox_timeout;
lv_obj_t *Tcount_l;
static lv_obj_t *network_ap_label;
static lv_obj_t *network_wifi_icon;
static lv_obj_t *network_status_bar;
static lv_obj_t *network_globe[3];
static lv_obj_t *network_server_slots[3];
static lv_obj_t *network_server_leds[3];
static int16_t last_network_status = -1;
static uint32_t last_status_blink_ms = 0;
static bool status_blink_visible = true;

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
static void btn_plus_event(lv_event_t *)  { temp_increase_target(); }
static void btn_minus_event(lv_event_t *) { temp_decrease_target(); }
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

    prefs.begin("gui", true);  // Read-only mode.
    uint8_t mvm_target = prefs.getInt(KEY_TARGET_TEMPERATURE, 0);  // Load from NVM
    prefs.end();
    temp_SetTemperatureTarget(TEMP_SENSOR_ROOM, 15 + ((float)mvm_target / 10.0f));

    /* ---- LVGL buffers ---- */
    buf1 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * screenWidth * drawBufferLines,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t *)heap_caps_malloc(
        sizeof(lv_color_t) * screenWidth * drawBufferLines,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * drawBufferLines);

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
    lv_label_set_text(version, "v1.0");

    // Detected temperature sensors / required temperature sensors
    Tcount_l = lv_label_create(scr_main);
    lv_obj_set_style_text_font(Tcount_l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(Tcount_l, lv_color_black(), 0);
    lv_obj_align(Tcount_l, LV_ALIGN_TOP_LEFT, 0, 45);
    lv_label_set_text(Tcount_l, "-/-");

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

    // Connectivity indicators at the bottom-left, clear of the meter's center.
    network_status_bar = lv_obj_create(scr_main);
    lv_obj_set_pos(network_status_bar, 0, screenHeight - 40);
    lv_obj_set_size(network_status_bar, 100, 40);
    lv_obj_set_style_bg_opa(network_status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(network_status_bar, 0, 0);
    lv_obj_set_style_shadow_width(network_status_bar, 0, 0);
    lv_obj_set_style_pad_all(network_status_bar, 0, 0);
    lv_obj_clear_flag(network_status_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    const lv_color_t inactive = lv_color_hex(0xD8D8D8);

    network_ap_label = lv_label_create(network_status_bar);
    lv_label_set_text(network_ap_label, "AP");
    lv_obj_set_style_text_font(network_ap_label, &lv_font_montserrat_14, 0);
    lv_obj_align(network_ap_label, LV_ALIGN_LEFT_MID, 2, -9);
    lv_obj_set_style_text_color(network_ap_label, inactive, 0);
    lv_obj_add_flag(network_ap_label, LV_OBJ_FLAG_HIDDEN);

    network_wifi_icon = lv_label_create(network_status_bar);
    lv_label_set_text(network_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(network_wifi_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(network_wifi_icon, LV_ALIGN_LEFT_MID, 2, -9);
    lv_obj_set_style_text_color(network_wifi_icon, inactive, 0);

    // Draw a globe with LVGL primitives so its shape does not depend on a font glyph.
    network_globe[0] = lv_obj_create(network_status_bar);
    lv_obj_set_size(network_globe[0], 14, 14);
    lv_obj_align(network_globe[0], LV_ALIGN_LEFT_MID, 2, 9);
    lv_obj_set_style_radius(network_globe[0], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(network_globe[0], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(network_globe[0], 1, 0);
    lv_obj_set_style_border_color(network_globe[0], inactive, 0);
    lv_obj_clear_flag(network_globe[0], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    network_globe[1] = lv_obj_create(network_globe[0]);
    lv_obj_set_size(network_globe[1], 6, 12);
    lv_obj_center(network_globe[1]);
    lv_obj_set_style_radius(network_globe[1], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(network_globe[1], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(network_globe[1], 1, 0);
    lv_obj_set_style_border_color(network_globe[1], inactive, 0);
    lv_obj_clear_flag(network_globe[1], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    network_globe[2] = lv_obj_create(network_globe[0]);
    lv_obj_set_size(network_globe[2], 10, 1);
    lv_obj_center(network_globe[2]);
    lv_obj_set_style_bg_color(network_globe[2], inactive, 0);
    lv_obj_set_style_bg_opa(network_globe[2], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(network_globe[2], 0, 0);
    lv_obj_clear_flag(network_globe[2], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Three stacked slots form a compact server symbol.
    for (uint8_t i = 0; i < 3; ++i) {
        network_server_slots[i] = lv_obj_create(network_status_bar);
        lv_obj_set_size(network_server_slots[i], 14, 4);
        lv_obj_align(network_server_slots[i], LV_ALIGN_LEFT_MID, 26, (int16_t)i * 5 + 4);
        lv_obj_set_style_radius(network_server_slots[i], 1, 0);
        lv_obj_set_style_bg_opa(network_server_slots[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(network_server_slots[i], 1, 0);
        lv_obj_set_style_border_color(network_server_slots[i], inactive, 0);
        lv_obj_clear_flag(network_server_slots[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        network_server_leds[i] = lv_obj_create(network_server_slots[i]);
        lv_obj_set_size(network_server_leds[i], 2, 2);
        lv_obj_align(network_server_leds[i], LV_ALIGN_RIGHT_MID, -1, 0);
        lv_obj_set_style_radius(network_server_leds[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(network_server_leds[i], inactive, 0);
        lv_obj_set_style_bg_opa(network_server_leds[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(network_server_leds[i], 0, 0);
        lv_obj_clear_flag(network_server_leds[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    settings_screen_create(nullptr);

    tft.startWrite();
}

void gui_update_network_status(bool wifi_enabled, bool setup_ap_available, bool wifi_connecting,
                               bool wifi_connected, bool internet_accessible,
                               int server_http_status)
{
    if (!network_wifi_icon) return;

    const uint8_t server_state = server_http_status <= 0 ? 0 :
                                 (server_http_status < 300 && server_http_status >= 200 ? 2 : 1);
    const uint16_t states = (wifi_enabled ? 1u : 0u) |
                            (setup_ap_available ? 2u : 0u) |
                            (wifi_connecting ? 4u : 0u) |
                            (wifi_connected ? 8u : 0u) |
                            (internet_accessible ? 16u : 0u) |
                            (static_cast<uint16_t>(server_state) << 5);

    const lv_color_t inactive = lv_color_hex(0xD8D8D8);
    const lv_color_t active = lv_color_hex(0x168A36);
    const lv_color_t pending = lv_color_hex(0xE08A00);

    const bool status_changed = last_network_status != static_cast<int16_t>(states);
    const bool ap_active = wifi_enabled && setup_ap_available;
    const bool should_blink = wifi_enabled && !ap_active &&
                              (wifi_connecting || !internet_accessible || server_state == 0);
    const uint32_t now = millis();
    bool blink_changed = false;
    if (should_blink && now - last_status_blink_ms >= 450) {
        last_status_blink_ms = now;
        status_blink_visible = !status_blink_visible;
        blink_changed = true;
    } else if (!should_blink && !status_blink_visible) {
        status_blink_visible = true;
        blink_changed = true;
    }

    if (!status_changed && !blink_changed) return;
    last_network_status = static_cast<int16_t>(states);

    if (wifi_enabled) lv_obj_clear_flag(network_status_bar, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(network_status_bar, LV_OBJ_FLAG_HIDDEN);

    if (ap_active) lv_obj_clear_flag(network_ap_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(network_ap_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_enabled && !ap_active) lv_obj_clear_flag(network_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(network_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(network_ap_label, ap_active ? active : inactive, 0);

    lv_obj_set_style_text_color(network_wifi_icon,
                                wifi_connecting ? pending : (wifi_connected ? active : inactive), 0);
    lv_obj_set_style_text_opa(network_wifi_icon,
                              wifi_connecting && !status_blink_visible ? LV_OPA_30 : LV_OPA_COVER, 0);

    const lv_color_t blink_off = lv_color_hex(0xFFFFFF);
    if (!ap_active) {
        lv_obj_clear_flag(network_globe[0], LV_OBJ_FLAG_HIDDEN);
        for (uint8_t i = 0; i < 3; ++i)
            lv_obj_clear_flag(network_server_slots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(network_globe[0], LV_OBJ_FLAG_HIDDEN);
        for (uint8_t i = 0; i < 3; ++i)
            lv_obj_add_flag(network_server_slots[i], LV_OBJ_FLAG_HIDDEN);
    }
    const lv_color_t globe_color = internet_accessible ? active :
                                   (status_blink_visible ? inactive : blink_off);
    lv_obj_set_style_border_color(network_globe[0], globe_color, 0);
    lv_obj_set_style_border_color(network_globe[1], globe_color, 0);
    lv_obj_set_style_bg_color(network_globe[2], globe_color, 0);

    const lv_color_t server_color = server_state == 0 ?
                                    (status_blink_visible ? inactive : blink_off) :
                                    (server_state == 2 ? active : lv_color_hex(0xD02020));
    for (uint8_t i = 0; i < 3; ++i) {
        lv_obj_set_style_border_color(network_server_slots[i], server_color, 0);
        lv_obj_set_style_bg_color(network_server_leds[i], server_color, 0);
    }
}

void gui_update_state(char *buf)
{
    lv_label_set_text(state, buf);
}

/* ------------------------------------------------------------------------ */
void gui_update_temperature(float current, float target, float hyst, float TemperatureFloor, float TemperatureFloorTarget)
{
    if(counter_s != 255)
    {
        counter_s += 2;
        if(counter_s >= 60)
        {
            counter_s = 255;

            // Save target temperature to NVM
            prefs.begin("gui", false);  // Read-write mode.
            uint8_t mvm_target = (uint8_t)((temp_GetTemperatureTarget(TEMP_SENSOR_ROOM) - 15.0f) * 10.0f);
            prefs.putInt(KEY_TARGET_TEMPERATURE, mvm_target);
            prefs.end();
            Serial.printf("Saved target temperature: %d\n", mvm_target);
        }
    } 

    lv_meter_set_indicator_value(meter, indic, current * 10);
    lv_meter_set_indicator_value(meter, indic2, TemperatureFloor * 10);

    float start = target - hyst/2;
    float end   = target + hyst/2;
    lv_meter_set_indicator_start_value(meter, hysteresis, start*10);
    lv_meter_set_indicator_end_value  (meter, hysteresis, end*10);

    set_float_with_comma(temperature_big, current, 1);
    set_float_with_comma(temperature_target, target, 1);
}

void gui_UpdateSensorsCount(uint8_t count, bool two_sensors_required)
{
    char buf[8];

    lv_snprintf(buf, sizeof(buf), "%d/%d", count, two_sensors_required ? 2 : 1);
    lv_label_set_text(Tcount_l, buf);
}

/* ------------------------------------------------------------------------ */
void temp_increase_target()
{
    float f_RoomTempTarget;
    float f_RoomTemperature;
    float f_TempHysteresis;
    float f_FloorTemperature;

    counter_s = 0;  // start timer countdown for saving
    f_TempHysteresis = settings_hysteresis();
    f_RoomTemperature = temp_GetCachedTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetCachedTemperature(TEMP_SENSOR_FLOOR);
    f_RoomTempTarget = temp_GetTemperatureTarget(TEMP_SENSOR_ROOM);
    f_RoomTempTarget += 0.5f;
    temp_SetTemperatureTarget(TEMP_SENSOR_ROOM, f_RoomTempTarget);
    Serial.printf("Increase target\n");
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, f_FloorTemperature, 25);
}

void temp_decrease_target()
{
    float f_RoomTempTarget;
    float f_RoomTemperature;
    float f_TempHysteresis;
    float f_FloorTemperature;

    counter_s = 0;  // start timer countdown for saving
    f_TempHysteresis = settings_hysteresis();
    f_RoomTemperature = temp_GetCachedTemperature(TEMP_SENSOR_ROOM);
    f_FloorTemperature = temp_GetCachedTemperature(TEMP_SENSOR_FLOOR);
    f_RoomTempTarget = temp_GetTemperatureTarget(TEMP_SENSOR_ROOM);
    f_RoomTempTarget -= 0.5f;
    temp_SetTemperatureTarget(TEMP_SENSOR_ROOM, f_RoomTempTarget);
     Serial.printf("Decrease target\n");
    gui_update_temperature(f_RoomTemperature, f_RoomTempTarget, f_TempHysteresis, f_FloorTemperature, 25);
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
        case 1: temp_increase_target(); break;   // + short
        case 2: temp_decrease_target(); break;   // - short
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
    gui_set_thermostat_enabled(!thermostat_enabled);
}

void gui_set_thermostat_enabled(bool enabled)
{
    thermostat_enabled = enabled;

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
