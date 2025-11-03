#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature.h"

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS1);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress SensorAddress[TEMP_SENSOR_COUNT];

#define TEMP_HISTORY_LEN 16

// Circular buffers for smoothing
float tempHistory[TEMP_SENSOR_COUNT][TEMP_HISTORY_LEN];
uint8_t tempIndex[TEMP_SENSOR_COUNT] = {0};
bool tempFilled[TEMP_SENSOR_COUNT] = {false};


uint8_t u8_temp_count = 0;

uint8_t u8_ErrorCode[TEMP_SENSOR_COUNT] = {0, 0};

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

uint8_t temp_Init(void)
{
  pinMode(ONE_WIRE_BUS1, INPUT_PULLUP);
  sensors.begin();

  Serial.println("Locating devices...");
  u8_temp_count = sensors.getDeviceCount();
  Serial.printf("Found %d devices.\n", u8_temp_count);

  if (u8_temp_count == 0)
  {
    Serial.println("❌ No OneWire devices found!");
    return 0;
  }

  // Try to get addresses for all expected sensors
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (sensors.getAddress(SensorAddress[i], i))
    {
      Serial.printf("Sensor %d address: ", i);
      printAddress(SensorAddress[i]);
      Serial.println();
    }
    else
    {
      Serial.printf("⚠️ Unable to find address for sensor %d\n", i);
      u8_ErrorCode[i] = 1;
    }

    // Set resolution to 9 bits for faster reads (can use 12 for higher accuracy)
    sensors.setResolution(SensorAddress[i], 9);
  }

  return u8_temp_count;
}

float temp_GetTemperature(uint8_t u8_sensor)
{
  if (u8_sensor >= TEMP_SENSOR_COUNT)
    return NAN;

  sensors.requestTemperatures();

  float temp = sensors.getTempC(SensorAddress[u8_sensor]);
  if (temp == DEVICE_DISCONNECTED_C)
  {
    Serial.printf("Sensor %d disconnected!\n", u8_sensor);
    u8_ErrorCode[u8_sensor] = 1;
    return NAN;
  }

  // Store reading in circular buffer
  tempHistory[u8_sensor][tempIndex[u8_sensor]] = temp;
  tempIndex[u8_sensor] = (tempIndex[u8_sensor] + 1) % TEMP_HISTORY_LEN;
  if (tempIndex[u8_sensor] == 0)
    tempFilled[u8_sensor] = true;

  // Compute average
  uint8_t count = tempFilled[u8_sensor] ? TEMP_HISTORY_LEN : tempIndex[u8_sensor];
  float sum = 0;
  for (uint8_t i = 0; i < count; i++)
    sum += tempHistory[u8_sensor][i];

  float avg = sum / count;
  u8_ErrorCode[u8_sensor] = 0;

  return avg;
}