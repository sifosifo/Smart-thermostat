#include "output.h"
#include <OneWire.h>
#include <DallasTemperature.h>

OutputState CurrentOutputState = OFF;
uint8_t u8_Time = 0;

void out_Init(void)
{
    pinMode(OUT_1, OUTPUT);
    digitalWrite(OUT_1, LOW);
    pinMode(OUT_2, OUTPUT);
    digitalWrite(OUT_2, LOW);
}

void out_Set(uint8_t u8_RequestedState)
{
    if(CurrentOutputState != OFF_LOCKED)    // If locked, do nothing
    {
        if(u8_RequestedState == OFF)        // If requested OFF, go OFF imediately
        {
            CurrentOutputState = OFF;
            digitalWrite(OUT_1, LOW);            
            digitalWrite(OUT_2, LOW);
        }else
        {
            if(CurrentOutputState == OFF)   // Listen to ON request only if in OFF state
            {
                CurrentOutputState = SET1ON;
            }
        }
    }
}

OutputState out_Tick(void)
{
    Serial.println("out_Tick");
    if((CurrentOutputState != OFF) && (CurrentOutputState != ON) && (CurrentOutputState != OFF_LOCKED)) // If not in stable state
    {
        if(CurrentOutputState == SET1ON)
        {
            Serial.println("0");
            CurrentOutputState = ON1_WAIT;
        }else if(CurrentOutputState == ON1_WAIT)   // Make first part of test
        {
            Serial.println("1");
            if(1)
            {
                Serial.println("2");
                CurrentOutputState = SET2ON;
                digitalWrite(OUT_1, HIGH);            
            }else
            {
                Serial.println("3");
                CurrentOutputState = OFF_LOCKED;
            }
        }else if(CurrentOutputState == SET2ON)
        {
            Serial.println("4");
            CurrentOutputState = ON2_WAIT;
        }else if(CurrentOutputState == ON2_WAIT)   // Make second part of test
        {
            Serial.println("5");
            if(1)
            {
                Serial.println("6");
                CurrentOutputState = ON;
                digitalWrite(OUT_2, HIGH);
            }else
            {
                Serial.println("7");
                CurrentOutputState = OFF_LOCKED;
            }
        }
    }
    return(CurrentOutputState);
}