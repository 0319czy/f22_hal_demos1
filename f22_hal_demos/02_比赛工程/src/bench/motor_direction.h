#ifndef MOTOR_DIRECTION_H
#define MOTOR_DIRECTION_H
#include "closed_loop.h"
/* Explicit propeller-free diagnostic; never selected in a flight build. */
void MotorDirection_Step(ClosedLoop *c, const ClosedLoopSample *sample,
                         HoverCommand command, float dt, ClosedLoopOutput *out);
#endif
