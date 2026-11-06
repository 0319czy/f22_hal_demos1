#include "aircraft1.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static HoverSample Ready(void)
{
    HoverSample s = {0};
    s.imu_valid = s.height_valid = s.position_valid = s.command_link_valid = 1U;
    s.height_m = .09f;
    return s;
}
static void Zero(const HoverOutput *out)
{ for (unsigned i = 0U; i < 4U; ++i) assert(out->virtual_motor[i] == 0.0f); }
int main(void)
{
    HoverSample s = Ready();
    HoverController c; HoverOutput out; Hover_Init(&c);
    unsigned wait = Aircraft1_WaitMask(&s, 1U, 1U, .005f);
    assert(wait == 0U);
    /* Too-close ground samples never become fabricated valid height or a
     * queued START that fires automatically when the aircraft is raised. */
    s.height_valid = s.position_valid = 0U;
    wait = Aircraft1_WaitMask(&s, 1U, 1U, .005f);
    assert(wait == (AIRCRAFT1_WAIT_HEIGHT | AIRCRAFT1_WAIT_FLOW));
    Hover_Step(&c, &s, Aircraft1_FilterStart(c.state, HOVER_START, wait), .005f, &out);
    assert(c.state == HOVER_IDLE); Zero(&out);
    s = Ready(); Hover_Step(&c, &s, HOVER_NONE, .005f, &out);
    assert(c.state == HOVER_IDLE); Zero(&out);
    assert(Aircraft1_WaitMask(&s, 0U, 0U, .005f) ==
           (AIRCRAFT1_WAIT_MOUNT | AIRCRAFT1_WAIT_FLOW_AXES));
    assert(Aircraft1_WaitMask(&s, 1U, 1U, .016f) == AIRCRAFT1_WAIT_TIMING);
    assert(Aircraft1_WaitMask(&s, 1U, 1U, NAN) == AIRCRAFT1_WAIT_TIMING);
    assert(Aircraft1_FilterStart(HOVER_IDLE, HOVER_STOP, 127U) == HOVER_STOP);
    Hover_Step(&c, &s, HOVER_START, .005f, &out);
    assert(c.state == HOVER_TAKEOFF);
    s.command_link_valid = 0U;
    wait = Aircraft1_WaitMask(&s, 1U, 1U, .005f);
    Hover_Step(&c, &s, Aircraft1_FilterStart(c.state, HOVER_NONE, wait), .005f, &out);
    assert(c.state == HOVER_FAULT); Zero(&out);
    s = Ready(); Hover_Step(&c, &s, HOVER_NONE, .005f, &out);
    assert(c.state == HOVER_FAULT); Zero(&out);
    Hover_Step(&c, &s, HOVER_STOP, .005f, &out);
    assert(c.state == HOVER_IDLE); Zero(&out);
    Hover_Step(&c, &s, HOVER_START, .005f, &out);
    s.height_valid = 0U;
    Hover_Step(&c, &s, HOVER_NONE, .005f, &out);
    assert(c.state == HOVER_FAULT); Zero(&out);
    float quad[4] = {.1f, .2f, .3f, .4f}, board[4];
    Aircraft1_MapMotors(quad, board);
    assert(board[AIRCRAFT1_M1] == .2f && board[AIRCRAFT1_M2] == .1f);
    assert(board[AIRCRAFT1_M3] == .3f && board[AIRCRAFT1_M6] == .4f);
    /* FRD positive roll lifts the left motors; positive pitch lifts the front.
     * Positive yaw reaction increases the CCW pair (M3/M2). */
    Hover_MixQuadX(.5f, .1f, 0, 0, quad); Aircraft1_MapMotors(quad, board);
    assert(board[AIRCRAFT1_M2] > board[AIRCRAFT1_M1]);
    assert(board[AIRCRAFT1_M6] > board[AIRCRAFT1_M3]);
    Hover_MixQuadX(.5f, 0, .1f, 0, quad); Aircraft1_MapMotors(quad, board);
    assert(board[AIRCRAFT1_M2] > board[AIRCRAFT1_M6]);
    assert(board[AIRCRAFT1_M1] > board[AIRCRAFT1_M3]);
    Hover_MixQuadX(.5f, 0, 0, .1f, quad); Aircraft1_MapMotors(quad, board);
    assert(board[AIRCRAFT1_M3] > board[AIRCRAFT1_M6]);
    assert(board[AIRCRAFT1_M2] > board[AIRCRAFT1_M1]);
    quad[1] = NAN; Aircraft1_MapMotors(quad, board);
    for (unsigned i = 0U; i < 4U; ++i) assert(board[i] == 0.0f);
    puts("Aircraft1 mapping, ground wait, rearm and failure tests passed (synthetic inputs)");
    return 0;
}
