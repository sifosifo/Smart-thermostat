#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED
#include <stdint.h>

#define SAFETY_RELAY_PIN    16
#define WORK_RELAY_PIN      4
#define AC_SENSE_PIN        17

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

void out_Init(void);
void out_TurnOnHeatingElement(void);
void out_TurnOffHeatingElement(void);
bool out_Get(void);
SequenceState out_ControlRelays(void);
void out_EnterDeadState(void);

#endif

