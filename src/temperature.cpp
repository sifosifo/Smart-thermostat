#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature.h"
#include "gui/settings.h"
#include "output.h"

OneWire oneWire(ONE_WIRE_BUS1);
DallasTemperature sensors(&oneWire);

DeviceAddress SensorAddress[TEMP_SENSOR_COUNT] = {0};

bool temp_swap_sensors_b = false;
uint8_t u8_temp_count = 0;
uint8_t u8_ErrorCode[TEMP_SENSOR_COUNT] = {0, 0};

float f_RoomTemperature = 0.0;
float f_FloorTemperature = 0.0;
static float cached_room_temperature = TEMP_SENSOR_NOT_CONNECTED;
static float cached_floor_temperature = TEMP_SENSOR_NOT_CONNECTED;
static bool temperature_read_pending = false;
float f_TempTarget[TEMP_SENSOR_COUNT] = {19, 25};

void printAddress(const DeviceAddress deviceAddress);
static int compareAddress(const DeviceAddress a, const DeviceAddress b);
static void swapAddress(DeviceAddress a, DeviceAddress b);

uint8_t temp_Init(void)
{
    bool verbose = false;

        // 1. Reset everything
    memset(SensorAddress, 0, sizeof(SensorAddress));
    memset(u8_ErrorCode,   0, sizeof(u8_ErrorCode));

    pinMode(ONE_WIRE_BUS1, INPUT_PULLUP);
    oneWire.reset();
    sensors.begin();                     // <-- this triggers a fresh search
    sensors.setWaitForConversion(false); // Conversion runs while the main loop handles LVGL.

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

uint8_t temp_Process(bool two_sensors_required, float room_temperature, float floor_temperature)
{
    bool sensors_ok = false;
    bool enable_heater = false;

    f_RoomTemperature = room_temperature;
    f_FloorTemperature = floor_temperature;
    float f_TempHysteresis = settings_hysteresis();

    if (two_sensors_required)
    {
        sensors_ok = (f_RoomTemperature != TEMP_SENSOR_NOT_CONNECTED) &&
                     (f_FloorTemperature != TEMP_SENSOR_NOT_CONNECTED);
    } else
    {
        sensors_ok = (f_RoomTemperature != TEMP_SENSOR_NOT_CONNECTED);
    }
    //Serial.printf("F:%f R:%f T:%f\n", f_FloorTemperature, f_RoomTemperature, f_TempHysteresis);
    if (!sensors_ok)
    {
        out_EnterDeadState();
        u8_temp_count = temp_Init();
        return(u8_temp_count);
    }

    if (two_sensors_required && (f_FloorTemperature > f_TempTarget[TEMP_SENSOR_FLOOR]))
    {   // floor temperature too high, turn off heating
        out_TurnOffHeatingElement();
        return(u8_temp_count);
    }
    
    if (f_RoomTemperature < (f_TempTarget[TEMP_SENSOR_ROOM] - f_TempHysteresis / 2))
        out_TurnOnHeatingElement();
    else if (f_RoomTemperature > (f_TempTarget[TEMP_SENSOR_ROOM] + f_TempHysteresis / 2))
        out_TurnOffHeatingElement();
    
    return(u8_temp_count);
}

void temp_StartTemperatureRead(void)
{
    if (temperature_read_pending) return;

    // Start one conversion for both devices without waiting for it to finish.
    sensors.requestTemperatures();
    temperature_read_pending = true;
}

bool temp_CompleteTemperatureRead(void)
{
    if (!temperature_read_pending || !sensors.isConversionComplete()) return false;

    const uint8_t room_sensor = temp_swap_sensors_b ? TEMP_SENSOR_FLOOR : TEMP_SENSOR_ROOM;
    const uint8_t floor_sensor = temp_swap_sensors_b ? TEMP_SENSOR_ROOM : TEMP_SENSOR_FLOOR;

    float room = sensors.getTempC(SensorAddress[room_sensor]);
    if (room == DEVICE_DISCONNECTED_C) {
        Serial.printf("Sensor %d disconnected!\n", room_sensor);
        u8_ErrorCode[room_sensor] = 1;
        room = TEMP_SENSOR_NOT_CONNECTED;
    } else {
        u8_ErrorCode[room_sensor] = 0;
    }

    float floor = sensors.getTempC(SensorAddress[floor_sensor]);
    if (floor == DEVICE_DISCONNECTED_C) {
        Serial.printf("Sensor %d disconnected!\n", floor_sensor);
        u8_ErrorCode[floor_sensor] = 1;
        floor = TEMP_SENSOR_NOT_CONNECTED;
    } else {
        u8_ErrorCode[floor_sensor] = 0;
    }

    cached_room_temperature = room;
    cached_floor_temperature = floor;
    temperature_read_pending = false;
    return true;
}

float temp_GetCachedTemperature(uint8_t u8_sensor)
{
    if (u8_sensor == TEMP_SENSOR_ROOM) return cached_room_temperature;
    if (u8_sensor == TEMP_SENSOR_FLOOR) return cached_floor_temperature;
    return NAN;
}

void temp_SetTemperatureTarget(uint8_t u8_sensor, float f_value)
{
    f_TempTarget[u8_sensor] = f_value;
}

float temp_GetTemperatureTarget(uint8_t u8_sensor)
{
    return(f_TempTarget[u8_sensor]);
}

// --- set swap flag ---
bool temp_swap_sensors(bool swap_sensors)
{
    temp_swap_sensors_b = swap_sensors;
    return true;
}

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
