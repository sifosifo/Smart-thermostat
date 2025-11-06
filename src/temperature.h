#ifndef TEMPERATURE_H_INCLUDED
#define TEMPERATURE_H_INCLUDED

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS1 22

#define TEMP_SENSOR_NOT_CONNECTED  -127

#define TEMP_SENSOR_COUNT   2
#define TEMP_SENSOR_FLOOR   1
#define TEMP_SENSOR_ROOM    0

uint8_t temp_Init(void);
float temp_GetTemperature(uint8_t u8_sensor);
float temp_GetTemperatureTarget(uint8_t u8_sensor);
void temp_SetTemperatureTarget(uint8_t u8_sensor, float f_value);
bool temp_swap_sensors(bool swap_sensors);
uint8_t temp_GetSensorCount(void);
uint8_t temp_Process(bool two_sensors_required);

#endif