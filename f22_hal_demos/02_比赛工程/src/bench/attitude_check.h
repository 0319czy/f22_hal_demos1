#ifndef F22_ATTITUDE_CHECK_H
#define F22_ATTITUDE_CHECK_H
#include "closed_loop.h"
/* Propeller-free only. State HOLD means bench feedback, never flight hold.
 * This mode does not consume or fabricate any range/flow measurement. */
void AttitudeCheck_Step(ClosedLoop *c, const ClosedLoopSample *s,
    HoverCommand command, float dt, ClosedLoopOutput *out);
#endif
