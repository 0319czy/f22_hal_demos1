#ifndef CLOSED_LOOP_H
#define CLOSED_LOOP_H
#include "hover.h"
#include "flow_assist.h"
#include <stdint.h>
typedef enum { CL_IDLE, CL_LAUNCH, CL_ASCEND, CL_HOLD, CL_LAND, CL_FAULT } ClosedLoopState;
typedef enum {
    CL_OK, CL_AXES_UNVERIFIED, CL_INPUT_INVALID, CL_LINK_LOST, CL_TIMING,
    CL_TILT, CL_TOF_LOST, CL_LAUNCH_TIMEOUT, CL_START_TOO_HIGH, CL_OUTPUT_FAULT,
    CL_LAND_TIMEOUT, CL_IMU_INVALID, CL_HEIGHT_INVALID,
    CL_FLOW_UNVERIFIED, CL_FLOW_UNAVAILABLE
} ClosedLoopFault;
typedef struct {
    float hover_duty, max_duty, target_range_m;
    unsigned imu_axes_verified, use_flow;
} ClosedLoopConfig;
typedef struct {
    float roll, pitch, gyro[3], range_m, vz, vx, vy;
    unsigned imu_valid, link_valid, tof_present, tof_fresh, range_valid, flow_valid;
    unsigned output_fault;
    unsigned flow_present, flow_calibrated;
} ClosedLoopSample;
typedef struct {
    ClosedLoopState state;
    ClosedLoopFault fault;
    ClosedLoopConfig config;
    FlightPid rate[3], vertical;
    float seconds, target_range, collective, near_ground_seconds;
    float ground_range;
    float height_feedforward, last_mix_collective;
    unsigned flow_engaged, ground_range_known;
    unsigned landing_flare;
    FlowAssist horizontal;
} ClosedLoop;
typedef struct {
    float motor[4], collective, height_feedforward, target_range;
    float flow_gain, flow_roll_target, flow_pitch_target;
    unsigned flow_mode, flow_requested, flow_auto_land;
    ClosedLoopState state; ClosedLoopFault fault;
} ClosedLoopOutput;
/* motor order is M1,M2,M3,M6; inputs FRD, range/vz positive UP. */
void ClosedLoop_Init(ClosedLoop *c, ClosedLoopConfig config);
unsigned ClosedLoop_ArmLevel(const float body_acc_g[3]);
/* Shared attitude/rate controller and motor mixer for flight and explicit
 * propeller-free bench use. Caller validates inputs and owns all state/guards. */
unsigned ClosedLoop_AttitudeMix(ClosedLoop *c, const ClosedLoopSample *s,
    float roll_target, float pitch_target, float dt, ClosedLoopOutput *out);
void ClosedLoop_Step(ClosedLoop *c, const ClosedLoopSample *s, HoverCommand command,
                     float dt, ClosedLoopOutput *out);
#endif
