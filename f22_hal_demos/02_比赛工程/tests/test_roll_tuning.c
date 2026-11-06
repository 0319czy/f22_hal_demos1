#include "closed_loop.h"
#include "closed_loop_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    ClosedLoopConfig cfg = {1, 1, .20f, 1, 1};
    ClosedLoop base, tuned; ClosedLoopOutput a = {0}, b = {0};
    ClosedLoopSample s = {.roll=8, .gyro={129.03f,0,0}};
    ClosedLoop_Init(&base,cfg); ClosedLoop_Init(&tuned,cfg);
    base.collective = tuned.collective = .95f;
    base.rate[0].limit = .04f;
    assert(tuned.rate[0].limit == .08f);
    assert(tuned.rate[1].limit == .04f && tuned.rate[2].limit == .04f);
    assert(base.rate[0].kp == tuned.rate[0].kp && base.rate[0].ki == tuned.rate[0].ki);
    assert(ClosedLoop_AttitudeMix(&base,&s,0,0,.006f,&a));
    assert(ClosedLoop_AttitudeMix(&tuned,&s,0,0,.006f,&b));
    /* Positive roll/right-down: right pair M1/M3 must beat left M2/M6.
     * Compare high-throttle differential after the real motor permutation. */
    /* Include the configured static left-drift trim in the mapped pair
     * differential: board M1-M2 is reduced by twice that trim. */
    assert(fabsf((a.motor[0]-a.motor[1])-(.08f-2.0f*CL_LEFT_DRIFT_TRIM)) < .00001f);
    assert(fabsf((b.motor[0]-b.motor[1])-(.16f-2.0f*CL_LEFT_DRIFT_TRIM)) < .00001f);
    assert(b.motor[2] > b.motor[3]);
    assert(b.rate_saturated_mask & 1U);
    assert(fabsf(b.rate_reference[0]+24) < .00001f);
    assert(fabsf(b.rate_correction[0]+.08f) < .00001f);
    for (unsigned i=0;i<4;++i) assert(b.motor[i]>=0 && b.motor[i]<=1);
    /* If recovery rotates back too fast, rate damping must reverse demand. */
    ClosedLoop_Init(&tuned,cfg);tuned.collective=.95f;s.gyro[0]=-129.03f;
    assert(ClosedLoop_AttitudeMix(&tuned,&s,0,0,.006f,&b));
    assert(b.rate_correction[0]>0 && b.motor[0]<b.motor[1] && b.motor[2]<b.motor[3]);
    /* Small errors retain the previous unsaturated gain. */
    ClosedLoop_Init(&base,cfg);ClosedLoop_Init(&tuned,cfg);
    base.rate[0].limit=.04f;base.collective=tuned.collective=.6f;
    s.roll=1;s.gyro[0]=0;
    assert(ClosedLoop_AttitudeMix(&base,&s,0,0,.006f,&a));
    assert(ClosedLoop_AttitudeMix(&tuned,&s,0,0,.006f,&b));
    for(unsigned i=0;i<4;++i) assert(fabsf(a.motor[i]-b.motor[i])<.00001f);
    ClosedLoop_Step(&tuned,&s,HOVER_STOP,.006f,&b);
    for(unsigned i=0;i<4;++i) assert(b.motor[i]==0);
    puts("PASS: roll-only4->8 percent authority, mapped restoring differential, high-PWM headroom, reverse damping, unchanged small-error gains and immediate STOP (synthetic, no flight claim)");
    return 0;
}
