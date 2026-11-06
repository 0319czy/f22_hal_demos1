#ifndef AIRCRAFT1_PWM_H
#define AIRCRAFT1_PWM_H
#include <stdint.h>
void Aircraft1Pwm_Init(void);
void Aircraft1Pwm_Trip(void);
void Aircraft1Pwm_Clear(void);
unsigned Aircraft1Pwm_Tripped(void);
unsigned Aircraft1Pwm_Apply(const float motor[4], uint32_t now);
#endif
