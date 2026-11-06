#ifndef MOTOR_CONTINUOUS_H
#define MOTOR_CONTINUOUS_H
#include "closed_loop.h"
#ifndef MOTOR_CONTINUOUS_DUTY
#define MOTOR_CONTINUOUS_DUTY .15f
#endif
typedef struct { ClosedLoopState state; } MotorContinuous;
void MotorContinuous_Init(MotorContinuous *control);
void MotorContinuous_Step(MotorContinuous *control, HoverCommand command,
                          unsigned output_tripped, float motor[4]);
#endif
