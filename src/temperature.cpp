#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
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
static bool room_has_valid_reading = false;
static bool floor_has_valid_reading = false;
static uint32_t room_last_valid_ms = 0;
static uint32_t floor_last_valid_ms = 0;
static uint32_t temperature_monitor_started_ms = 0;
static bool stale_sensor_fault_reported = false;
static constexpr uint32_t TEMPERATURE_STALE_TIMEOUT_MS = 10000;
float f_TempTarget[TEMP_SENSOR_COUNT] = {19, 25};

void printAddress(const DeviceAddress deviceAddress);
static int compareAddress(const DeviceAddress a, const DeviceAddress b);
static void swapAddress(DeviceAddress a, DeviceAddress b);

static bool isValidTemperature(float temperature)
{
    return isfinite(temperature) && temperature != DEVICE_DISCONNECTED_C &&
           temperature >= -55.0f && temperature <= 125.0f;
}

static bool readingIsFresh(bool has_reading, uint32_t last_valid_ms, uint32_t now)
{
    return has_reading && now - last_valid_ms < TEMPERATURE_STALE_TIMEOUT_MS;
}

static bool readingHasExpired(bool has_reading, uint32_t last_valid_ms, uint32_t now)
{
    const uint32_t reference_ms = has_reading ? last_valid_ms : temperature_monitor_started_ms;
    return now - reference_ms >= TEMPERATURE_STALE_TIMEOUT_MS;
}

static bool requiredReadingsAreFresh(uint32_t now)
{
    if (!readingIsFresh(room_has_valid_reading, room_last_valid_ms, now)) return false;
    if (settings_require_two_sensors() &&
        !readingIsFresh(floor_has_valid_reading, floor_last_valid_ms, now)) return false;
    return true;
}

static bool requiredReadingHasExpired(uint32_t now)
{
    if (readingHasExpired(room_has_valid_reading, room_last_valid_ms, now)) return true;
    if (settings_require_two_sensors() &&
        readingHasExpired(floor_has_valid_reading, floor_last_valid_ms, now)) return true;
    return false;
}

uint8_t temp_Init(void)
{
    bool verbose = false;
    temperature_monitor_started_ms = millis();

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
    if (stale_sensor_fault_reported) return u8_temp_count;

    bool sensors_ok = false;

    f_RoomTemperature = room_temperature;
    f_FloorTemperature = floor_temperature;
    float f_TempHysteresis = settings_hysteresis();

    const bool room_sensor_ok = isValidTemperature(f_RoomTemperature);
    const bool floor_sensor_ok = isValidTemperature(f_FloorTemperature);

    if (two_sensors_required)
    {
        sensors_ok = room_sensor_ok && floor_sensor_ok;
    } else
    {
        sensors_ok = room_sensor_ok;
    }
    //Serial.printf("F:%f R:%f T:%f\n", f_FloorTemperature, f_RoomTemperature, f_TempHysteresis);
    if (!sensors_ok)
    {
        stale_sensor_fault_reported = true;
        const FaultCode fault_code = !room_sensor_ok && two_sensors_required && !floor_sensor_ok ?
                                     FAULT_T3_BOTH_SENSORS_STALE :
                                     (!room_sensor_ok ? FAULT_T1_ROOM_STALE : FAULT_T2_FLOOR_STALE);
        out_RequestSafeShutdown(fault_code);
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
    if (temperature_read_pending || stale_sensor_fault_reported) return;

    // Start one conversion for both devices without waiting for it to finish.
    sensors.requestTemperatures();
    temperature_read_pending = true;
}

bool temp_CompleteTemperatureRead(void)
{
    // Keep the 2 s relay state machine running so a latched sensor fault can
    // complete the normal staged shutdown sequence.
    if (stale_sensor_fault_reported) return true;

    const uint32_t now = millis();
    bool conversion_completed = false;
    if (temperature_read_pending && sensors.isConversionComplete()) {
        temperature_read_pending = false;
        conversion_completed = true;

        const uint8_t room_sensor = temp_swap_sensors_b ? TEMP_SENSOR_FLOOR : TEMP_SENSOR_ROOM;
        const uint8_t floor_sensor = temp_swap_sensors_b ? TEMP_SENSOR_ROOM : TEMP_SENSOR_FLOOR;

        const float room = sensors.getTempC(SensorAddress[room_sensor]);
        if (isValidTemperature(room)) {
            cached_room_temperature = room;
            room_last_valid_ms = now;
            room_has_valid_reading = true;
            u8_ErrorCode[room_sensor] = 0;
        } else {
            u8_ErrorCode[room_sensor] = 1;
        }

        const float floor = sensors.getTempC(SensorAddress[floor_sensor]);
        if (isValidTemperature(floor)) {
            cached_floor_temperature = floor;
            floor_last_valid_ms = now;
            floor_has_valid_reading = true;
            u8_ErrorCode[floor_sensor] = 0;
        } else {
            u8_ErrorCode[floor_sensor] = 1;
        }
    }

    if (requiredReadingsAreFresh(now)) return conversion_completed;
    if (temp_CheckTemperatureFreshness()) return true;

    return false;
}

bool temp_CheckTemperatureFreshness(void)
{
    if (stale_sensor_fault_reported) return false;

    const uint32_t now = millis();
    if (!requiredReadingHasExpired(now)) return false;

    const bool room_expired = readingHasExpired(room_has_valid_reading, room_last_valid_ms, now);
    const bool floor_expired = settings_require_two_sensors() &&
                               readingHasExpired(floor_has_valid_reading, floor_last_valid_ms, now);
    const FaultCode fault_code = room_expired && floor_expired ? FAULT_T3_BOTH_SENSORS_STALE :
                                 (room_expired ? FAULT_T1_ROOM_STALE : FAULT_T2_FLOOR_STALE);

    Serial.printf("ERROR %s: no valid temperature update for 10 seconds.\n",
                  fault_code == FAULT_T1_ROOM_STALE ? "T1" :
                  fault_code == FAULT_T2_FLOOR_STALE ? "T2" : "T3");
    if (room_expired)
        cached_room_temperature = TEMP_SENSOR_NOT_CONNECTED;
    if (floor_expired)
        cached_floor_temperature = TEMP_SENSOR_NOT_CONNECTED;
    stale_sensor_fault_reported = true;
    temperature_read_pending = false;
    out_RequestSafeShutdown(fault_code);
    return true;
}

float temp_GetCachedTemperature(uint8_t u8_sensor)
{
    const uint32_t now = millis();
    if (u8_sensor == TEMP_SENSOR_ROOM)
        return readingIsFresh(room_has_valid_reading, room_last_valid_ms, now) ?
               cached_room_temperature : TEMP_SENSOR_NOT_CONNECTED;
    if (u8_sensor == TEMP_SENSOR_FLOOR)
        return readingIsFresh(floor_has_valid_reading, floor_last_valid_ms, now) ?
               cached_floor_temperature : TEMP_SENSOR_NOT_CONNECTED;
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
