#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED
#include <stdint.h>

enum OutputState
{
    OFF = 0,
    SET1ON,
    ON1_WAIT,
    SET2ON,
    ON2_WAIT,
    ON,
    OFF_LOCKED
};

void out_Init(void);
void out_Set(uint8_t u8_State);
OutputState out_Tick(void);

#endif