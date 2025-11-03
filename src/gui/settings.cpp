#include "gui/settings.h"
#include "gui.h"
#include <Arduino.h>

/* --------------------------------------------------------------------- */
lv_obj_t *scr_settings;
static lv_obj_t *checkbox_timeout;
static lv_obj_t *checkbox_req2;
static lv_obj_t *checkbox_swap;
static lv_obj_t *checkbox_ac;
static lv_obj_t *checkbox_auto_bright;

static lv_obj_t *slider_max_bright;
static lv_obj_t *label_max_bright;
static lv_obj_t *slider_hyst;
static lv_obj_t *label_hyst;

/* --------------------------------------------------------------------- */
static const char *KEY_TIMEOUT      = "timeout_en";
static const char *KEY_REQ2         = "req2";
static const char *KEY_SWAP         = "swap";
static const char *KEY_AC           = "ac";
static const char *KEY_AUTO_BRIGHT  = "auto_bright";
static const char *KEY_MAX_BRIGHT   = "max_bright";
static const char *KEY_HYST         = "hyst";

lv_obj_t *settings_get_screen(void)
{
    return scr_settings;
}

/* --------------------------------------------------------------------- */
/* LVGL 8 callback signature: void my_cb(_lv_event_t * e) */
static void slider_event_cb(lv_event_t *e)          // ← keep the argument!
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t v = lv_slider_get_value(slider);

    if (slider == slider_max_bright) {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", v);
        lv_label_set_text(label_max_bright, buf);
    } else if (slider == slider_hyst) {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%.1f", v / 10.0f);
        lv_label_set_text(label_hyst, buf);
    }
}

/* --------------------------------------------------------------------- */
static void btn_back_event(lv_event_t *) { settings_screen_save_and_exit(); }

