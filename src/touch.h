#ifndef TOUCH_H
#define TOUCH_H

#include "CST820.h"
#include <lvgl.h> // Add this to include LVGL types

#define I2C_SDA 33
#define I2C_SCL 32
#define TP_RST 25
#define TP_INT 21

void touch_init(void);
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

#endif