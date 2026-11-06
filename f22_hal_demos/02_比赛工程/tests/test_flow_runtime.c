#include "flow_runtime.h"
#include <assert.h>
#include <stdio.h>
static unsigned identity_calls, poll_calls, simulated_ms;
int Flow_CheckIdentity(FlowSensor *s)
{ (void)s; ++identity_calls; simulated_ms += 10; return -1; }
int Flow_Poll(FlowSensor *s)
{ (void)s; ++poll_calls; simulated_ms += 10; return -3; }
int main(void)
{
    FlowSensor flow = {.present=1,.valid=1}; uint32_t checked = 0;
    /* START between sensor windows must invalidate idle data with no SPI. */
    FlightFlow_BeginCycle(&flow,1,HOVER_START);
#ifdef CL_TETHERED_PWM100
    assert(!flow.valid && !identity_calls && !poll_calls && !simulated_ms);
#else
    assert(flow.valid);
#endif
    for (unsigned i = 0; i < 500; ++i) {
        int result = FlightFlow_Poll(&flow,0,HOVER_NONE,2000+i,&checked);
#ifdef CL_TETHERED_PWM100
        assert(result == 0 && !flow.valid);
#else
        assert(result == -3);
#endif
    }
#ifdef CL_TETHERED_PWM100
    assert(!identity_calls && !poll_calls && !simulated_ms);
    assert(!FlightFlow_Poll(&flow,1,HOVER_START,3000,&checked));
    assert(!FlightFlow_Poll(&flow,1,HOVER_STOP,3000,&checked));
    assert(!identity_calls && !poll_calls);
#else
    assert(identity_calls && poll_calls == 500 && simulated_ms > 0);
#endif
    assert(FlightFlow_Poll(&flow,1,HOVER_NONE,4000,&checked) == -3);
    assert(identity_calls && poll_calls && checked == 4000);
    puts("Flow runtime timeout injection: no-flow START/active cycles isolated; idle diagnostics and flow-enabled error path preserved PASS (synthetic I/O)");
}
