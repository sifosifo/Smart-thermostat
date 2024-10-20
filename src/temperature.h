#ifndef TEMPERATURE_H_INCLUDED
#define TEMPERATURE_H_INCLUDED

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS1 27

#define TEMP_SENSOR_NOT_CONNECTED  -127

#define TEMP_SENSOR_COUNT   2
#define TEMP_SENSOR_FLOOR   0
#define TEMP_SENSOR_ROOM    1

void temp_Init(void);
float temp_GetTemperature(uint8_t u8_sensor);

#endif