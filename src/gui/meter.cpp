// src/gui/meter.cpp
#include "meter.h"
#include "gui.h"  // for access to global meter objects if needed

// Global objects (still accessible from main.cpp)
lv_obj_t *meter;
lv_meter_indicator_t *indic;
lv_meter_indicator_t *indic2;
lv_meter_indicator_t *hysteresis;
lv_obj_t *temperature_big;
lv_obj_t *temperature_target;
lv_obj_t *state;

lv_obj_t *saf_relay_led;
lv_obj_t *work_relay_led;
lv_obj_t *ac_led;

static void set_float_with_comma(lv_obj_t *label, float value, int decimals)
{
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    for (char *p = buf; *p; ++p) if (*p == '.') *p = ',';
    lv_label_set_text(label, buf);
}

void gui_CreateIndicators(lv_obj_t *parent)
{
  saf_relay_led = lv_led_create(parent);
  lv_obj_align(saf_relay_led, LV_ALIGN_CENTER, -30, 75);

  work_relay_led = lv_led_create(parent);
  lv_obj_align(work_relay_led, LV_ALIGN_CENTER, 0, 80);

  ac_led = lv_led_create(parent);
  lv_obj_align(ac_led, LV_ALIGN_CENTER, 30, 75);

  lv_led_off(saf_relay_led);
  lv_led_off(work_relay_led);
  lv_led_off(ac_led);
}

void gui_UpdateIndicators(bool saf_relay, bool work_relay, bool ac_sense)
{
  saf_relay  ? lv_led_on(saf_relay_led)  : lv_led_off(saf_relay_led);
  work_relay ? lv_led_on(work_relay_led) : lv_led_off(work_relay_led);
  ac_sense   ? lv_led_on(ac_led)         : lv_led_off(ac_led);
}

lv_obj_t* create_temperature_meter(
    lv_obj_t *parent,
    int16_t x, int16_t y,
    uint16_t radius,
    float min_temp, float max_temp,
    float min_angle, float max_angle
)
{
    // === 1. Create meter ===
    meter = lv_meter_create(parent);
    lv_obj_set_pos(meter, x, y);
    lv_obj_set_size(meter, radius * 2, radius * 2);

    // === 2. Main scale (for needle & arc) ===
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, scale,
                             (int)(min_temp * 10), (int)(max_temp * 10),
                             (uint16_t)(max_angle - min_angle), (int16_t)min_angle);

    // Ticks: 41 minor, major every 10 units (1.0°C)
    lv_meter_set_scale_ticks(meter, scale, (max_temp - min_temp) * 10 + 1, 2, 5, lv_color_black());
    lv_meter_set_scale_major_ticks(meter, scale, 10, 4, 20, lv_color_black(), -100); // -100 = labels outside

    // === 3. Number scale (for visible °C labels) ===
    lv_meter_scale_t *scale_num = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, scale_num,
                             (int)min_temp, (int)max_temp,
                             (uint16_t)(max_angle - min_angle), (int16_t)min_angle);
    lv_meter_set_scale_ticks(meter, scale_num, (max_temp - min_temp) * 10 + 1, 0, 0, lv_color_black());
    lv_meter_set_scale_major_ticks(meter, scale_num, 10, 0, 0, lv_color_black(), 30); // 30 = label offset

    // === 4. Outline arc ===
    lv_meter_indicator_t *outline = lv_meter_add_arc(meter, scale, 2, lv_color_black(), 2);
    lv_meter_set_indicator_start_value(meter, outline, (int)(min_temp * 10));
    lv_meter_set_indicator_end_value(meter, outline, (int)(max_temp * 10));

    // === 5. Hysteresis arc (green) ===
    hysteresis = lv_meter_add_arc(meter, scale, 50, lv_color_hex(0x0000FF), 0);

    // === 6. Needle ===
    indic = lv_meter_add_needle_line(meter, scale, 8, lv_color_hex(0xFF0000), 0);
    indic2 = lv_meter_add_needle_line(meter, scale, 1, lv_color_hex(0x00FF00), 10);

    // === 7. Inner circle (white background) ===
    lv_obj_t *inner = lv_obj_create(parent);
    uint16_t inner_size = (uint16_t)(radius * 1.1f);
    lv_obj_set_size(inner, inner_size, inner_size);
    lv_obj_align(inner, LV_ALIGN_CENTER, -40, 0);
    lv_obj_set_style_bg_color(inner, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(inner, 2, 0);
    lv_obj_set_style_border_color(inner, lv_color_black(), 0);
    //lv_obj_clear_flag(inner, LV_OBJ_FLAG_CLICKABLE);     // No clicks
    //lv_obj_add_flag(inner, LV_OBJ_FLAG_SCROLLABLE);      // Prevent parent scroll
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);        // No scrolling

    // === 8. Target temperature label ===
    temperature_target = lv_label_create(inner);
    lv_obj_set_style_text_font(temperature_target, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temperature_target, lv_color_black(), 0);
    lv_obj_align(temperature_target, LV_ALIGN_CENTER, 0, -35);
    set_float_with_comma(temperature_target, 20.0f, 1);  // initial

    // === 8. Big temperature label ===
    temperature_big = lv_label_create(inner);
    lv_obj_set_style_text_font(temperature_big, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(temperature_big, lv_color_black(), 0);
    lv_obj_center(temperature_big);
    set_float_with_comma(temperature_big, 20.0f, 1);  // initial

    // === 9. Unit label ===
    lv_obj_t *unit = lv_label_create(inner);
    lv_label_set_text(unit, "°C");
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(unit, lv_color_black(), 0);
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 40);
    
    gui_CreateIndicators(meter);

    state = lv_label_create(meter);
    lv_label_set_text(state, "init");
    lv_obj_set_style_text_font(state, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(state, lv_color_black(), 0);
    lv_obj_align(state, LV_ALIGN_CENTER, 0, 100);

    // Floor temperature reading
    // Heat indicator

    return meter;
}