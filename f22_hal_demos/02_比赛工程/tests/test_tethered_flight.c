#include "closed_loop.h"
#include "closed_loop_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static ClosedLoopConfig Config(void)
{
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, CL_TARGET_RANGE_M,
                            CL_IMU_AXES_VERIFIED, 0U};
    return cfg;
}
static ClosedLoopSample Sample(void)
{
    ClosedLoopSample s = {0};
    s.imu_valid = s.link_valid = s.tof_present = s.tof_fresh = 1U;
    return s;
}
static void Bounded(const ClosedLoopOutput *o)
{
    for (unsigned i = 0; i < 4; ++i)
        assert(isfinite(o->motor[i]) && o->motor[i] >= 0 && o->motor[i] <= CL_MAX_DUTY);
}
static void Zero(const ClosedLoopOutput *o)
{ for (unsigned i = 0; i < 4; ++i) assert(o->motor[i] == 0); }
static void Tick(ClosedLoop *c, ClosedLoopSample *s, HoverCommand cmd, ClosedLoopOutput *o)
{ ClosedLoop_Step(c, s, cmd, .005f, o); Bounded(o); }
static void Ramp(ClosedLoop *c, ClosedLoopSample *s, unsigned steps, ClosedLoopOutput *o)
{
    ClosedLoop_Init(c, Config());
    for (unsigned i = 0; i < steps; ++i) {
        Tick(c, s, i ? HOVER_NONE : HOVER_START, o);
        assert(o->state == CL_LAUNCH && o->fault == CL_OK);
    }
}
static void Handover(void)
{
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    Ramp(&c, &s, 240, &o); /* lift at approximately60%, below launch ceiling */
    float reference = o.collective;
    s.range_valid = 1; s.range_m = .09f;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_ASCEND && fabsf(c.height_feedforward-reference) < .0001f);
    assert(fabsf(o.collective-reference) < .0031f);
    for (unsigned i = 0; i < 800; ++i) {
        s.range_m = c.target_range; s.vz = 0;
        Tick(&c, &s, HOVER_NONE, &o);
        assert(c.state == CL_ASCEND || c.state == CL_HOLD);
        assert(fabsf(o.collective-reference) < .01f); /* never chases90% */
    }
    assert(c.state == CL_HOLD);
    ClosedLoop low = c, high = c; ClosedLoopOutput lo, hi;
    s.range_m = .20f; s.vz = -.10f;
    Tick(&low, &s, HOVER_NONE, &lo);
    s.vz = .10f; Tick(&high, &s, HOVER_NONE, &hi);
    assert(lo.collective > hi.collective); /* damps vertical motion */
    Tick(&c, &s, HOVER_STOP, &o); assert(c.state == CL_IDLE); Zero(&o);
    s = Sample(); Ramp(&c, &s, 100, &o);
    s.range_valid = 1; s.range_m = .09f;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.height_feedforward < .26f); /* no previous-flight reference */
}
static void GroundAndTimeout(void)
{
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    s.range_valid = 1; s.range_m = .09f;
    Ramp(&c, &s, 450, &o); /* valid high ground clearance is not takeoff */
    assert(c.ground_range_known && fabsf(c.collective-CL_MAX_DUTY) < .0001f);
    s.range_m = .135f; Tick(&c, &s, HOVER_NONE, &o);
    /* The 12cm low-hover target is already reached at this 13.5cm sample. */
    /* The configured left-drift trim consumes a small amount of high-throttle
     * headroom before the captured reference is stored. */
    assert((c.state == CL_ASCEND || c.state == CL_HOLD) && c.height_feedforward > .88f);
    s = Sample(); Ramp(&c, &s, 450, &o); /* unchanged blind zone beyond old2s */
    for (unsigned i = 0; i < 160; ++i) Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_FAULT && c.fault == CL_LAUNCH_TIMEOUT); Zero(&o);
    Tick(&c, &s, HOVER_START, &o); assert(c.state == CL_FAULT); Zero(&o);
    Tick(&c, &s, HOVER_STOP, &o); assert(c.state == CL_IDLE); Zero(&o);
}
static void MixerHeadroom(void)
{
    for (unsigned i = 0; i < 16; ++i) {
        ClosedLoop mid, top; ClosedLoopOutput a = {0}, b = {0};
        ClosedLoopSample s = Sample();
        s.roll = (i & 1) ? 6 : -6; s.pitch = (i & 2) ? 6 : -6;
        s.gyro[2] = (i & 4) ? 15 : -15;
        ClosedLoop_Init(&mid, Config()); ClosedLoop_Init(&top, Config());
        mid.collective = .6f;
        top.collective = CL_MAX_DUTY - ((i & 8) ? 0.0f : .01f);
        assert(ClosedLoop_AttitudeMix(&mid, &s, 0, 0, .005f, &a));
        assert(ClosedLoop_AttitudeMix(&top, &s, 0, 0, .005f, &b));
        Bounded(&b);
        for (unsigned m = 0; m < 4; ++m) for (unsigned n = m+1; n < 4; ++n)
            assert(fabsf((a.motor[m]-a.motor[n])-(b.motor[m]-b.motor[n])) < .00001f);
    }
    ClosedLoop c; ClosedLoopOutput o = {0}; ClosedLoopSample s = Sample();
    ClosedLoop_Init(&c, Config()); s.roll = 20;
    assert(ClosedLoop_AttitudeMix(&c, &s, 0, 0, .005f, &o)); Zero(&o);
}
static void Landing(void)
{
    /* Airborne LAND can reduce PWM twice as quickly as HOLD for the same
     * downward demand, without invoking the near-ground fade. */
    for (unsigned landing = 0; landing < 2; ++landing) {
        ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
        ClosedLoop_Init(&c, Config());
        c.state = landing ? CL_LAND : CL_HOLD;
        c.collective = .50f; c.height_feedforward = .30f; c.target_range = .20f;
        s.range_valid = 1U; s.range_m = .20f;
        Tick(&c, &s, HOVER_NONE, &o);
        assert(c.fault == CL_OK && !c.landing_flare);
        assert(fabsf(.50f - c.collective - (landing ? .50f : .25f) * .005f) < .000001f);
    }
    /* From a high captured reference, landing must accept either a valid floor
     * estimate or fresh raw ToF evidence inside the fused observer blind zone. */
    for (unsigned valid_floor = 0; valid_floor < 2; ++valid_floor) {
        ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
        Ramp(&c, &s, 370, &o);
        s.range_valid = 1; s.range_m = .09f; Tick(&c, &s, HOVER_NONE, &o);
        assert(c.state == CL_ASCEND && c.height_feedforward > .88f);
        Tick(&c, &s, HOVER_REQUEST_LAND, &o); assert(c.state == CL_LAND);
        s.range_valid = valid_floor; s.range_m = .04f; s.vz = 0;
        s.tof_near_ground = valid_floor ? 0U : 1U;
        if (!valid_floor) {
            /* An invalid fused estimate while still outside the blind-zone
             * edge must not be interpreted as touchdown or start motor fade. */
            s.tof_near_ground = 0U;
            for (unsigned i = 0; i < 30; ++i) Tick(&c, &s, HOVER_NONE, &o);
            assert(c.state == CL_LAND && !c.landing_flare && o.collective > .88f);
            s.tof_near_ground = 1U;
        }
        for (unsigned i = 0; i < 1500 && c.state == CL_LAND; ++i)
            Tick(&c, &s, HOVER_NONE, &o);
        assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);
    }
    /* F3 during blind launch cancels spool-up and reaches stopped IDLE. */
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    Ramp(&c, &s, 150, &o);
    float before_cancel = c.collective;
    Tick(&c, &s, HOVER_REQUEST_LAND, &o);
    /* Faster final fade: 40 percentage points/s, still a ramp, not a cut. */
    assert(fabsf(before_cancel - c.collective - .40f * .005f) < .000001f);
    for (unsigned i = 0; i < 700; ++i) Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_IDLE); Zero(&o);
    /* F3 abort with VALID ground ToF must never spool UP toward the as-yet
     * uncaptured90% height reference while waiting for floor detection. */
    for (unsigned high_ground = 0; high_ground < 2; ++high_ground) {
        s = Sample(); s.range_valid = 1; s.range_m = high_ground ? .09f : .04f;
        Ramp(&c, &s, 80, &o);
        float prior = o.collective;
        for (unsigned i = 0; i < 1000; ++i) {
            Tick(&c, &s, i ? HOVER_NONE : HOVER_REQUEST_LAND, &o);
            assert(o.collective <= prior + .00001f);
            prior = o.collective;
        }
        assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);
    }
    /* Once raw near-ground evidence starts final fade, validity/velocity
     * jitter cannot re-enable thrust. */
    s = Sample(); Ramp(&c, &s, 370, &o);
    s.range_valid = 1; s.range_m = .09f; Tick(&c, &s, HOVER_NONE, &o);
    Tick(&c, &s, HOVER_REQUEST_LAND, &o);
    s.range_valid = 0; s.tof_near_ground = 1U;
    for (unsigned i = 0; i < 200 && !c.landing_flare; ++i)
        Tick(&c, &s, HOVER_NONE, &o);
    assert(c.landing_flare);
    float prior = o.collective;
    for (unsigned i = 0; i < 1300; ++i) {
        s.range_valid = i % 2; s.range_m = (i % 3) ? .06f : .04f;
        s.vz = (i % 2) ? .06f : -.06f;
        Tick(&c, &s, HOVER_NONE, &o);
        assert(o.collective <= prior + .00001f); prior = o.collective;
    }
    assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);
}
static void Guards(void)
{
    for (unsigned fault = 1; fault < 6; ++fault) {
        ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
        Ramp(&c, &s, 150, &o);
        if (fault == 1) s.imu_valid = 0;
        if (fault == 2) s.roll = 25;
        if (fault == 3) s.output_fault = 1;
        if (fault == 4) s.tof_fresh = 0;
        if (fault == 4) {
            /* ToF 新鲜度断档：宽限窗口内允许继续起飞，耗尽后仍必须停机。 */
            ClosedLoop_Step(&c, &s, HOVER_NONE, .005f, &o);
            assert(c.state != CL_FAULT);
            for (unsigned i = 0; i < 40 && c.state != CL_FAULT; ++i)
                Tick(&c, &s, HOVER_NONE, &o);
        } else {
            ClosedLoop_Step(&c, &s, HOVER_NONE, fault == 5 ? .016f : .005f, &o);
        }
        assert(c.state == CL_FAULT); Zero(&o);
        Tick(&c, &s, HOVER_START, &o); assert(c.state == CL_FAULT); Zero(&o);
        Tick(&c, &s, HOVER_STOP, &o); assert(c.state == CL_IDLE); Zero(&o);
    }
}
static void LinkLossLanding(void)
{
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    Ramp(&c, &s, 370, &o);
    s.range_valid = 1U; s.range_m = .09f;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_ASCEND && o.collective > 0.0f);

    s.link_valid = 0U;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_LAND && c.fault == CL_OK);
    assert(c.link_lost_landing && o.link_lost_landing);
    assert(o.collective > 0.0f); /* radio loss must not cut lift immediately */

    /* Continue a sensor-verified landing without radio packets. */
    for (unsigned i = 0; i < 100; ++i) {
        s.range_m = fmaxf(.04f, s.range_m - .0005f);
        Tick(&c, &s, HOVER_NONE, &o);
        assert(c.state == CL_LAND || c.state == CL_IDLE);
        assert(c.link_lost_landing && o.link_lost_landing);
    }
    assert(c.state == CL_LAND);
    s.range_m = .04f; s.tof_near_ground = 1U; s.vz = 0.0f;
    for (unsigned i = 0; i < 1500 && c.state == CL_LAND; ++i)
        Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);

    /* Idle remains stopped without radio; a stale-link START is rejected. */
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);
    Tick(&c, &s, HOVER_START, &o);
    assert(c.state == CL_FAULT && c.fault == CL_LINK_LOST); Zero(&o);

    /* In the ToF blind zone, radio loss is not called a controlled landing
     * unless fresh raw distance confirms the ground-launch abort path. */
    s = Sample(); Ramp(&c, &s, 80, &o);
    s.link_valid = 0U;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_FAULT && c.fault == CL_LINK_LOST); Zero(&o);

    s = Sample(); s.tof_near_ground = 1U; Ramp(&c, &s, 80, &o);
    s.tof_fresh = 0U; s.link_valid = 0U;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_FAULT && c.fault == CL_LINK_LOST); Zero(&o);

    s = Sample(); s.tof_near_ground = 1U; Ramp(&c, &s, 80, &o);
    float launch_output = o.collective;
    s.link_valid = 0U;
    Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_LAND && c.fault == CL_OK && c.link_lost_landing);
    assert(c.landing_flare && o.collective > 0.0f && o.collective < launch_output);
    for (unsigned i = 0; i < 200 && c.state == CL_LAND; ++i)
        Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_IDLE && c.fault == CL_OK); Zero(&o);

    /* Loss of a critical sensor during a healthy landing still faults out. */
    s = Sample(); Ramp(&c, &s, 370, &o);
    s.range_valid = 1U; s.range_m = .09f;
    Tick(&c, &s, HOVER_NONE, &o);
    s.link_valid = 0U; Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_LAND && c.fault == CL_OK && o.collective > 0.0f);
    s.tof_present = 0U; Tick(&c, &s, HOVER_NONE, &o);
    assert(c.state == CL_FAULT && c.fault == CL_TOF_LOST); Zero(&o);
}
static void LaunchNearGroundBlindZone(void)
{
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    s.range_valid = 1U; s.range_m = .02f; s.tof_near_ground = 1U;
    Ramp(&c, &s, 10, &o);
    assert(c.state == CL_LAUNCH && c.fault == CL_OK);
    assert(c.collective > 0.0f);
}
int main(void)
{
    assert(CL_IMU_AXES_VERIFIED == 1 && CL_FLOW_AXES_VERIFIED == 0);
#ifdef CL_TETHERED_PWM100
    assert(CL_MAX_DUTY == 1.0f && CL_HOVER_DUTY == 1.0f);
#else
    assert(CL_MAX_DUTY == .9f && CL_HOVER_DUTY == .9f);
#endif
    Handover(); GroundAndTimeout(); LaunchNearGroundBlindZone(); MixerHeadroom();
    Landing(); Guards(); LinkLossLanding();
    puts("Tether trial: captured takeoff reference, differential preservation, real ground clearance, bounded launch, landing and stop/fault guards passed (synthetic only)");
    return 0;
}
