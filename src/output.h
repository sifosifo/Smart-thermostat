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

extern bool ACsenseEnabled;

void out_Init(void);
void out_BL_init(void);
void out_TurnOnHeatingElement(void);
void out_TurnOffHeatingElement(void);
bool out_Get(void);
bool out_get_saf_relay(void);
bool out_get_work_relay(void);
SequenceState out_ControlRelays(void);
void out_EnterDeadState(void);
bool measureOutput(void);
void AdjustLCDBrightness(void);

#endif

