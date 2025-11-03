#include "touch.h"
#include "CST820.h"
#include <Arduino.h>

CST820 touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    bool touched;
    uint8_t gesture;
    uint16_t touchX, touchY;

    touched = touch.getTouch(&touchX, &touchY, &gesture);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        // Adjust coordinates for screen rotation = 1 (landscape)
        uint16_t newX = touchY;
        uint16_t newY = 240 - touchX;   // Invert Y axis

        data->point.x = newX;
        data->point.y = newY;

        //Serial.printf("Raw: X=%u Y=%u -> Mapped: X=%u Y=%u\n", touchX, touchY, newX, newY);
    }
}

void touch_init(void)
{
    touch.begin();
}