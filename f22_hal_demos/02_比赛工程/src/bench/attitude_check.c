#include "attitude_check.h"
#include "closed_loop_profile.h"
#include <math.h>
#include <string.h>
#ifndef CL_ATTITUDE_CHECK
#error "Attitude bench requires its explicit low-power build profile"
#endif
static void Reset(ClosedLoop *c)
{
    c->collective = c->seconds = 0;
    for (unsigned k = 0U; k < 3U; ++k) FlightPid_Reset(&c->rate[k]);
    FlightPid_Reset(&c->vertical);
}
static void Fail(ClosedLoop *c, ClosedLoopFault fault)
{ Reset(c); c->state = CL_FAULT; c->fault = fault; }
void AttitudeCheck_Step(ClosedLoop *c, const ClosedLoopSample *s,
    HoverCommand command, float dt, ClosedLoopOutput *out)
{
    memset(out, 0, sizeof(*out));
    if (command == HOVER_STOP || command == HOVER_REQUEST_LAND) {
        Reset(c); c->state = CL_IDLE; c->fault = CL_OK; goto finish;
    }
    if (c->state == CL_IDLE && command != HOVER_START) goto finish;
    if (c->state == CL_FAULT) goto finish;
    if (!isfinite(c->config.hover_duty) || !isfinite(c->config.max_duty) ||
        c->config.hover_duty <= 0 || c->config.hover_duty > c->config.max_duty ||
        c->config.max_duty > CL_MAX_DUTY || c->config.use_flow) {
        Fail(c, CL_INPUT_INVALID); goto finish;
    }
    if (!c->config.imu_axes_verified) { Fail(c, CL_AXES_UNVERIFIED); goto finish; }
    if (s->output_fault) { Fail(c, CL_OUTPUT_FAULT); goto finish; }
    if (!isfinite(dt) || dt <= 0 || dt > .015f) { Fail(c, CL_TIMING); goto finish; }
    if (!s->link_valid) { Fail(c, CL_LINK_LOST); goto finish; }
    if (!s->imu_valid || !isfinite(s->roll) || !isfinite(s->pitch)) {
        Fail(c, CL_IMU_INVALID); goto finish;
    }
    for (unsigned k = 0U; k < 3U; ++k) if (!isfinite(s->gyro[k])) {
        Fail(c, CL_IMU_INVALID); goto finish;
    }
    if (fabsf(s->roll) >= 25 || fabsf(s->pitch) >= 25) { Fail(c, CL_TILT); goto finish; }
    if (c->state == CL_IDLE) {
        if (fabsf(s->roll) > 8 || fabsf(s->pitch) > 8 ||
            fabsf(s->gyro[0]) > 10 || fabsf(s->gyro[1]) > 10 || fabsf(s->gyro[2]) > 10) {
            Fail(c, CL_START_TOO_HIGH); goto finish;
        }
        Reset(c); c->state = CL_HOLD;
    }
    c->collective = fminf(c->config.hover_duty,
        c->collective + CL_LAUNCH_RAMP_DUTY_PER_SEC * dt);
    /* Reuse the flight attitude/rate gains and mixer with zero angle targets.
     * No ToF preconditions, fake height, takeoff ramp or height integrator. */
    if (!ClosedLoop_AttitudeMix(c, s, 0, 0, dt, out)) {
        Fail(c, CL_INPUT_INVALID); memset(out, 0, sizeof(*out));
    }
finish:
    out->state = c->state; out->fault = c->fault;
}
