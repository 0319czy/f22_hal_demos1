#include "closed_loop.h"
#include "closed_loop_config.h"
#include "aircraft1.h"
#include "observation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static ClosedLoopConfig Config(void)
{ ClosedLoopConfig c = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, 1U, 0U}; return c; }
static ClosedLoopSample Sample(void)
{
    ClosedLoopSample s = {0};
    s.imu_valid = s.link_valid = s.tof_present = s.tof_fresh = 1;
    s.range_m = .02f; /* fresh status in the ToF near-ground blind zone */
    return s;
}
static void Zero(const ClosedLoopOutput *o)
{ for (unsigned i = 0; i < 4; ++i) assert(o->motor[i] == 0); }
static void Bounded(const ClosedLoopOutput *o)
{ for (unsigned i = 0; i < 4; ++i) assert(isfinite(o->motor[i]) && o->motor[i] >= 0 && o->motor[i] <= CL_MAX_DUTY); }
static void Hold(ClosedLoop *c)
{
    ClosedLoop_Init(c, Config()); c->state = CL_HOLD;
    c->target_range = .20f; c->collective = CL_HOVER_DUTY;
    /* Height-loop assertions isolate vertical feedback from the operator's
     * mechanical trim settings. Trim behavior is covered by roll_tuning. */
    c->tuning.left_trim = c->tuning.front_trim = c->tuning.yaw_trim = 0.0f;
    c->tuning.collective_trim = 0.0f;
}
static void GravityAlignmentGate(void)
{
    Attitude a; Attitude_Init(&a);
    const float rest[3] = {0,0,-1}, still[3] = {0,0,0};
    for (unsigned i=0U; i<39U; ++i)
        assert(!Attitude_Update(&a, rest, still, .005f));
    assert(!a.valid && !a.initialized);
    float moving_rate[3] = {0,0,6};
    assert(!Attitude_Update(&a, rest, moving_rate, .005f));
    for (unsigned i=0U; i<39U; ++i)
        assert(!Attitude_Update(&a, rest, still, .005f));
    assert(Attitude_Update(&a, rest, still, .005f));
    assert(a.valid && a.initialized && fabsf(a.roll)<.01f && fabsf(a.pitch)<.01f);

    /* A changing specific-force direction with near-1g magnitude is not a
     * static gravity reference; discard it and wait for a stable window. */
    Attitude_Init(&a);
    for (unsigned i=0U; i<80U; ++i) {
        float moving[3] = {(i & 1U) ? .10f : -.10f, 0.0f, -sqrtf(.99f)};
        assert(!Attitude_Update(&a, moving, still, .005f));
    }
    assert(!a.valid && !a.initialized);
    for (unsigned i=0U; i<39U; ++i)
        assert(!Attitude_Update(&a, rest, still, .005f));
    assert(Attitude_Update(&a, rest, still, .005f));

    /* Gravity alone cannot establish a level reference when inverted. */
    Attitude_Init(&a);
    const float inverted[3] = {0,0,1};
    for (unsigned i=0U; i<40U; ++i)
        (void)Attitude_Update(&a, inverted, still, .005f);
    assert(!a.valid && !a.initialized);
}
static void MountFeedback(void)
{
    /* Synthetic raw vectors consistent with the latest motor-labelled reports:
     * nose down -> OLD pitch+; right side down -> OLD roll-.
     * These are not captured raw IMU samples or physical torque evidence. */
    const float sn = .173648178f, cs = .984807753f;
    const struct {
        float acc[3], rate[3], roll, pitch, body_rate[3];
    } poses[] = {
        {{sn,0,cs}, {0,-15,0}, 10,0, {15,0,0}},
        {{-sn,0,cs}, {0,15,0}, -10,0, {-15,0,0}},
        {{0,sn,cs}, {15,0,0}, 0,-10, {0,-15,0}},
        {{0,-sn,cs}, {-15,0,0}, 0,10, {0,15,0}}
    };
    for (unsigned n = 0U; n < sizeof(poses)/sizeof(poses[0]); ++n) {
        float acc[3], gyro[3], still[3] = {0}; Attitude attitude;
        Aircraft1_ImuToBody(poses[n].acc, acc);
        Aircraft1_ImuToBody(poses[n].rate, gyro);
        for (unsigned k = 0U; k < 3U; ++k)
            assert(fabsf(gyro[k] - poses[n].body_rate[k]) < .001f);
        Attitude_Init(&attitude);
        unsigned aligned = 0U;
        for (unsigned k = 0U; k < 1200U; ++k)
            aligned = Attitude_Update(&attitude, acc, still, .005f);
        assert(aligned && attitude.initialized);
        assert(fabsf(attitude.roll - poses[n].roll) < .1f);
        assert(fabsf(attitude.pitch - poses[n].pitch) < .1f);
        for (unsigned rate_only = 0U; rate_only < 2U; ++rate_only) {
            ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
            Hold(&c); c.collective = .3f;
            s.range_valid = 1U; s.range_m = .20f;
            s.roll = rate_only ? 0 : attitude.roll;
            s.pitch = rate_only ? 0 : attitude.pitch;
            for (unsigned k = 0U; k < 3U; ++k) s.gyro[k] = rate_only ? gyro[k] : 0;
            ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
            assert(c.state == CL_HOLD && c.fault == CL_OK);
            if (n == 0U) { /* right down: right motors demand more */
                assert(o.motor[AIRCRAFT1_M1] > o.motor[AIRCRAFT1_M2]);
                assert(o.motor[AIRCRAFT1_M3] > o.motor[AIRCRAFT1_M6]);
            } else if (n == 1U) {
                assert(o.motor[AIRCRAFT1_M1] < o.motor[AIRCRAFT1_M2]);
                assert(o.motor[AIRCRAFT1_M3] < o.motor[AIRCRAFT1_M6]);
            } else if (n == 2U) { /* nose down: front motors demand more */
                assert(o.motor[AIRCRAFT1_M1] > o.motor[AIRCRAFT1_M3]);
                assert(o.motor[AIRCRAFT1_M2] > o.motor[AIRCRAFT1_M6]);
            } else {
                assert(o.motor[AIRCRAFT1_M1] < o.motor[AIRCRAFT1_M3]);
                assert(o.motor[AIRCRAFT1_M2] < o.motor[AIRCRAFT1_M6]);
            }
        }
    }
    const float *m = cl_imu_mount;
    float determinant = m[0]*(m[4]*m[8]-m[5]*m[7]) -
        m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
    assert(fabsf(determinant - 1.0f) < .001f);
    puts("Synthetic IMU mount -> attitude/rate -> restoring motor demand passed");
}
int main(void)
{
    GravityAlignmentGate();
    MountFeedback();
    /* A candidate installation must not permit flight start until accepted. */
    ClosedLoop pending; ClosedLoopOutput pending_out;
    ClosedLoopConfig pending_cfg = Config(); pending_cfg.imu_axes_verified = 0U;
    ClosedLoop_Init(&pending, pending_cfg); ClosedLoopSample pending_sample = Sample();
    ClosedLoop_Step(&pending, &pending_sample, HOVER_START, .005f, &pending_out);
    assert(pending.state == CL_FAULT && pending.fault == CL_AXES_UNVERIFIED);
    Zero(&pending_out);
    float a[3] = {0, 0, -1}; assert(ClosedLoop_ArmLevel(a));
    a[2] = 1; assert(!ClosedLoop_ArmLevel(a)); /* upside-down quaternion singularity */
    a[2] = -1; a[0] = .20f; assert(!ClosedLoop_ArmLevel(a));
    a[0] = 0; a[1] = NAN; assert(!ClosedLoop_ArmLevel(a));
    a[1] = 0; a[2] = -.5f; assert(!ClosedLoop_ArmLevel(a));
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    ClosedLoop_Init(&c, Config());
    for (unsigned i = 0; i < 100; ++i) {
        ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
        assert(c.state == CL_IDLE); Zero(&o);
    }
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.state == CL_LAUNCH); Bounded(&o);
    for (unsigned i = 0; i < 39; ++i) ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_LAUNCH && o.collective > .09f && o.collective < .11f);
    for (unsigned i = 0; i < 160; ++i) ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_LAUNCH && fabsf(o.collective - .50f) < .001f);
    for (unsigned i = 0; i < 180; ++i) ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_LAUNCH && fabsf(o.collective - CL_HOVER_DUTY) < .001f);
    Bounded(&o);
    s.range_valid = 1; s.range_m = .09f;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_ASCEND); Bounded(&o);
    float prior = o.collective;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(o.collective - prior <= .00051f); /* no handover throttle jump */
    /* Height feedback changes REAL mapped motor demands. */
    ClosedLoop low, high; ClosedLoopOutput lo, hi; Hold(&low); Hold(&high);
    s.range_m = .15f; ClosedLoopSample h = s; h.range_m = .25f;
    for (unsigned i = 0; i < 100; ++i) {
        ClosedLoop_Step(&low, &s, HOVER_NONE, .005f, &lo);
        ClosedLoop_Step(&high, &h, HOVER_NONE, .005f, &hi);
    }
    assert(lo.collective > hi.collective);
    for (unsigned i = 0; i < 4; ++i) assert(lo.motor[i] > hi.motor[i]);
    Hold(&c); s.range_m = .20f; s.roll = 5;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(o.motor[1] < o.motor[0] && o.motor[3] < o.motor[2]);
    Hold(&c); s.roll = 0; s.pitch = 5;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(o.motor[1] < o.motor[3] && o.motor[0] < o.motor[2]);
    Hold(&c); s.pitch = 0; s.gyro[2] = 10;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(o.motor[1] < o.motor[0] && o.motor[2] < o.motor[3]);
    s = Sample(); ClosedLoop_Init(&c, Config());
    for (unsigned i = 0; i < 410; ++i)
        ClosedLoop_Step(&c, &s, i == 0 ? HOVER_START : HOVER_NONE, .005f, &o);
    assert(c.state == CL_FAULT && c.fault == CL_LAUNCH_TIMEOUT); Zero(&o);
    s.range_valid = 1; s.range_m = .09f;
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.state == CL_FAULT); Zero(&o);
    ClosedLoop_Step(&c, &s, HOVER_STOP, NAN, &o);
    assert(c.state == CL_IDLE); Zero(&o);
    /* A landed aircraft with 9cm sensor-to-floor clearance still has VALID ToF:
     * LAND must taper at its captured ground baseline, not wait for INVALID. */
    ClosedLoop_Init(&c, Config()); s = Sample(); s.range_valid = 1; s.range_m = .09f;
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.ground_range_known && c.ground_range == .09f);
    c.state = CL_HOLD; c.collective = CL_HOVER_DUTY; c.target_range = .20f;
    ClosedLoop_Step(&c, &s, HOVER_REQUEST_LAND, .005f, &o);
    for (unsigned i = 0; i < 1300; ++i) ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);
    /* A newer takeoff must not reuse the previous landing reference. */
    s.range_valid = 0; s.range_m = .02f;
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(!c.ground_range_known);
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    s.link_valid = 0; ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_LINK_LOST); Zero(&o);
    ClosedLoop_Init(&c, Config()); s = Sample(); s.gyro[2] = 11;
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.fault == CL_START_TOO_HIGH); Zero(&o);
    ClosedLoop_Init(&c, Config()); s = Sample(); s.range_valid = 1; s.range_m = .20f;
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.fault == CL_START_TOO_HIGH); Zero(&o);
    s = Sample(); s.range_valid = 1; s.range_m = .20f;
    Hold(&c); s.range_valid = 0;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_TOF_LOST); Zero(&o);
    Hold(&c); s = Sample(); s.tof_fresh = 0;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_TOF_LOST); Zero(&o);
    Hold(&c); s = Sample(); s.roll = 25;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_TILT); Zero(&o);
    Hold(&c); s = Sample();
    ClosedLoop_Step(&c, &s, HOVER_NONE, .016f, &o);
    assert(c.fault == CL_TIMING); Zero(&o);
    Hold(&c); s.output_fault = 1;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_OUTPUT_FAULT); Zero(&o);
    Hold(&c); s = Sample(); s.gyro[0] = NAN;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_IMU_INVALID); Zero(&o);
    /* Unknown flow never disables attitude/height. Once explicitly enabled,
     * invalid optical velocity must fault rather than pretending zero motion. */
    Hold(&c); s = Sample(); s.range_valid = 1; s.range_m = .20f;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_HOLD);
    c.config.use_flow = 1;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.fault == CL_INPUT_INVALID); Zero(&o);
    /* Fused height invalid by itself is not touchdown evidence. A fresh raw
     * ToF result inside the 5..39mm blind band may start the terminal fade. */
    Hold(&c); s = Sample(); s.range_valid = 0; s.tof_near_ground = 1U;
    ClosedLoop_Step(&c, &s, HOVER_REQUEST_LAND, .005f, &o);
    for (unsigned i = 0; i < 1000; ++i) ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_IDLE); Zero(&o);
    /* Regression: a valid 40mm raw sample projects below 40mm when tilted.
     * During ground launch this must not be reported as malformed input. */
    HeightEstimate projected; Height_Reset(&projected);
    assert(Height_Update(&projected, .04f, 5, 0, 100));
    assert(projected.height < .04f && projected.height > .03f);
    ClosedLoop_Init(&c, Config()); c.state = CL_LAUNCH;
    s = Sample(); s.roll = 5; s.range_valid = 1; s.range_m = projected.height;
    ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
    assert(c.state == CL_LAUNCH && c.fault == CL_OK);
    ClosedLoopConfig invalid_cfg = Config(); invalid_cfg.max_duty = CL_MAX_DUTY + .01f;
    ClosedLoop_Init(&c, invalid_cfg); s = Sample();
    ClosedLoop_Step(&c, &s, HOVER_START, .005f, &o);
    assert(c.state == CL_FAULT && c.fault == CL_INPUT_INVALID); Zero(&o);
    puts("Physical closed-loop feedback, mix signs, blind-zone handover and fault tests passed (synthetic inputs)");
    return 0;
}
