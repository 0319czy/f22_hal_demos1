#include "closed_loop.h"
#include "closed_loop_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void Setup(ClosedLoop *c, ClosedLoopSample *s)
{
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, 1U, 0U};
    ClosedLoop_Init(c, cfg);
    c->state = CL_HOLD;
    c->collective = c->last_mix_collective = c->height_feedforward = .70f;
    c->target_range = .20f;
    *s = (ClosedLoopSample){.imu_valid=1, .link_valid=1,
        .tof_present=1, .tof_fresh=1, .tof_near_ground=1,
        .range_valid=1, .range_m=.028832f, .vz=-.524f};
}
static void Zero(const ClosedLoopOutput *o)
{ for (unsigned i=0; i<4; ++i) assert(o->motor[i] == 0); }
int main(void)
{
    ClosedLoop c; ClosedLoopSample s; ClosedLoopOutput o;
    /* Recorded fault12: recent raw36mm, valid short IMU coast28.832mm,
     * descending0.524m/s. Do not turn a physically plausible floor crossing
     * into a one-cycle power cut or chase the old20cm height target. */
    for (unsigned phase=0; phase<3; ++phase) {
        Setup(&c,&s); c.state=phase==0?CL_ASCEND:phase==1?CL_HOLD:CL_LAND;
        ClosedLoop_Step(&c,&s,HOVER_NONE,.006f,&o);
        assert(c.state==CL_LAND && c.fault==CL_OK && c.landing_flare);
        assert(o.collective>0 && o.collective<.70f);
        float previous=o.collective;
        for (unsigned i=0; i<1000 && c.state!=CL_IDLE; ++i) {
            /* Height can expire or briefly rebound, but terminal fade must
             * never resume the airborne PI or re-arm on repeated START. */
            s.range_valid=i%2; s.vz=(i%3)?-.1f:.1f;
            s.range_m=(i%4)? .029f:.05f;
            ClosedLoop_Step(&c,&s,HOVER_START,.006f,&o);
            assert(c.fault==CL_OK && o.collective<=previous+.000001f);
            previous=o.collective;
        }
        assert(c.state==CL_IDLE); Zero(&o);
    }
    /* Independent evidence is mandatory. Preserve malformed/stale/critical
     * input guards and the ability to stop immediately during terminal fade. */
    for (unsigned bad=0; bad<8; ++bad) {
        Setup(&c,&s);
        if (bad==0) s.tof_near_ground=0;
        if (bad==1) s.tof_fresh=0;
        if (bad==2) s.range_m=-.001f;
        if (bad==3) s.range_m=NAN;
        if (bad==4) s.vz=.1f;
        if (bad==5) s.tof_present=0;
        if (bad==6) s.imu_valid=0;
        if (bad==7) s.output_fault=1;
        ClosedLoop_Step(&c,&s,HOVER_NONE,.006f,&o);
        assert(c.state==CL_FAULT); Zero(&o);
    }
    Setup(&c,&s); ClosedLoop_Step(&c,&s,HOVER_NONE,.006f,&o);
    ClosedLoop_Step(&c,&s,HOVER_STOP,NAN,&o);
    assert(c.state==CL_IDLE && c.fault==CL_OK); Zero(&o);
    puts("PASS: recorded28.832mm floor crossing, evidence-gated terminal fade, no reboost, invalid/stale/sensor/output guards and STOP (synthetic)");
}
