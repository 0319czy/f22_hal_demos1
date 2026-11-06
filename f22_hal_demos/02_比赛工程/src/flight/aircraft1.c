#include "aircraft1.h"
#include "closed_loop_config.h"
#include <math.h>
unsigned Aircraft1_WaitMask(const HoverSample *s, unsigned mount, unsigned flow, float dt)
{
    unsigned wait = 0U;
    if (!s->imu_valid) wait |= AIRCRAFT1_WAIT_IMU;
    if (!s->height_valid) wait |= AIRCRAFT1_WAIT_HEIGHT;
    if (!s->position_valid) wait |= AIRCRAFT1_WAIT_FLOW;
    if (!s->command_link_valid) wait |= AIRCRAFT1_WAIT_LINK;
    if (!mount) wait |= AIRCRAFT1_WAIT_MOUNT;
    if (!flow) wait |= AIRCRAFT1_WAIT_FLOW_AXES;
    if (!isfinite(dt) || dt <= 0.0f || dt > .015f) wait |= AIRCRAFT1_WAIT_TIMING;
    return wait;
}
void Aircraft1_ImuToBody(const float sensor[3], float body[3])
{
    for (unsigned i = 0U; i < 3U; ++i)
        body[i] = cl_imu_mount[3*i]*sensor[0] + cl_imu_mount[3*i+1]*sensor[1] +
                  cl_imu_mount[3*i+2]*sensor[2];
}
HoverCommand Aircraft1_FilterStart(HoverState state, HoverCommand command, unsigned wait)
{
    return state == HOVER_IDLE && command == HOVER_START && wait ? HOVER_NONE : command;
}
void Aircraft1_MapMotors(const float quad[4], float board[4])
{
    /* Reject the whole demand on any bad channel; never partially energize. */
    for (unsigned i = 0U; i < 4U; ++i) {
        if (!isfinite(quad[i]) || quad[i] < 0.0f || quad[i] > 1.0f) {
            for (unsigned j = 0U; j < 4U; ++j) board[j] = 0.0f;
            return;
        }
    }
    float mapped[4] = {quad[1], quad[0], quad[2], quad[3]};
    for (unsigned i = 0U; i < 4U; ++i) board[i] = mapped[i];
}
