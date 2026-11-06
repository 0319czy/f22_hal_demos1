#include "closed_loop.h"
#include "closed_loop_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static ClosedLoopConfig Config(void)
{
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, 1U, 1U}; return cfg;
}
static ClosedLoopSample Sample(void)
{
    ClosedLoopSample s = {0};
    s.imu_valid = s.link_valid = s.tof_present = s.tof_fresh = 1U;
    s.flow_present = s.flow_calibrated = 1U;
    s.flow_control_provisional = CL_FLOW_PROVISIONAL_CONTROL;
    return s;
}
static void Tick(ClosedLoop *c, ClosedLoopSample *s, HoverCommand cmd, ClosedLoopOutput *o)
{
    ClosedLoop_Step(c,s,cmd,.005f,o);
    for (unsigned i = 0; i < 4; ++i)
        assert(isfinite(o->motor[i]) && o->motor[i] >= 0 && o->motor[i] <= CL_MAX_DUTY);
}
static void Zero(ClosedLoopOutput *o)
{ for (unsigned i = 0; i < 4; ++i) assert(o->motor[i] == 0); }
int main(void)
{
    assert(CL_MAX_DUTY == 1.0f && CL_HOVER_DUTY == 1.0f);
    ClosedLoop c; ClosedLoopOutput o; ClosedLoopSample s = Sample();
    ClosedLoop_Init(&c,Config()); s.flow_present = 0;
    Tick(&c,&s,HOVER_START,&o); assert(o.fault == CL_FLOW_UNAVAILABLE); Zero(&o);
    ClosedLoop_Init(&c,Config()); s = Sample(); s.flow_calibrated = 0;
    s.flow_control_provisional = 0;
    Tick(&c,&s,HOVER_START,&o); assert(o.fault == CL_FLOW_UNVERIFIED); Zero(&o);
    ClosedLoop_Init(&c,Config()); s = Sample(); s.flow_calibrated = 0;
    s.flow_control_provisional = 1; s.range_m = .02f;
    Tick(&c,&s,HOVER_START,&o);
    assert(c.state == CL_LAUNCH && o.fault == CL_OK);
    ClosedLoop_Init(&c,Config()); s = Sample(); s.range_m = .02f;
    s.flow_valid = 1; s.vx = 1; s.vy = -1;
    for (unsigned i = 0; i < 100; ++i) {
        Tick(&c,&s,i ? HOVER_NONE : HOVER_START,&o);
        assert(c.state == CL_LAUNCH && o.flow_mode == FLOW_ASSIST_NEAR_GROUND);
        assert(o.flow_gain == 0 && o.flow_roll_target == 0 && o.flow_pitch_target == 0);
    }
    s.range_valid = 1; s.range_m = .06f;
    Tick(&c,&s,HOVER_NONE,&o); assert(o.flow_mode == FLOW_ASSIST_NEAR_GROUND);
    s.range_m = .079f; s.vx = .5f; s.vy = -.4f;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_mode == FLOW_ASSIST_NEAR_GROUND && o.flow_gain == 0);
    s.range_m = .08f;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_ASCEND && o.flow_mode == FLOW_ASSIST_BLEND);
    assert(o.flow_gain > 0 && o.flow_gain < .02f);
    assert(o.flow_roll_target > 0 && o.flow_pitch_target > 0);
    assert(o.flow_roll_target <= .05001f && o.flow_pitch_target <= .05001f);
    for (unsigned i = 0; i < 130; ++i) {
        float r = o.flow_roll_target, p = o.flow_pitch_target;
        Tick(&c,&s,HOVER_NONE,&o);
        assert(fabsf(o.flow_roll_target-r) <= .05001f && fabsf(o.flow_pitch_target-p) <= .05001f);
    }
    assert(o.flow_mode == FLOW_ASSIST_ACTIVE && o.flow_gain > .999f);
    assert(fabsf(o.flow_roll_target-2.0f)<.001f && fabsf(o.flow_pitch_target-2.5f)<.001f);
    assert(o.flow_position_hold);
    /* Relative position feedback brakes drift and asks to return to the
     * captured local point, with bounded body velocity and small tilt. */
    s.flow_x = .20f; s.vx = .05f; s.vy = 0;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_position_hold && o.flow_position_error_forward < -.19f);
    assert(o.flow_velocity_target_forward < 0 && o.flow_velocity_target_forward >= -.20f);
    assert(o.flow_pitch_target > 0 && o.flow_pitch_target <= 5.0f);
    s.flow_x = 0; s.flow_y = 0; s.vx = .5f; s.vy = -.4f;
    s.range_m = .09f;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_mode == FLOW_ASSIST_ACTIVE && o.flow_position_hold);
    s.range_m = .079f;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_mode == FLOW_ASSIST_NEAR_GROUND && o.flow_gain < 1.0f);
    s.range_m = .08f; s.flow_x = .20f; s.flow_y = -.10f; s.vx = s.vy = 0;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_mode == FLOW_ASSIST_BLEND && o.flow_position_hold);
    assert(fabsf(o.flow_position_error_forward) < .001f);
    assert(fabsf(o.flow_position_error_right) < .001f);
    /* A fresh capture after re-entering the optical range must not chase the
     * stale pre-blind-zone coordinate and cause a step in motor demand. */
    for (unsigned i = 0; i < 140; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(fabsf(o.flow_roll_target) < .001f && fabsf(o.flow_pitch_target) < .001f);
    s.flow_valid = 0; s.vx = NAN;
    for (unsigned i = 0; i < 30; ++i) {
        Tick(&c,&s,HOVER_NONE,&o);
        assert(c.state != CL_LAND && !o.flow_auto_land);
    }
    /* A transient dropout can recover without a full-angle step. */
    s.flow_valid = 1; s.vx = -.5f; s.vy = .4f;
    for (unsigned i = 0; i < 140; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_pitch_target < 0 && o.flow_roll_target < 0);
    s.flow_valid = s.flow_present = 0;
    for (unsigned i = 0; i < 55; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_LAND && o.flow_auto_land && c.fault == CL_OK);
    s.flow_valid = s.flow_present = 1;
    for (unsigned i = 0; i < 100; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_LAND && o.flow_auto_land);
    assert(o.flow_roll_target < 0 && o.flow_pitch_target < 0);
    /* Committed landing still uses healthy flow above the blind zone. */
    s.range_m = .06f;
    for (unsigned i = 0; i < 100; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(o.flow_roll_target == 0 && o.flow_pitch_target == 0);
    Tick(&c,&s,HOVER_STOP,&o); assert(c.state == CL_IDLE && !o.flow_auto_land); Zero(&o);
    s = Sample(); Tick(&c,&s,HOVER_START,&o);
    assert(c.state == CL_LAUNCH && o.flow_mode == FLOW_ASSIST_NEAR_GROUND);
    /* With observable vertical state, radio loss starts controlled descent
     * while preserving lift and any still-usable flow damping. */
    s.range_valid = s.flow_valid = 1; s.range_m = .11f; s.vx = .5f; s.vy = -.4f;
    Tick(&c,&s,HOVER_NONE,&o); assert(c.state == CL_ASCEND);
    s.link_valid = 0; Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_LAND && c.fault == CL_OK && o.link_lost_landing);
    assert(o.collective > 0 && o.flow_gain > 0);
    float landing_time = c.seconds;
    Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_LAND && c.seconds > landing_time && o.link_lost_landing);
    assert(o.collective > 0);
    /* F3 requests descent without discarding valid optical velocity feedback. */
    ClosedLoop_Init(&c,Config()); s = Sample();
    Tick(&c,&s,HOVER_START,&o);
    s.range_valid = s.flow_valid = 1; s.range_m = .20f; s.vx = .5f; s.vy = -.4f;
    for (unsigned i = 0; i < 140; ++i) Tick(&c,&s,HOVER_NONE,&o);
    Tick(&c,&s,HOVER_REQUEST_LAND,&o);
    for (unsigned i = 0; i < 50; ++i) Tick(&c,&s,HOVER_NONE,&o);
    assert(c.state == CL_LAND && !o.flow_auto_land);
    assert(o.flow_gain > .999f && o.flow_roll_target > 0 && o.flow_pitch_target > 0);

    /* A quarter-turn in yaw rotates the map displacement into body axes. */
    FlowAssist a; FlowAssist_Reset(&a);
    FlowAssist_Step(&a,1,1,.12f,1,0,0,0,0,90.0f,.005f);
    for (unsigned i=0;i<120;i++)
        FlowAssist_Step(&a,1,1,.12f,1,.20f,0,0,0,90.0f,.005f);
    assert(a.position_hold && fabsf(a.position_error_forward) < .001f);
    assert(a.position_error_right > .19f && a.velocity_target_right > 0);
    assert(a.roll_target > 0);
    FlowAssist_Step(&a,1,1,.079f,0,.20f,0,0,0,90.0f,.005f);
    assert(!a.range_usable && !a.reference_valid && a.mode == FLOW_ASSIST_NEAR_GROUND);
    puts("Flow assist: IMU attitude + relative XY hold at >=8cm, near-ground fade, bounded velocity/tilt targets, provisional gate and STOP passed (synthetic; no calibration or flight claim)");
    return 0;
}