/* --------------------------------------------------------------------- */
void settings_screen_create(lv_obj_t *)
{
    scr_settings = lv_obj_create(NULL);   // ← NULL = create a screen
    lv_obj_set_style_bg_color(scr_settings, lv_color_hex(0xF0F0F0), 0);

    /* ----- Title ----- */
    lv_obj_t *title = lv_label_create(scr_settings);
    lv_label_set_text(title, "Nastavenia");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* ----- Back button ----- */
    lv_obj_t *btn_back = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_back, 60, 40);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 5);
    lv_obj_add_event_cb(btn_back, btn_back_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);

    /* ----- Checkboxes ----- */
    int y = 60;
    checkbox_timeout = lv_checkbox_create(scr_settings);
    lv_checkbox_set_text(checkbox_timeout, "Display light timeout");
    lv_obj_align(checkbox_timeout, LV_ALIGN_TOP_LEFT, 20, y); y += 40;

    checkbox_req2 = lv_checkbox_create(scr_settings);
    lv_checkbox_set_text(checkbox_req2, "Require 2 temperature sensors");
    lv_obj_align(checkbox_req2, LV_ALIGN_TOP_LEFT, 20, y); y += 40;

    checkbox_swap = lv_checkbox_create(scr_settings);
    lv_checkbox_set_text(checkbox_swap, "Swap temperature sensors");
    lv_obj_align(checkbox_swap, LV_ALIGN_TOP_LEFT, 20, y); y += 40;

    checkbox_ac = lv_checkbox_create(scr_settings);
    lv_checkbox_set_text(checkbox_ac, "Require AC sense");
    lv_obj_align(checkbox_ac, LV_ALIGN_TOP_LEFT, 20, y); y += 40;

    checkbox_auto_bright = lv_checkbox_create(scr_settings);
    lv_checkbox_set_text(checkbox_auto_bright, "Automatic brightness");
    lv_obj_align(checkbox_auto_bright, LV_ALIGN_TOP_LEFT, 20, y); y += 50;

    /* ----- Max brightness ----- */
    slider_max_bright = lv_slider_create(scr_settings);
    lv_slider_set_range(slider_max_bright, 50, 255);
    lv_obj_set_width(slider_max_bright, 200);
    lv_obj_align(slider_max_bright, LV_ALIGN_TOP_LEFT, 20, y);
    lv_obj_add_event_cb(slider_max_bright, slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    label_max_bright = lv_label_create(scr_settings);
    lv_obj_align_to(label_max_bright, slider_max_bright, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    y += 50;

    /* ----- Hysteresis ----- */
    slider_hyst = lv_slider_create(scr_settings);
    lv_slider_set_range(slider_hyst, 1, 50);               // 0.1-5.0 °C
    lv_obj_set_width(slider_hyst, 200);
    lv_obj_align(slider_hyst, LV_ALIGN_TOP_LEFT, 20, y);
    lv_obj_add_event_cb(slider_hyst, slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    label_hyst = lv_label_create(scr_settings);
    lv_obj_align_to(label_hyst, slider_hyst, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    /* ----- Load saved values ----- */
    prefs.begin("gui", true);
    lv_obj_add_state(checkbox_timeout,      prefs.getBool(KEY_TIMEOUT,      true)  ? LV_STATE_CHECKED : 0);
    lv_obj_add_state(checkbox_req2,         prefs.getBool(KEY_REQ2,         false) ? LV_STATE_CHECKED : 0);
    lv_obj_add_state(checkbox_swap,         prefs.getBool(KEY_SWAP,         false) ? LV_STATE_CHECKED : 0);
    lv_obj_add_state(checkbox_ac,           prefs.getBool(KEY_AC,           false) ? LV_STATE_CHECKED : 0);
    lv_obj_add_state(checkbox_auto_bright,  prefs.getBool(KEY_AUTO_BRIGHT, true)  ? LV_STATE_CHECKED : 0);

    lv_slider_set_value(slider_max_bright, prefs.getUChar(KEY_MAX_BRIGHT, 255), LV_ANIM_OFF);
    lv_slider_set_value(slider_hyst,       prefs.getUShort(KEY_HYST, 5),      LV_ANIM_OFF);
    prefs.end();

    /* ----- Initialize slider labels with current values ----- */
    {
        int v = lv_slider_get_value(slider_max_bright);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", v);
        lv_label_set_text(label_max_bright, buf);
    }
    {
        int v = lv_slider_get_value(slider_hyst);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%.1f", v / 10.0f);
        lv_label_set_text(label_hyst, buf);
    }
}

/* --------------------------------------------------------------------- */
void settings_screen_save_and_exit()
{
    prefs.begin("gui", false);

    prefs.putBool(KEY_TIMEOUT,      lv_obj_has_state(checkbox_timeout,      LV_STATE_CHECKED));
    prefs.putBool(KEY_REQ2,         lv_obj_has_state(checkbox_req2,         LV_STATE_CHECKED));
    prefs.putBool(KEY_SWAP,         lv_obj_has_state(checkbox_swap,         LV_STATE_CHECKED));
    prefs.putBool(KEY_AC,           lv_obj_has_state(checkbox_ac,           LV_STATE_CHECKED));
    prefs.putBool(KEY_AUTO_BRIGHT,  lv_obj_has_state(checkbox_auto_bright,  LV_STATE_CHECKED));

    prefs.putUChar(KEY_MAX_BRIGHT, (uint8_t)lv_slider_get_value(slider_max_bright));
    prefs.putUShort(KEY_HYST,      (uint16_t)lv_slider_get_value(slider_hyst));

    prefs.end();

    lv_scr_load(scr_main);   // back to main screen
}

/* --------------------------------------------------------------------- */
/* Public getters – call from your main logic */
bool settings_display_timeout_enabled()   { return lv_obj_has_state(checkbox_timeout,      LV_STATE_CHECKED); }
bool settings_require_two_sensors()       { return lv_obj_has_state(checkbox_req2,         LV_STATE_CHECKED); }
bool settings_swap_sensors()              { return lv_obj_has_state(checkbox_swap,         LV_STATE_CHECKED); }
bool settings_require_ac_sense()          { return lv_obj_has_state(checkbox_ac,           LV_STATE_CHECKED); }
bool settings_auto_brightness()           { return lv_obj_has_state(checkbox_auto_bright,  LV_STATE_CHECKED); }
uint8_t settings_max_brightness()         { return (uint8_t)lv_slider_get_value(slider_max_bright); }
float   settings_hysteresis()             { return lv_slider_get_value(slider_hyst) / 10.0f; }