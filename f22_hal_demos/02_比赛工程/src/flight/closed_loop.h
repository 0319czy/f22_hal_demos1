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
    CL_FLOW_UNVERIFIED, CL_FLOW_UNAVAILABLE, CL_FLIGHT_INHIBITED
} ClosedLoopFault;
typedef struct {
    float hover_duty, max_duty, target_range_m;
    unsigned imu_axes_verified, use_flow;
} ClosedLoopConfig;
typedef struct {
    float rate_kp, rate_ki, roll_limit, pitch_yaw_limit, rate_integral_limit;
    float angle_to_rate_gain, rate_ref_limit_dps;
    float left_trim, front_trim, yaw_trim, collective_trim;
    float target_range_m, height_to_vz_gain, vz_target_limit_mps;
    float vertical_kp, vertical_ki, vertical_limit, vertical_integral_limit;
    float collective_slew_up, collective_slew_down;
    float land_collective_slew_down, land_final_fade_per_sec;
    float takeoff_range_m, ground_clearance_margin_m, target_range_slew;
    float launch_vz_target_limit, launch_trim_slew, launch_trim_limit;
} ClosedLoopTuning;
typedef struct {
    float roll, pitch, yaw, gyro[3], range_m, vz, vx, vy;
    /* tof_fresh accepts recent status9 blind-zone results (including 0..4mm);
     * it does not grant valid height or ground-contact evidence. */
    unsigned imu_valid, link_valid, tof_present, tof_fresh, range_valid, flow_valid;
    /* Fresh raw ToF status with a 5..39mm reading: outside the fused-height
     * policy range, but usable as near-ground evidence during landing. */
    unsigned tof_near_ground;
    unsigned output_fault;
    unsigned flow_present, flow_calibrated, flow_control_provisional;
    float flow_x, flow_y;
    /* 加速度计原始比力（单位 g，机体系 x 前 / y 右 / z 上）。
     * 用于和光流做互补滤波估计水平速度：加速度补高频，光流压低频漂移。 */
    float accel[3];
} ClosedLoopSample;
typedef struct {
    ClosedLoopState state;
    ClosedLoopFault fault;
    ClosedLoopConfig config;
    ClosedLoopTuning tuning;
    FlightPid rate[3], vertical;
    float seconds, target_range, collective, near_ground_seconds;
    /* ToF 证据断档累计时长（秒），配合 CL_TOF_GAP_GRACE_SEC 宽限判决。 */
    float tof_gap_s;
    /* 垂直速度低通值：ToF 差分得到的 vz 噪声大，直接进控制律会让油门
     * 在 0.1 秒内跳 ±100‰（实测），高度必然振荡。 */
    float vz_f;
    /* 重心偏置在线学习：飞行中按姿态残差积累的差动前馈（roll/pitch），
     * 直接叠加到四路电机上，抵消固定力矩偏置。 */
    float learn_trim_roll, learn_trim_pitch;
    /* 地面零位（度）：起飞瞬间记录的 roll/pitch，作为姿态控制的 0 基准。
     * 用于吃掉 IMU/机架安装倾斜——2026-10-28 实测静止放平时 roll≈+2.6°，
     * 导致"水平"悬停时机体实际倾斜、推力被投到横向而持续漂移。 */
    float roll_zero_deg, pitch_zero_deg;
    /* 互补滤波水平速度估计：加速度积分（高频）+ 光流校正（低频）。
     * ins_v* 是估计速度，ins_b* 是加速度零偏/倾角泄漏的在线估计。 */
    float ins_vx, ins_vy, ins_bx, ins_by;
    unsigned ins_ready;
    float launch_collective_base, launch_trim;
    float ground_range;
    float height_feedforward, last_mix_collective;
    unsigned flow_engaged, ground_range_known;
    unsigned link_lost_landing;
    unsigned landing_flare;
    unsigned height_floor_landing;
    FlowAssist horizontal;
} ClosedLoop;
typedef struct {
    float motor[4], collective, height_feedforward, target_range;
    float rate_reference[3], rate_correction[3];
    unsigned rate_saturated_mask;
    float flow_gain, flow_roll_target, flow_pitch_target;
    unsigned flow_mode, flow_requested, flow_auto_land;
    unsigned link_lost_landing;
    unsigned height_floor_landing;
    unsigned flow_position_hold;
    float flow_position_error_forward, flow_position_error_right;
    float flow_velocity_target_forward, flow_velocity_target_right;
    ClosedLoopState state; ClosedLoopFault fault;
} ClosedLoopOutput;
/* motor order is M1,M2,M3,M6; inputs FRD, range/vz positive UP. */
void ClosedLoop_Init(ClosedLoop *c, ClosedLoopConfig config);
/* Update one bounded RAM parameter from a host tuning command. */
unsigned ClosedLoop_TuningSet(ClosedLoop *c, const char *name, float value);
unsigned ClosedLoop_ArmLevel(const float body_acc_g[3]);
/* Shared attitude/rate controller and motor mixer for flight and explicit
 * propeller-free bench use. Caller validates inputs and owns all state/guards. */
unsigned ClosedLoop_AttitudeMix(ClosedLoop *c, const ClosedLoopSample *s,
    float roll_target, float pitch_target, float dt, ClosedLoopOutput *out);
void ClosedLoop_Step(ClosedLoop *c, const ClosedLoopSample *s, HoverCommand command,
                     float dt, ClosedLoopOutput *out);
#endif
