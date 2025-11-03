#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature.h"

OneWire oneWire(ONE_WIRE_BUS1);
DallasTemperature sensors(&oneWire);

DeviceAddress SensorAddress[TEMP_SENSOR_COUNT];

#define TEMP_HISTORY_LEN 16

float tempHistory[TEMP_SENSOR_COUNT][TEMP_HISTORY_LEN];
uint8_t tempIndex[TEMP_SENSOR_COUNT] = {0};
bool tempFilled[TEMP_SENSOR_COUNT] = {false};
bool temp_swap_sensors_b = false;

uint8_t u8_temp_count = 0;
uint8_t u8_ErrorCode[TEMP_SENSOR_COUNT] = {0, 0};

// --- helper to print addresses ---
void printAddress(const DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// --- helper to compare addresses ---
static int compareAddress(const DeviceAddress a, const DeviceAddress b)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

// --- helper to swap addresses ---
static void swapAddress(DeviceAddress a, DeviceAddress b)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    uint8_t tmp = a[i];
    a[i] = b[i];
    b[i] = tmp;
  }
}

uint8_t temp_Init(void)
{
  pinMode(ONE_WIRE_BUS1, INPUT_PULLUP);
  sensors.begin();

  Serial.println("Locating devices...");
  u8_temp_count = sensors.getDeviceCount();
  Serial.printf("Found %d devices.\n", u8_temp_count);

  if (u8_temp_count < 2)
  {
    Serial.println("❌ Less than 2 OneWire devices found!");
    return u8_temp_count;
  }

  // Get addresses
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    if (sensors.getAddress(SensorAddress[i], i))
    {
      Serial.printf("Sensor %d address: ", i);
      printAddress(SensorAddress[i]);
      Serial.println();
      sensors.setResolution(SensorAddress[i], 9);
    }
    else
    {
      Serial.printf("⚠️ Unable to find address for sensor %d\n", i);
      u8_ErrorCode[i] = 1;
    }
  }

  // --- Sort sensors by serial number ---
  if (compareAddress(SensorAddress[0], SensorAddress[1]) > 0)
  {
    Serial.println("Sorting sensors by address (swapping order).");
    swapAddress(SensorAddress[0], SensorAddress[1]);
  }

  Serial.println("Final sensor order:");
  for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
  {
    Serial.printf("Sensor %d -> ", i);
    printAddress(SensorAddress[i]);
    Serial.println();
  }

  return u8_temp_count;
}

// --- get temperature ---
float temp_GetTemperature(uint8_t u8_sensor)
{
  if (u8_sensor >= TEMP_SENSOR_COUNT) return NAN;

  // Apply swap flag
  if (temp_swap_sensors_b)
    u8_sensor ^= 1;  // 0↔1 toggle

  sensors.requestTemperatures();

  float temp = sensors.getTempC(SensorAddress[u8_sensor]);
  if (temp == DEVICE_DISCONNECTED_C)
  {
    Serial.printf("Sensor %d disconnected!\n", u8_sensor);
    u8_ErrorCode[u8_sensor] = 1;
    return NAN;
  }

  // Add to circular buffer for smoothing
  tempHistory[u8_sensor][tempIndex[u8_sensor]] = temp;
  tempIndex[u8_sensor] = (tempIndex[u8_sensor] + 1) % TEMP_HISTORY_LEN;
  if (tempIndex[u8_sensor] == 0)
    tempFilled[u8_sensor] = true;

  // Compute average
  uint8_t count = tempFilled[u8_sensor] ? TEMP_HISTORY_LEN : tempIndex[u8_sensor];
  float sum = 0;
  for (uint8_t i = 0; i < count; i++)
    sum += tempHistory[u8_sensor][i];

  u8_ErrorCode[u8_sensor] = 0;
  return sum / count;
}

// --- set swap flag ---
bool temp_swap_sensors(bool swap_sensors)
{
  temp_swap_sensors_b = swap_sensors;
  //Serial.printf("Sensor swap %s\n", swap_sensors ? "ENABLED" : "DISABLED");
  return true;
}
