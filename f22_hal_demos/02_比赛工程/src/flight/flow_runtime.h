#ifndef F22_FLOW_RUNTIME_H
#define F22_FLOW_RUNTIME_H
#include "flow.h"
#include "hover.h"
/* Physical entry only. In a no-flow trial, SPI diagnostics are idle-only;
 * unused optical hardware must not consume an active output cycle's budget. */
void FlightFlow_BeginCycle(FlowSensor *flow, unsigned outputs_stopped, HoverCommand command);
int FlightFlow_Poll(FlowSensor *flow, unsigned outputs_stopped, HoverCommand command,
                    uint32_t now, uint32_t *identity_checked_ms);
#endif
