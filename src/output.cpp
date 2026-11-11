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
static bool sensorFaultShutdownLatched = false;
static FaultCode activeFaultCode = FAULT_NONE;

static bool isAcSenseFault(FaultCode fault_code)
{
    return fault_code == FAULT_A1_OUTPUT_ACTIVE_BEFORE_ON ||
           fault_code == FAULT_A2_OUTPUT_MISSING_AFTER_ON ||
           fault_code == FAULT_A3_OUTPUT_ACTIVE_AFTER_WORK_OFF ||
           fault_code == FAULT_A4_OUTPUT_ACTIVE_AFTER_SHUTDOWN;
}

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

const char *out_GetFaultCodeText(void)
{
    switch (activeFaultCode)
    {
        case FAULT_T1_ROOM_STALE: return "T1";
        case FAULT_T2_FLOOR_STALE: return "T2";
        case FAULT_T3_BOTH_SENSORS_STALE: return "T3";
        case FAULT_A1_OUTPUT_ACTIVE_BEFORE_ON: return "A1";
        case FAULT_A2_OUTPUT_MISSING_AFTER_ON: return "A2";
        case FAULT_A3_OUTPUT_ACTIVE_AFTER_WORK_OFF: return "A3";
        case FAULT_A4_OUTPUT_ACTIVE_AFTER_SHUTDOWN: return "A4";
        default: return "?";
    }
}

void out_EnterDeadState(FaultCode fault_code)
{
    // A sense failure during shutdown takes display priority over the original
    // sensor fault because it means output power may still be present.
    const bool ac_sense_fault = isAcSenseFault(fault_code);
    const bool current_fault_is_ac = isAcSenseFault(activeFaultCode);
    if (activeFaultCode == FAULT_NONE || ac_sense_fault || !current_fault_is_ac)
        activeFaultCode = fault_code;

    SAFETY_RELAY_OFF;
    WORK_RELAY_OFF;
    safetyRelayState = false;
    workRelayState = false;
    sequenceState = DEAD;  // Enter dead state
    Serial.printf("ERROR %s: entering DEAD state.\n", out_GetFaultCodeText());
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
    if (!sensorFaultShutdownLatched && sequenceState == IDLE && !safetyRelayState)
    {
        sequenceState = TURNING_ON_SAFETY;
    }
}

// Function to trigger the OFF sequence
void out_TurnOffHeatingElement()
{
    switch (sequenceState)
    {
        case IDLE:
        case TURNING_ON_SAFETY:
        case TURNING_ON_WORK:
        case VERIFY_ON:
            if (safetyRelayState || workRelayState || sequenceState != IDLE)
                sequenceState = TURNING_OFF_WORK;
            break;
        default:
            break;
    }
}

// Latch a sensor fault and shut the relays down in their normal staged order.
void out_RequestSafeShutdown(FaultCode fault_code)
{
    sensorFaultShutdownLatched = true;
    if (activeFaultCode == FAULT_NONE) activeFaultCode = fault_code;
    if (sequenceState == DEAD) return;

    if (sequenceState == TURNING_OFF_WORK || sequenceState == TURNING_OFF_SAFETY ||
        sequenceState == VERIFY_OFF) return;

    if (safetyRelayState || workRelayState) sequenceState = TURNING_OFF_WORK;
    else sequenceState = VERIFY_OFF;
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
                    out_EnterDeadState(FAULT_A1_OUTPUT_ACTIVE_BEFORE_ON);  // Output unexpectedly active before enabling work relay.
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
                    out_EnterDeadState(FAULT_A2_OUTPUT_MISSING_AFTER_ON);  // Output did not activate.
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
                    out_EnterDeadState(FAULT_A3_OUTPUT_ACTIVE_AFTER_WORK_OFF);  // Output remained active after work relay opened.
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
                sequenceState = sensorFaultShutdownLatched ? DEAD : IDLE;
            } else {
                // If the output is not off, something failed
                if(ACsenseEnabled || sensorFaultShutdownLatched)
                {
                    out_EnterDeadState(FAULT_A4_OUTPUT_ACTIVE_AFTER_SHUTDOWN);  // Output remained active after shutdown.
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
