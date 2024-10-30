#include "output.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define SAFETY_RELAY_ON digitalWrite(SAFETY_RELAY_PIN, HIGH)
#define SAFETY_RELAY_OFF digitalWrite(SAFETY_RELAY_PIN, LOW)
#define WORK_RELAY_ON digitalWrite(WORK_RELAY_PIN, HIGH)
#define WORK_RELAY_OFF digitalWrite(WORK_RELAY_PIN, LOW)

SequenceState sequenceState = IDLE;
uint8_t u8_Time = 0;
// State of relays (0 = off, 1 = on)
bool safetyRelayState = false;
bool workRelayState = false;
bool OutputState = false;

void out_Init(void)
{
    pinMode(SAFETY_RELAY_PIN, OUTPUT);
    SAFETY_RELAY_OFF;
    pinMode(WORK_RELAY_PIN, OUTPUT);
    WORK_RELAY_OFF;
}

void out_EnterDeadState()
{
    SAFETY_RELAY_OFF;
    WORK_RELAY_OFF;
    safetyRelayState = false;
    workRelayState = false;
    sequenceState = DEAD;  // Enter dead state
    Serial.println("ERROR: Failure detected, entering DEAD state.");
}

// Function to measure the output (stub - implement based on your sensor)
bool measureOutput()
{
    // Read the sensor pin or measure the actual output state here
    // return true if the heating element is on, false if off
    return digitalRead(AC_SENSE_PIN);
}

// Function to trigger the ON sequence
void out_TurnOnHeatingElement()
{
    if ((sequenceState == IDLE) & (safetyRelayState == false))
    {
        sequenceState = TURNING_ON_SAFETY;
    }
}

// Function to trigger the OFF sequence
void out_TurnOffHeatingElement()
{
    if ((sequenceState == IDLE) & (safetyRelayState == true))
    {
        sequenceState = TURNING_OFF_WORK;
    }
}

bool out_Get()
{
    return(safetyRelayState & workRelayState);
}

SequenceState out_ControlRelays()
{      
    switch (sequenceState)
    {
        case IDLE:
            // Waiting for a command to turn on or off
            break;

        case TURNING_ON_SAFETY:
            SAFETY_RELAY_ON;  // Turn on safety relay
            safetyRelayState = true;
            sequenceState = TURNING_ON_WORK;
            break;

        case TURNING_ON_WORK:
            // Measure output before turning on the work relay
            if (!measureOutput()) {
                WORK_RELAY_ON;  // Turn on work relay if output is still off
                workRelayState = true;
                sequenceState = VERIFY_ON;
            } else {
                // If the output is already on before turning on the work relay, something went wrong
                /////////////////////////////////////////////////////////////////////////////// no measurement
                //out_EnterDeadState();  // Enter dead state
                WORK_RELAY_ON;  // Turn on work relay if output is still off
                workRelayState = true;
                sequenceState = VERIFY_ON;
            }
            break;

        case VERIFY_ON:
            if (measureOutput()) {
                // Heating element is on, done with the ON sequence
                sequenceState = IDLE;
            } else {
                // If the output is not on, something failed
                /////////////////////////////////////////////////////////////////////////////// no measurement
                //out_EnterDeadState();  // Enter dead state
                sequenceState = IDLE;
            }
            break;

        case TURNING_OFF_WORK:
            WORK_RELAY_OFF;  // Turn off work relay
            workRelayState = false;
            sequenceState = TURNING_OFF_SAFETY;
            break;

        case TURNING_OFF_SAFETY:
            // Measure output before turning off the safety relay
            if (!measureOutput()) {
                SAFETY_RELAY_OFF;  // Turn off safety relay if output is still off
                safetyRelayState = false;
                sequenceState = VERIFY_OFF;
            } else {
                // If the output is still on before turning off the safety relay, failure detected
                /////////////////////////////////////////////////////////////////////////////// no measurement
                //out_EnterDeadState();  // Enter dead state
                SAFETY_RELAY_OFF;  // Turn off safety relay if output is still off
                safetyRelayState = false;
                sequenceState = VERIFY_OFF;
            }
            break;

        case VERIFY_OFF:
            if (!measureOutput()) {
                // Heating element is off, done with the OFF sequence
                sequenceState = IDLE;
            } else {
                // If the output is not off, something failed
                /////////////////////////////////////////////////////////////////////////////// no measurement
                //out_EnterDeadState();  // Enter dead state
                sequenceState = IDLE;
            }
            break;

        case DEAD:
            // System in failure mode, waiting for external reset
            // Optionally: implement reset logic here if needed
            break;
    }
    return(sequenceState);
}