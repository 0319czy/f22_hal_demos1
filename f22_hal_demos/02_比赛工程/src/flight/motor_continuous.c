#include "motor_continuous.h"
void MotorContinuous_Init(MotorContinuous *c) { c->state = CL_IDLE; }
void MotorContinuous_Step(MotorContinuous *c, HoverCommand command,
                          unsigned output_tripped, float motor[4])
{
    for (unsigned i = 0; i < 4; ++i) motor[i] = 0;
    if (command == HOVER_STOP || command == HOVER_REQUEST_LAND) {
        c->state = CL_IDLE;
        return;
    }
    if (output_tripped) c->state = CL_FAULT;
    if (c->state == CL_FAULT) return;
    if (command == HOVER_START && c->state == CL_IDLE) c->state = CL_HOLD;
    if (c->state == CL_HOLD)
        for (unsigned i = 0; i < 4; ++i) motor[i] = MOTOR_CONTINUOUS_DUTY;
}
