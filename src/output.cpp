#include "output.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "gui/settings.h"

#define SAFETY_RELAY_ON digitalWrite(SAFETY_RELAY_PIN, HIGH)
#define SAFETY_RELAY_OFF digitalWrite(SAFETY_RELAY_PIN, LOW)
#define WORK_RELAY_ON digitalWrite(WORK_RELAY_PIN, HIGH)
#define WORK_RELAY_OFF digitalWrite(WORK_RELAY_PIN, LOW)

#define PWM_CHANNEL   0     // LEDC channel (0-15)
#define PWM_FREQ      1000  // 5kHz (good for LED)
#define PWM_RESOLUTION 8    // 8-bit: 0–255

SequenceState sequenceState = IDLE;
uint8_t u8_Time = 0;
// State of relays (0 = off, 1 = on)
bool safetyRelayState = false;
bool workRelayState = false;
bool OutputState = false;
bool ACsenseEnabled = false;

void out_Init(void)
{
    pinMode(SAFETY_RELAY_PIN, OUTPUT);
    SAFETY_RELAY_OFF;
    pinMode(WORK_RELAY_PIN, OUTPUT);
    WORK_RELAY_OFF;
    pinMode(AC_SENSE_PIN, INPUT);
    gpio_pulldown_dis((gpio_num_t)AC_SENSE_PIN);
    gpio_pullup_dis((gpio_num_t)AC_SENSE_PIN);
   
    analogReadResolution(12);  // 12-bit (0-4095)
}

void out_BL_init(void)
{
       // Setup PWM for backlight
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(LCD_BL_PIN, PWM_CHANNEL);

    ledcWrite(PWM_CHANNEL, 255);
    ledcWrite(PWM_CHANNEL, 0);
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

// Returns inverted value of input, because we want it to be true, when AC voltage is present.
// AC voltage activated 100 times a second optocoupler, which connects 100 times a second
// ground to C1.
// If no AC is present, C1 is charged to 3,3V using resistor R3
// NO AC => 3,3V => false
// AC => 0V => true
bool measureOutput()
{
    if(!digitalRead(AC_SENSE_PIN))
    {
        //Serial.println("ACsense true");
        return(true);
    }else
    {
        //Serial.println("ACsense false");
        return(false);
    }
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

bool out_get_saf_relay()
{
    return(safetyRelayState);
}

bool out_get_work_relay()
{
    return(workRelayState);
}

bool out_Get()
{
    return(safetyRelayState & workRelayState);
}

SequenceState out_ControlRelays()
{
    ACsenseEnabled = settings_require_ac_sense();
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
                if(ACsenseEnabled)
                {
                    out_EnterDeadState();  // Enter dead state
                }else
                {
                    WORK_RELAY_ON;  // Turn on work relay if output is still off
                    workRelayState = true;
                    sequenceState = VERIFY_ON;
                }
            }
            break;

        case VERIFY_ON:
            if (measureOutput()) {
                // Heating element is on, done with the ON sequence
                sequenceState = IDLE;
            } else {
                // If the output is not on, something failed
                if(ACsenseEnabled)
                {
                    out_EnterDeadState();  // Enter dead state
                }else
                {
                    sequenceState = IDLE;
                }
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
                if(ACsenseEnabled)
                {
                    out_EnterDeadState();  // Enter dead state
                }else
                {
                    SAFETY_RELAY_OFF;  // Turn off safety relay if output is still off
                    safetyRelayState = false;
                    sequenceState = VERIFY_OFF;
                }
            }
            break;

        case VERIFY_OFF:
            if (!measureOutput()) {
                // Heating element is off, done with the OFF sequence
                sequenceState = IDLE;
            } else {
                // If the output is not off, something failed
                if(ACsenseEnabled)
                {
                    out_EnterDeadState();  // Enter dead state
                }else
                {
                    sequenceState = IDLE;
                }
            }
            break;

        case DEAD:
            // System in failure mode, waiting for external reset
            // Optionally: implement reset logic here if needed
            break;
    }
    return(sequenceState);
}

void AdjustLCDBrightness()
{
    uint16_t pwm = 255;
    uint8_t MaxBrightness = 128u;

    MaxBrightness = settings_max_brightness();

    if(settings_auto_brightness())
    {
        int adc = analogRead(PHOTORESISTOR_PIN);  // Read raw ADC
        if(adc > 1500) adc = 1500;
        pwm = MaxBrightness - (adc * (MaxBrightness-1L)) / 1500;  
        
        //Serial.printf("ADC: %d   PWM: %d\n", adc, pwm);
    }else
    {
        pwm = MaxBrightness;
    }

    ledcWrite(PWM_CHANNEL, (pwm > MaxBrightness) ? MaxBrightness : (pwm == 0) ? 1 : (uint8_t)pwm); 
}