#ifndef AIRCRAFT1_H
#define AIRCRAFT1_H
#include "hover.h"
/* Same fly1 aircraft, user rotated the forward reference by 180 degrees.
 * FL=M2 CCW, FR=M1 CW, RR=M3 CCW, RL=M6 CW (viewed from above). */
enum { AIRCRAFT1_M1, AIRCRAFT1_M2, AIRCRAFT1_M3, AIRCRAFT1_M6 };
enum {
    AIRCRAFT1_WAIT_IMU = 1U, AIRCRAFT1_WAIT_HEIGHT = 2U,
    AIRCRAFT1_WAIT_FLOW = 4U, AIRCRAFT1_WAIT_LINK = 8U,
    AIRCRAFT1_WAIT_MOUNT = 16U, AIRCRAFT1_WAIT_FLOW_AXES = 32U,
    AIRCRAFT1_WAIT_TIMING = 64U
};
unsigned Aircraft1_WaitMask(const HoverSample *sample, unsigned mount_verified,
                           unsigned flow_verified, float dt);
/* Discard an unready START; never store it for automatic later activation.
 * An active controller still receives invalid data and enters FAULT. */
HoverCommand Aircraft1_FilterStart(HoverState state, HoverCommand command, unsigned wait);
/* Same proper rotation for specific force and bias-corrected angular velocity.
 * Input and output must be distinct vectors. */
void Aircraft1_ImuToBody(const float sensor[3], float body[3]);
void Aircraft1_MapMotors(const float quad[4], float board[4]);
#endif
