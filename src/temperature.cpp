#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature.h"

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS1);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress SensorAddress[TEMP_SENSOR_COUNT];

uint8_t u8_temp_count = 0;

uint8_t u8_ErrorCode[TEMP_SENSOR_COUNT] = {0, 0};

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  // method 1 - slower
  //Serial.print("Temp C: ");
  //Serial.print(sensors.getTempC(deviceAddress));
  //Serial.print(" Temp F: ");
  //Serial.print(sensors.getTempF(deviceAddress)); // Makes a second call to getTempC and then converts to Fahrenheit

  // method 2 - faster
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }else
  {
//    fT1 = tempC;
  }
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
}

void temp_Init(void)
{
    pinMode(ONE_WIRE_BUS1, INPUT_PULLUP);
    while(u8_temp_count == 0)
  {
    // locate devices on the bus
    Serial.print("Locating devices...");
    sensors.begin();
    Serial.print("Found ");
    u8_temp_count = sensors.getDeviceCount();
    Serial.print(u8_temp_count, DEC);
    Serial.println(" devices.");
    delay(100);
  }
  if (!sensors.getAddress(SensorAddress[TEMP_SENSOR_FLOOR], TEMP_SENSOR_FLOOR)) Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(SensorAddress[TEMP_SENSOR_ROOM], TEMP_SENSOR_ROOM)) Serial.println("Unable to find address for Device 1");

    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(SensorAddress[TEMP_SENSOR_FLOOR], 9);
  sensors.setResolution(SensorAddress[TEMP_SENSOR_ROOM], 9);
}

float temp_GetTemperature(uint8_t u8_sensor)
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  // It responds almost immediately. Let's print out the data
  printTemperature(SensorAddress[u8_sensor]); // Use a simple function to print out the data

  return(sensors.getTempC(SensorAddress[u8_sensor]));
}