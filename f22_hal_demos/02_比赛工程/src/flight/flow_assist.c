#include "flow_assist.h"
#include "closed_loop_profile.h"
#include "closed_loop_tuning.h"
#include <math.h>
#include <string.h>
static float Clamp(float x, float low, float high)
{ return fminf(high, fmaxf(low, x)); }
static float Deadband(float x, float threshold)
{ return fabsf(x) <= threshold ? 0.0f : x; }
void FlowAssist_Reset(FlowAssist *a) { memset(a, 0, sizeof(*a)); }
void FlowAssist_Step(FlowAssist *a, unsigned active, unsigned usable_range,
                     float range_m, unsigned flow_valid,
                     float x, float y, float vx, float vy, float yaw_deg, float dt)
{
    if (!isfinite(dt) || dt <= 0 || dt > .015f) {
        /* 启动首拍主循环会用 20ms 兜底 dt，超过光流 15ms 限制。此前无条件置
         * request_land=1，导致"还没起飞就带着降落请求"，一进 ASCEND 就自动降
         * （2026-10-28 实机 1.6s 自动 LAND 的根因）。只有已经处于工作距离或
         * 已经建立光流增益时才请求降落。 */
        unsigned engaged = a->range_usable || a->gain > 0.01f;
        FlowAssist_Reset(a);
        a->mode = FLOW_ASSIST_LOST;
        a->request_land = engaged ? 1U : 0U;
        return;
    }

    /* Enter at the requested 8cm optical boundary. Gain and angle targets
     * ramp, so crossing the threshold cannot step the attitude command. */
    if (!active || !usable_range || !isfinite(range_m) || range_m < CL_FLOW_WORKING_RANGE_M) {
        a->range_usable = 0U;
        a->reference_valid = 0U;
    } else if (!a->range_usable && range_m >= CL_FLOW_WORKING_RANGE_M) {
        a->range_usable = 1U;
    }

    unsigned trusted = active && a->range_usable && flow_valid &&
        isfinite(x) && isfinite(y) && isfinite(vx) && isfinite(vy) &&
        isfinite(yaw_deg) && hypotf(vx, vy) <= 2.0f;
    float wanted_roll = 0.0f, wanted_pitch = 0.0f;
    a->position_hold = 0U;
    a->position_error_forward = a->position_error_right = 0.0f;
    a->velocity_target_forward = a->velocity_target_right = 0.0f;
    if (trusted) {
#if CL_FLOW_BRAKE_ONLY
        /* Experimental velocity damping only. Nominal optical scale is not
         * accepted position calibration: never chase its accumulated XY.
         * Caller separately gates quality and low body angular motion. */
        a->reference_valid = 0U;
        wanted_roll = Clamp(-CL_FLOW_BRAKE_GAIN * Deadband(vy, CL_FLOW_BRAKE_DEADBAND_MPS),
                            -CL_FLOW_BRAKE_ANGLE_LIMIT_DEG, CL_FLOW_BRAKE_ANGLE_LIMIT_DEG);
        wanted_pitch = Clamp(CL_FLOW_BRAKE_GAIN * Deadband(vx, CL_FLOW_BRAKE_DEADBAND_MPS),
                             -CL_FLOW_BRAKE_ANGLE_LIMIT_DEG, CL_FLOW_BRAKE_ANGLE_LIMIT_DEG);
#else
        if (!a->reference_valid) {
            a->reference_x = x;
            a->reference_y = y;
            a->reference_valid = 1U;
        }
        const float radians = yaw_deg * .01745329251994329577f;
        const float c = cosf(radians), s = sinf(radians);
        float error_north = a->reference_x - x;
        float error_east = a->reference_y - y;
        float error_forward = c * error_north + s * error_east;
        float error_right = -s * error_north + c * error_east;
        error_forward = Deadband(error_forward, CL_FLOW_POSITION_DEADBAND_M);
        error_right = Deadband(error_right, CL_FLOW_POSITION_DEADBAND_M);

        /* Position outer loop requests a bounded body velocity. The inner
         * optical-flow velocity loop turns tracking error into small tilt. */
        a->velocity_target_forward = Clamp(CL_FLOW_VELOCITY_GAIN * error_forward,
                                           -CL_FLOW_VELOCITY_LIMIT_MPS, CL_FLOW_VELOCITY_LIMIT_MPS);
        a->velocity_target_right = Clamp(CL_FLOW_VELOCITY_GAIN * error_right,
                                         -CL_FLOW_VELOCITY_LIMIT_MPS, CL_FLOW_VELOCITY_LIMIT_MPS);
        a->position_error_forward = error_forward;
        a->position_error_right = error_right;
        float forward_error = Deadband(vx - a->velocity_target_forward, CL_FLOW_VELOCITY_DEADBAND_MPS);
        float right_error = Deadband(vy - a->velocity_target_right, CL_FLOW_VELOCITY_DEADBAND_MPS);
        wanted_roll = Clamp(-CL_FLOW_ANGLE_GAIN_DEG_PER_MPS * right_error,
                            -CL_FLOW_ANGLE_LIMIT_DEG, CL_FLOW_ANGLE_LIMIT_DEG);
        wanted_pitch = Clamp(CL_FLOW_ANGLE_GAIN_DEG_PER_MPS * forward_error,
                             -CL_FLOW_ANGLE_LIMIT_DEG, CL_FLOW_ANGLE_LIMIT_DEG);
        a->position_hold = 1U;
#endif
    }
    a->gain = Clamp(a->gain + (trusted ? CL_FLOW_GAIN_RAMP_UP_PER_SEC :
                               -CL_FLOW_GAIN_RAMP_DOWN_PER_SEC) * dt, 0, 1);
    /* Both gain and angle demand ramp; no step from level attitude to 5deg. */
    a->roll_target += Clamp(wanted_roll * a->gain - a->roll_target, -10*dt, 10*dt);
    a->pitch_target += Clamp(wanted_pitch * a->gain - a->pitch_target, -10*dt, 10*dt);
    if (!active) {
        a->missing_seconds = 0;
        a->mode = FLOW_ASSIST_LANDING;
    } else if (!a->range_usable) {
        a->missing_seconds = 0;
        a->mode = FLOW_ASSIST_NEAR_GROUND;
    } else if (trusted) {
        a->missing_seconds = 0;
        a->mode = a->gain < .999f ? FLOW_ASSIST_BLEND : FLOW_ASSIST_ACTIVE;
    } else {
        a->missing_seconds += dt;
        a->mode = FLOW_ASSIST_WAIT_DATA;
        if (a->missing_seconds >= CL_FLOW_MISSING_TIMEOUT_SEC) {
            a->request_land = 1U; a->mode = FLOW_ASSIST_LOST;
        }
    }
}
