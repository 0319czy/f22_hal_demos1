#include "flow_runtime.h"
#include "closed_loop_profile.h"
static unsigned AllowIo(FlowSensor *flow, unsigned outputs_stopped, HoverCommand command)
{
#ifdef CL_TETHERED_PWM100
    if (!outputs_stopped || command != HOVER_NONE) {
        flow->valid = 0U;
        return 0U;
    }
#else
    (void)flow; (void)outputs_stopped; (void)command;
#endif
    return 1U;
}
void FlightFlow_BeginCycle(FlowSensor *flow, unsigned outputs_stopped, HoverCommand command)
{ (void)AllowIo(flow, outputs_stopped, command); }
int FlightFlow_Poll(FlowSensor *flow, unsigned outputs_stopped, HoverCommand command,
                    uint32_t now, uint32_t *identity_checked_ms)
{
    if (!AllowIo(flow, outputs_stopped, command)) return 0;
    if (flow->present && (uint32_t)(now - *identity_checked_ms) >= 1000U) {
        (void)Flow_CheckIdentity(flow);
        *identity_checked_ms = now;
    }
    return Flow_Poll(flow);
}
