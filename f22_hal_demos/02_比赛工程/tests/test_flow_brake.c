#include "closed_loop.h"
#include "closed_loop_profile.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    ClosedLoop c; ClosedLoopOutput o;
    ClosedLoopConfig cfg={1,1,.20f,1,1};
    ClosedLoopSample s={.imu_valid=1,.link_valid=1,.tof_present=1,
        .tof_fresh=1,.range_valid=1,.range_m=.20f,.flow_present=1,
        .flow_valid=1,.flow_control_provisional=1,.vx=.6f,.vy=.6f,
        .flow_x=50,.flow_y=-50};
    ClosedLoop_Init(&c,cfg); c.state=CL_HOLD;
    c.collective=c.height_feedforward=.60f; c.target_range=.20f;
    for(unsigned i=0;i<150;++i) {
        ClosedLoop_Step(&c,&s,HOVER_NONE,.005f,&o);
        assert(c.fault==CL_OK && !o.flow_position_hold);
        assert(fabsf(o.flow_roll_target)<=1 && fabsf(o.flow_pitch_target)<=1);
    }
    assert(o.flow_roll_target<-.99f && o.flow_pitch_target>.99f);
    assert(o.flow_position_error_forward==0 && o.flow_position_error_right==0);
    /* Huge accumulated drift never creates a position-chase demand. */
    s.vx=s.vy=0;
    for(unsigned i=0;i<100;++i) ClosedLoop_Step(&c,&s,HOVER_NONE,.005f,&o);
    assert(o.flow_roll_target==0 && o.flow_pitch_target==0);
    s.vx=s.vy=-.6f;
    for(unsigned i=0;i<100;++i) ClosedLoop_Step(&c,&s,HOVER_NONE,.005f,&o);
    assert(o.flow_roll_target>.99f && o.flow_pitch_target<-.99f);
    /* Angular image motion must not become a new brake command. Sustained
     * unusable optical feedback commits to the existing landing policy. */
    s.gyro[2]=20;
    /* 断流超时在 2026-10-28 从 0.25s 放宽到 1.0s（CL_FLOW_MISSING_TIMEOUT_SEC），
     * 这里按 1.2s 推进确保覆盖新阈值。 */
    for(unsigned i=0;i<240 && c.state!=CL_LAND;++i) ClosedLoop_Step(&c,&s,HOVER_NONE,.005f,&o);
    assert(c.state==CL_LAND && o.flow_auto_land);
    assert(o.flow_roll_target==0 && o.flow_pitch_target==0);
    ClosedLoop_Step(&c,&s,HOVER_STOP,.005f,&o);
    for(unsigned i=0;i<4;++i) assert(o.motor[i]==0);
    assert(c.state==CL_IDLE && !o.flow_auto_land);
    /* Below80mm, even huge raw velocity cannot command lateral tilt. */
    ClosedLoop_Init(&c,cfg); c.state=CL_LAUNCH;
    s.range_m=.079f;s.gyro[2]=0;s.vx=s.vy=1.5f;
    for(unsigned i=0;i<100;++i) ClosedLoop_Step(&c,&s,HOVER_NONE,.005f,&o);
    assert(o.flow_gain==0 && o.flow_roll_target==0 && o.flow_pitch_target==0);
    puts("PASS: experimental1deg velocity damping, signs, no XY chase, angular gate, dropout landing,80mm gate and STOP (synthetic only)");
}
