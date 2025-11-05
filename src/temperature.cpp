#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature.h"

OneWire oneWire(ONE_WIRE_BUS1);
DallasTemperature sensors(&oneWire);

DeviceAddress SensorAddress[TEMP_SENSOR_COUNT] = {0};

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
    bool verbose = false;

        // 1. Reset everything
    memset(SensorAddress, 0, sizeof(SensorAddress));
    memset(u8_ErrorCode,   0, sizeof(u8_ErrorCode));

    pinMode(ONE_WIRE_BUS1, INPUT_PULLUP);
    oneWire.reset();
    sensors.begin();                     // <-- this triggers a fresh search

    u8_temp_count = sensors.getDeviceCount();

    if (verbose) {
        Serial.println("Locating devices...");
        Serial.printf("Found %d device(s).\n", u8_temp_count);
    }

    if (u8_temp_count == 0) {
        if (verbose) Serial.println("No OneWire devices found!");
        return 0;
    }

    // 2. Read addresses + set resolution
    for (uint8_t i = 0; i < u8_temp_count && i < TEMP_SENSOR_COUNT; ++i) {
        if (sensors.getAddress(SensorAddress[i], i)) {
            if (verbose) {
                Serial.printf("Sensor %d address: ", i);
                printAddress(SensorAddress[i]);
                Serial.println();
            }
            sensors.setResolution(SensorAddress[i], 12);
        } else {
            if (verbose) Serial.printf("Unable to get address for device %d\n", i);
            u8_ErrorCode[i] = 1;
        }
    }

    // 3. Sort by address (only when we have >=2)
    if (u8_temp_count >= 2 && compareAddress(SensorAddress[0], SensorAddress[1]) > 0) {
        if (verbose) Serial.println("Sorting sensors by address (swap).");
        swapAddress(SensorAddress[0], SensorAddress[1]);
    }

    // 4. Optional final dump
    if (verbose) {
        Serial.println("Final sensor order:");
        for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; ++i) {
            Serial.printf("Sensor %d -> ", i);
            printAddress(SensorAddress[i]);
            Serial.println();
        }
    }

    return u8_temp_count;
}

// --- get latest temperature ---
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

    u8_ErrorCode[u8_sensor] = 0;
    return temp;
}

// --- set swap flag ---
bool temp_swap_sensors(bool swap_sensors)
{
    temp_swap_sensors_b = swap_sensors;
    return true;
}
