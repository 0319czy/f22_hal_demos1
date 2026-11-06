#include "motor_direction.h"
#include "closed_loop_profile.h"
#include <math.h>
#include <string.h>
#ifndef CL_DIRECTION_CHECK
#error "Motor direction diagnostic requires its explicit build profile"
#endif
void MotorDirection_Step(ClosedLoop *c, const ClosedLoopSample *s,
                         HoverCommand command, float dt, ClosedLoopOutput *out)
{
    memset(out, 0, sizeof(*out));
    if (command == HOVER_STOP || command == HOVER_REQUEST_LAND) {
        c->state = CL_IDLE; c->fault = CL_OK; c->collective = 0;
    } else if (c->state != CL_FAULT && (c->state != CL_IDLE || command == HOVER_START)) {
        ClosedLoopFault fault = s->output_fault ? CL_OUTPUT_FAULT :
            !s->link_valid ? CL_LINK_LOST :
            (!isfinite(dt) || dt <= 0 || dt > .015f) ? CL_TIMING : CL_OK;
        if (fault != CL_OK) {
            c->state = CL_FAULT; c->fault = fault; c->collective = 0;
        } else {
            /* No attitude/height feedback, starting kick or automatic power rise. */
            c->state = CL_HOLD; c->collective = CL_HOVER_DUTY;
            for (unsigned i = 0; i < 4; ++i) out->motor[i] = CL_HOVER_DUTY;
        }
    }
    out->state = c->state; out->fault = c->fault; out->collective = c->collective;
}
