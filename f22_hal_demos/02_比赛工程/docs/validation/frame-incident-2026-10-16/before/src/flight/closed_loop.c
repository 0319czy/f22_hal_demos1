#include "closed_loop.h"
#include "closed_loop_config.h"
#include "aircraft1.h"
#include <math.h>
#include <string.h>
static float Clamp(float v, float low, float high)
{ return fminf(high, fmaxf(low, v)); }
static void ResetPids(ClosedLoop *c)
{
    for (unsigned i = 0; i < 3; ++i) FlightPid_Reset(&c->rate[i]);
    FlightPid_Reset(&c->vertical);
}
static void Fail(ClosedLoop *c, ClosedLoopFault fault)
{ c->state = CL_FAULT; c->fault = fault; c->collective = 0; ResetPids(c); }
void ClosedLoop_Init(ClosedLoop *c, ClosedLoopConfig cfg)
{
    memset(c, 0, sizeof(*c)); c->config = cfg;
    c->height_feedforward = cfg.hover_duty;
    for (unsigned i = 0; i < 3; ++i) {
        c->rate[i].kp = .0015f; c->rate[i].ki = .0003f;
        c->rate[i].limit = .04f; c->rate[i].integral_limit = .01f;
    }
    c->vertical.kp = .18f; c->vertical.ki = .03f;
    c->vertical.limit = .05f; c->vertical.integral_limit = .015f;
}
static unsigned ConfigValid(const ClosedLoopConfig *c)
{
    return isfinite(c->hover_duty) && isfinite(c->max_duty) && isfinite(c->target_range_m) &&
        c->hover_duty > 0 && c->hover_duty <= c->max_duty && c->max_duty <= CL_MAX_DUTY &&
        c->target_range_m >= .12f && c->target_range_m <= 1.0f;
}
unsigned ClosedLoop_ArmLevel(const float acc[3])
{
    /* Quaternion starts at identity: an exactly inverted gravity vector has
     * zero cross-product correction. Never rely only on estimated roll/pitch
     * when arming; require direct stationary FRD specific-force evidence. */
    for (unsigned i = 0; i < 3; ++i) if (!isfinite(acc[i])) return 0U;
    float norm = sqrtf(acc[0]*acc[0] + acc[1]*acc[1] + acc[2]*acc[2]);
    return norm >= .90f && norm <= 1.10f && acc[2] < -.85f &&
           fabsf(acc[0]) <= .14f && fabsf(acc[1]) <= .14f;
}
unsigned ClosedLoop_AttitudeMix(ClosedLoop *c, const ClosedLoopSample *s,
    float roll_target, float pitch_target, float dt, ClosedLoopOutput *out)
{
    float r = FlightPid_Step(&c->rate[0], Clamp(3*(roll_target-s->roll), -45, 45), s->gyro[0], dt);
    float p = FlightPid_Step(&c->rate[1], Clamp(3*(pitch_target-s->pitch), -45, 45), s->gyro[1], dt);
    float y = FlightPid_Step(&c->rate[2], 0, s->gyro[2], dt);
    /* Bound correction authority during spool-up; collective zero means all zero. */
    float authority = fminf(1, c->collective / .08f);
    r *= authority; p *= authority; y *= authority;
    float quad[4] = {c->collective+r+p+y, c->collective-r+p-y,
                     c->collective-r-p+y, c->collective+r-p-y};
    for (unsigned i = 0; i < 4; ++i) if (!isfinite(quad[i])) return 0U;
#ifdef CL_TETHERED_FLIGHT
    /* Shift common thrust before clipping so high throttle preserves the
     * attitude differential. Max throttle is not four channels pinned high. */
    float low = quad[0], high = quad[0];
    for (unsigned i = 1; i < 4; ++i) {
        low = fminf(low, quad[i]); high = fmaxf(high, quad[i]);
    }
    float span = high - low;
    if (span > c->config.max_duty) {
        for (unsigned i = 0; i < 4; ++i)
            quad[i] = (quad[i] - low) * c->config.max_duty / span;
    } else {
        float shift = Clamp(0, -low, c->config.max_duty - high);
        for (unsigned i = 0; i < 4; ++i) quad[i] += shift;
    }
#endif
    for (unsigned i = 0; i < 4; ++i) {
        if (!isfinite(quad[i])) return 0U;
        quad[i] = Clamp(quad[i], 0, c->config.max_duty);
    }
    Aircraft1_MapMotors(quad, out->motor); out->collective = c->collective;
#ifdef CL_TETHERED_FLIGHT
    out->collective = .25f * (quad[0] + quad[1] + quad[2] + quad[3]);
#endif
    c->last_mix_collective = out->collective;
    return 1U;
}
void ClosedLoop_Step(ClosedLoop *c, const ClosedLoopSample *s, HoverCommand command,
                     float dt, ClosedLoopOutput *out)
{
    memset(out, 0, sizeof(*out));
    if (command == HOVER_STOP) {
        c->state = CL_IDLE; c->fault = CL_OK; c->seconds = c->collective = 0;
        c->near_ground_seconds = 0; c->flow_engaged = 0; c->landing_flare = 0;
#ifdef CL_FLOW_ASSIST
        FlowAssist_Reset(&c->horizontal);
#endif
        ResetPids(c); goto finish;
    }
    /* Ground INVALID is expected while IDLE; it is never synthesized as height zero. */
    if (c->state == CL_IDLE && command != HOVER_START) goto finish;
    if (c->state == CL_FAULT) goto finish;
    if (!ConfigValid(&c->config)) { Fail(c, CL_INPUT_INVALID); goto finish; }
    if (!c->config.imu_axes_verified) { Fail(c, CL_AXES_UNVERIFIED); goto finish; }
    if (s->output_fault) { Fail(c, CL_OUTPUT_FAULT); goto finish; }
    if (!isfinite(dt) || dt <= 0 || dt > .015f) { Fail(c, CL_TIMING); goto finish; }
    if (!s->link_valid) { Fail(c, CL_LINK_LOST); goto finish; }
    if (!s->imu_valid || !isfinite(s->roll) || !isfinite(s->pitch) ||
        !isfinite(s->gyro[0]) || !isfinite(s->gyro[1]) || !isfinite(s->gyro[2])) {
        Fail(c, CL_IMU_INVALID); goto finish;
    }
    if (fabsf(s->roll) >= 25 || fabsf(s->pitch) >= 25) { Fail(c, CL_TILT); goto finish; }
    if (!s->tof_present || !s->tof_fresh) { Fail(c, CL_TOF_LOST); goto finish; }
    if (s->range_valid && (!isfinite(s->range_m) || !isfinite(s->vz) ||
        /* Sensor validity is checked against RAW >=40mm by the ToF driver.
         * Vertical projection at up to 25deg tilt can be below 40mm. */
        s->range_m < .03f || s->range_m > 1.2f)) { Fail(c, CL_HEIGHT_INVALID); goto finish; }
    if (c->state == CL_IDLE) {
#ifdef CL_FLOW_ASSIST
        if (c->config.use_flow && !s->flow_present) { Fail(c, CL_FLOW_UNAVAILABLE); goto finish; }
        if (c->config.use_flow && !s->flow_calibrated) { Fail(c, CL_FLOW_UNVERIFIED); goto finish; }
#endif
        if (fabsf(s->roll) > 8 || fabsf(s->pitch) > 8 ||
            fabsf(s->gyro[0]) > 10 || fabsf(s->gyro[1]) > 10 || fabsf(s->gyro[2]) > 10 ||
            (s->range_valid && s->range_m > .12f)) { Fail(c, CL_START_TOO_HIGH); goto finish; }
        c->state = CL_LAUNCH; c->seconds = c->collective = 0;
        c->near_ground_seconds = 0; c->flow_engaged = 0; ResetPids(c);
        c->height_feedforward = c->config.hover_duty;
        c->last_mix_collective = 0; c->landing_flare = 0;
#ifdef CL_FLOW_ASSIST
        FlowAssist_Reset(&c->horizontal);
#endif
        c->ground_range_known = s->range_valid;
        c->ground_range = s->range_valid ? s->range_m : .04f;
    }
#ifdef CL_FLOW_ASSIST
    if (c->config.use_flow) {
        /* IMU attitude/rate control always runs. Keep usable flow damping on
         * descent too; fade it only in the blind zone or final motor fade. */
        FlowAssist_Step(&c->horizontal, c->state != CL_LAND || !c->landing_flare,
            s->range_valid && s->range_m >= .08f, s->flow_valid, s->vx, s->vy, dt);
        if (c->horizontal.request_land) command = HOVER_REQUEST_LAND;
    }
#endif
    if (command == HOVER_REQUEST_LAND && c->state != CL_LAND) {
#ifdef CL_TETHERED_FLIGHT
        /* Cancel a ground launch monotonically, even with valid ground ToF. */
        c->landing_flare = c->state == CL_LAUNCH;
#endif
#ifdef CL_F4_SOFT_LAND
        /* A land command can arrive on the first valid airborne sample,
         * before LAUNCH has handed over to the height loop. Retain current
         * delivered thrust, not the uncaptured full-power launch reference. */
        float takeoff_range = c->ground_range_known ?
            fmaxf(.08f, c->ground_range + .04f) : .08f;
        if (c->state == CL_LAUNCH && s->range_valid && s->range_m >= takeoff_range)
            c->landing_flare = 0U;
        c->collective = c->last_mix_collective;
        c->height_feedforward = c->last_mix_collective;
#endif
        c->state = CL_LAND; c->seconds = 0; c->near_ground_seconds = 0;
        c->target_range = s->range_valid ? s->range_m : .04f;
#ifdef CL_F4_SOFT_LAND
        /* Attitude stabilization continues without resetting its rate state. */
        FlightPid_Reset(&c->vertical);
#else
        ResetPids(c);
#endif
    }
    c->seconds += dt;
    if (c->state == CL_LAUNCH) {
        /* Attitude is already closed loop. Range control starts only after a
         * valid sample above the near-ground blind zone arrives. */
        c->collective = fminf(c->config.hover_duty,
                              c->collective + CL_LAUNCH_RAMP_DUTY_PER_SEC * dt);
        float takeoff_range = .08f;
#ifdef CL_TETHERED_FLIGHT
        /* A valid 9cm ground clearance is not evidence of takeoff. */
        if (c->ground_range_known) takeoff_range = fmaxf(takeoff_range, c->ground_range + .04f);
#endif
        if (s->range_valid && s->range_m >= takeoff_range) {
#ifdef CL_TETHERED_FLIGHT
            /* A takeoff reference, not a calibrated equilibrium hover duty. */
            c->height_feedforward = c->last_mix_collective;
#endif
            c->state = CL_ASCEND; c->target_range = s->range_m; c->seconds = 0;
            FlightPid_Reset(&c->vertical);
        } else if (c->seconds >= CL_LAUNCH_TIMEOUT_SEC) { Fail(c, CL_LAUNCH_TIMEOUT); goto finish; }
    }
    if (c->state == CL_ASCEND || c->state == CL_HOLD) {
        if (!s->range_valid) { Fail(c, CL_TOF_LOST); goto finish; }
        c->target_range = fminf(c->config.target_range_m, c->target_range + .10f * dt);
        if (c->target_range >= c->config.target_range_m) c->state = CL_HOLD;
    }
    if (c->state == CL_LAND) {
        if (c->seconds >= 8.0f) { Fail(c, CL_LAND_TIMEOUT); goto finish; }
        float floor = c->ground_range_known ? c->ground_range : .04f;
        unsigned can_detect_floor = c->ground_range_known;
#ifdef CL_TETHERED_FLIGHT
        /* Unknown ground clearance can still reach the valid blind-zone edge. */
        can_detect_floor = 1U;
#endif
        c->target_range = fmaxf(floor, c->target_range - .10f * dt);
        if (!s->range_valid || c->landing_flare) {
            /* Only a near-ground sample with valid device status may be used
             * here; tof_fresh is cleared for other bad statuses in the entry. */
#ifdef CL_TETHERED_FLIGHT
            c->landing_flare = 1U;
#endif
            c->collective = fmaxf(0, c->collective - .20f * dt);
            c->near_ground_seconds += dt;
            if (c->collective <= 0 && c->near_ground_seconds >= .3f) {
                c->state = CL_IDLE; ResetPids(c); goto finish;
            }
        } else if (can_detect_floor && c->target_range <= floor &&
                   s->range_m <= floor + .012f && fabsf(s->vz) <= .05f) {
            c->near_ground_seconds += dt;
            if (c->near_ground_seconds >= .3f) {
#ifdef CL_TETHERED_FLIGHT
                c->landing_flare = 1U;
#endif
                c->collective = fmaxf(0, c->collective - .20f * dt);
                if (c->collective <= 0) {
                    c->state = CL_IDLE; ResetPids(c); goto finish;
                }
            }
        } else c->near_ground_seconds = 0;
    }
    if (c->state != CL_LAUNCH && s->range_valid &&
        !(c->state == CL_LAND && (c->near_ground_seconds >= .3f || c->landing_flare))) {
        float vz_target = Clamp(1.0f * (c->target_range - s->range_m), -.15f, .15f);
        float demanded = Clamp(c->height_feedforward + FlightPid_Step(&c->vertical,
                               vz_target, s->vz, dt), 0, c->config.max_duty);
        /* Prevent a launch->height transition from jumping to hover throttle. */
        c->collective += Clamp(demanded - c->collective, -.25f * dt, .10f * dt);
    }
    float roll_target = 0, pitch_target = 0;
#ifdef CL_FLOW_ASSIST
    if (c->config.use_flow) {
        roll_target = c->horizontal.roll_target;
        pitch_target = c->horizontal.pitch_target;
        c->flow_engaged = c->horizontal.gain > 0;
    }
#else
    if (c->config.use_flow && (c->state == CL_ASCEND || c->state == CL_HOLD)) {
        if (!s->flow_valid || !isfinite(s->vx) || !isfinite(s->vy)) {
            Fail(c, CL_INPUT_INVALID); goto finish;
        }
        c->flow_engaged = 1;
        /* Body velocity damping; not an absolute XY position hold. */
        roll_target = Clamp(-5.0f * s->vy, -5, 5);
        pitch_target = Clamp(5.0f * s->vx, -5, 5);
    }
#endif
    if (!ClosedLoop_AttitudeMix(c, s, roll_target, pitch_target, dt, out)) {
        Fail(c, CL_INPUT_INVALID); goto finish;
    }
finish:
#ifdef CL_FLOW_ASSIST
    out->flow_gain = c->horizontal.gain;
    out->flow_roll_target = c->horizontal.roll_target;
    out->flow_pitch_target = c->horizontal.pitch_target;
    out->flow_mode = (unsigned)c->horizontal.mode;
    out->flow_requested = c->config.use_flow;
    out->flow_auto_land = c->horizontal.request_land;
#endif
    out->height_feedforward = c->height_feedforward;
    out->target_range = c->target_range;
    out->state = c->state; out->fault = c->fault;
}
