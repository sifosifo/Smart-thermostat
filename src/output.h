#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED
#include <stdint.h>

#define SAFETY_RELAY_PIN    16
#define WORK_RELAY_PIN      4
#define AC_SENSE_PIN        21
#define LCD_BL_PIN          27
#define PHOTORESISTOR_PIN   34

enum SequenceState {
    IDLE,
    TURNING_ON_SAFETY,
    TURNING_ON_WORK,
    VERIFY_ON,
    TURNING_OFF_WORK,
    TURNING_OFF_SAFETY,
    VERIFY_OFF,
    DEAD  // Error state
};

enum FaultCode : uint8_t {
    FAULT_NONE = 0,
    FAULT_T1_ROOM_STALE,                  // Room sensor has no valid update for 10 s.
    FAULT_T2_FLOOR_STALE,                 // Required floor sensor has no valid update for 10 s.
    FAULT_T3_BOTH_SENSORS_STALE,          // Both required sensors have no valid update for 10 s.
    FAULT_A1_OUTPUT_ACTIVE_BEFORE_ON,     // AC sense is active before the work relay turns on.
    FAULT_A2_OUTPUT_MISSING_AFTER_ON,     // AC sense is inactive after the work relay turns on.
    FAULT_A3_OUTPUT_ACTIVE_AFTER_WORK_OFF,// AC sense remains active after the work relay opens.
    FAULT_A4_OUTPUT_ACTIVE_AFTER_SHUTDOWN // AC sense remains active after the shutdown sequence.
};

extern bool ACsenseEnabled;

void out_Init(void);
void out_BL_init(void);
void out_TurnOnHeatingElement(void);
void out_TurnOffHeatingElement(void);
void out_RequestSafeShutdown(FaultCode fault_code);
bool out_Get(void);
bool out_get_saf_relay(void);
bool out_get_work_relay(void);
SequenceState out_ControlRelays(void);
void out_EnterDeadState(FaultCode fault_code);
const char *out_GetFaultCodeText(void);
bool measureOutput(void);
void AdjustLCDBrightness(void);

#endif

