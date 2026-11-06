#include "closed_loop.h"
#include "closed_loop_config.h"
#include "closed_loop_tuning.h"
#include "aircraft1.h"
#include <math.h>
#include <string.h>
static float Clamp(float v, float low, float high)
{ return fminf(high, fmaxf(low, v)); }
static void ResetPids(ClosedLoop *c)
{
    for (unsigned i = 0; i < 3; ++i) FlightPid_Reset(&c->rate[i]);
    FlightPid_Reset(&c->vertical);
}
static void Fail(ClosedLoop *c, ClosedLoopFault fault)
{ c->state = CL_FAULT; c->fault = fault; c->collective = 0; ResetPids(c); }
static void TuningDefaults(ClosedLoopTuning *t)
{
    t->rate_kp = CL_RATE_KP; t->rate_ki = CL_RATE_KI;
    t->roll_limit = CL_ROLL_DUTY_LIMIT; t->pitch_yaw_limit = CL_RATE_PITCH_YAW_LIMIT;
    t->rate_integral_limit = CL_RATE_INTEGRAL_LIMIT;
    t->angle_to_rate_gain = CL_ANGLE_TO_RATE_GAIN;
    t->rate_ref_limit_dps = CL_RATE_REF_LIMIT_DPS;
    t->left_trim = CL_LEFT_DRIFT_TRIM; t->front_trim = CL_FRONT_DRIFT_TRIM;
    t->yaw_trim = CL_YAW_TRIM; t->collective_trim = CL_COLLECTIVE_TRIM;
    t->target_range_m = CL_TARGET_RANGE_M; t->height_to_vz_gain = CL_HEIGHT_TO_VZ_GAIN;
    t->vz_target_limit_mps = CL_VZ_TARGET_LIMIT_MPS;
    t->vertical_kp = CL_VERTICAL_KP; t->vertical_ki = CL_VERTICAL_KI;
    t->vertical_limit = CL_VERTICAL_LIMIT;
    t->vertical_integral_limit = CL_VERTICAL_INTEGRAL_LIMIT;
    t->collective_slew_up = CL_COLLECTIVE_SLEW_UP;
    t->collective_slew_down = CL_COLLECTIVE_SLEW_DOWN;
    t->land_collective_slew_down = CL_LAND_COLLECTIVE_SLEW_DOWN;
    t->land_final_fade_per_sec = CL_LAND_FINAL_FADE_PER_SEC;
    t->takeoff_range_m = CL_TAKEOFF_RANGE_M;
    t->ground_clearance_margin_m = CL_GROUND_CLEARANCE_MARGIN_M;
    t->target_range_slew = CL_TARGET_RANGE_SLEW;
    t->launch_vz_target_limit = CL_LAUNCH_VZ_TARGET_LIMIT;
    t->launch_trim_slew = CL_LAUNCH_TRIM_SLEW;
    t->launch_trim_limit = CL_LAUNCH_TRIM_LIMIT;
}
static unsigned SetFloat(float *dst, float value, float low, float high)
{
    if (!dst || !isfinite(value) || value < low || value > high) return 0U;
    *dst = value; return 1U;
}
unsigned ClosedLoop_TuningSet(ClosedLoop *c, const char *name, float value)
{
    if (!c || !name) return 0U;
#define SET(n, field, lo, hi) if (!strcmp(name, n)) { if (!SetFloat(&c->tuning.field, value, lo, hi)) return 0U; matched = 1U; }
    unsigned matched = 0U;
    SET("rate_kp", rate_kp, 0.0f, .02f); SET("rate_ki", rate_ki, 0.0f, .01f);
    SET("roll_limit", roll_limit, 0.0f, .20f); SET("pitch_yaw_limit", pitch_yaw_limit, 0.0f, .20f);
    SET("rate_integral_limit", rate_integral_limit, 0.0f, .10f);
    SET("angle_to_rate_gain", angle_to_rate_gain, 0.0f, 15.0f);
    SET("rate_ref_limit_dps", rate_ref_limit_dps, 1.0f, 180.0f);
    SET("left_trim", left_trim, -.10f, .10f); SET("front_trim", front_trim, -.10f, .10f);
    SET("yaw_trim", yaw_trim, -.10f, .10f); SET("collective_trim", collective_trim, -.10f, .10f);
    SET("target_range_m", target_range_m, .04f, 1.0f);
    SET("height_to_vz_gain", height_to_vz_gain, 0.0f, 10.0f);
    SET("vz_target_limit_mps", vz_target_limit_mps, .01f, 1.0f);
    SET("vertical_kp", vertical_kp, 0.0f, 2.0f); SET("vertical_ki", vertical_ki, 0.0f, 1.0f);
    SET("vertical_limit", vertical_limit, 0.0f, .30f);
    SET("vertical_integral_limit", vertical_integral_limit, 0.0f, .30f);
    SET("collective_slew_up", collective_slew_up, .001f, 2.0f);
    SET("collective_slew_down", collective_slew_down, .001f, 2.0f);
    SET("land_collective_slew_down", land_collective_slew_down, .001f, 2.0f);
    SET("land_final_fade_per_sec", land_final_fade_per_sec, .001f, 2.0f);
    SET("takeoff_range_m", takeoff_range_m, .04f, 1.0f);
    SET("ground_clearance_margin_m", ground_clearance_margin_m, 0.0f, .30f);
    SET("target_range_slew", target_range_slew, .001f, 2.0f);
    SET("launch_vz_target_limit", launch_vz_target_limit, .01f, 1.0f);
    SET("launch_trim_slew", launch_trim_slew, .001f, 2.0f);
    SET("launch_trim_limit", launch_trim_limit, 0.0f, .30f);
#undef SET
    if (!matched) return 0U;
    c->config.target_range_m = c->tuning.target_range_m;
    c->rate[0].kp = c->rate[1].kp = c->rate[2].kp = c->tuning.rate_kp;
    c->rate[0].ki = c->rate[1].ki = c->rate[2].ki = c->tuning.rate_ki;
    c->rate[0].limit = c->tuning.roll_limit;
    c->rate[1].limit = c->rate[2].limit = c->tuning.pitch_yaw_limit;
    c->rate[0].integral_limit = c->rate[1].integral_limit = c->rate[2].integral_limit = c->tuning.rate_integral_limit;
    c->vertical.kp = c->tuning.vertical_kp; c->vertical.ki = c->tuning.vertical_ki;
    c->vertical.limit = c->tuning.vertical_limit;
    c->vertical.integral_limit = c->tuning.vertical_integral_limit;
    return 1U;
}
void ClosedLoop_Init(ClosedLoop *c, ClosedLoopConfig cfg)
{
    memset(c, 0, sizeof(*c)); c->config = cfg; TuningDefaults(&c->tuning);
    /* The selected image supplies the initial target; host tests and runtime
     * callers may intentionally provide a different target. */
    c->tuning.target_range_m = cfg.target_range_m;
    c->height_feedforward = cfg.hover_duty;
    for (unsigned i = 0; i < 3; ++i) {
        c->rate[i].kp = c->tuning.rate_kp;
        /* 2026-10-31：偏航需要大积分才能在 0.7 秒内压住 20~40°/s 的自转，
         * 但同样的 KI 用在 roll/pitch 上会让姿态环 5Hz 发散（实测 ±10° 振荡）。
         * 因此偏航单独用 CL_RATE_KI_YAW。 */
        c->rate[i].ki = (i == 2U) ? CL_RATE_KI_YAW : c->tuning.rate_ki;
        c->rate[i].limit = i == 0U ? c->tuning.roll_limit : c->tuning.pitch_yaw_limit;
        c->rate[i].integral_limit = (i == 2U) ? CL_RATE_INTEGRAL_LIMIT_YAW
                                              : c->tuning.rate_integral_limit;
    }
    c->vertical.kp = c->tuning.vertical_kp; c->vertical.ki = c->tuning.vertical_ki;
    c->vertical.limit = c->tuning.vertical_limit; c->vertical.integral_limit = c->tuning.vertical_integral_limit;
}
/* 目标高度下限：原先写死 .12f，导致 8cm 系留试验的目标被当成非法配置
 * （2026-10-27 实机：按 F2 直接 fault=2 CL_INPUT_INVALID，飞机毫无反应）。
 * 下限放到 .06f 仍然高于 ToF 有效高度门限（40mm）的 1.5 倍，
 * 低空试验用它；比赛目标 20～100cm 不受影响。 */
static unsigned ConfigValid(const ClosedLoopConfig *c)
{
    return isfinite(c->hover_duty) && isfinite(c->max_duty) && isfinite(c->target_range_m) &&
        c->hover_duty > 0 && c->hover_duty <= c->max_duty && c->max_duty <= CL_MAX_DUTY &&
        c->target_range_m >= .06f && c->target_range_m <= 1.0f;
}
unsigned ClosedLoop_ArmLevel(const float acc[3])
{
    /* Quaternion starts at identity: an exactly inverted gravity vector has
     * zero cross-product correction. Never rely only on estimated roll/pitch
     * when arming; require direct stationary FRD specific-force evidence. */
    for (unsigned i = 0; i < 3; ++i) if (!isfinite(acc[i])) return 0U;
    float norm = sqrtf(acc[0]*acc[0] + acc[1]*acc[1] + acc[2]*acc[2]);
    return norm >= .90f && norm <= 1.10f && acc[2] < -.85f &&
           fabsf(acc[0]) <= .14f && fabsf(acc[1]) <= .14f;
}
unsigned ClosedLoop_AttitudeMix(ClosedLoop *c, const ClosedLoopSample *s,
    float roll_target, float pitch_target, float dt, ClosedLoopOutput *out)
{
    /* 默认用原始姿态做误差（地面零位见 F2 处说明）。 */
    const float roll_now = s->roll - c->roll_zero_deg;
    const float pitch_now = s->pitch - c->pitch_zero_deg;
    out->rate_reference[0] = Clamp(c->tuning.angle_to_rate_gain*(roll_target-roll_now),
                                   -c->tuning.rate_ref_limit_dps, c->tuning.rate_ref_limit_dps);
    out->rate_reference[1] = Clamp(c->tuning.angle_to_rate_gain*(pitch_target-pitch_now),
                                   -c->tuning.rate_ref_limit_dps, c->tuning.rate_ref_limit_dps);
    out->rate_reference[2] = 0;
    float r = FlightPid_Step(&c->rate[0], out->rate_reference[0], s->gyro[0], dt);
    float p = FlightPid_Step(&c->rate[1], out->rate_reference[1], s->gyro[1], dt);
    float y = FlightPid_Step(&c->rate[2], 0, s->gyro[2], dt);
#ifdef CL_TRIM_LEARN
    /* 重心/安装偏置在线学习（2026-10-29）：
     * 固定力矩偏置会让姿态存在稳态误差（实测 pitch 长期 -6°），比例环为了
     * 抵消它必须保留同样的误差。这里把该误差缓慢积分成"差动前馈"，叠加到
     * 四路电机上，姿态误差就能收敛到 0，飞机水平了就不再往一个方向飘。
     * 仅在空中、且误差在 12° 以内时学习，防止异常姿态把学习值带偏。 */
    const unsigned airborne = (c->state == CL_ASCEND) || (c->state == CL_HOLD);
    const float err_roll = roll_target - roll_now;
    const float err_pitch = pitch_target - pitch_now;
    if (airborne && fabsf(err_roll) < 12.0f && fabsf(err_pitch) < 12.0f) {
        c->learn_trim_roll = Clamp(c->learn_trim_roll +
            CL_TRIM_LEARN_KI * err_roll * dt, -CL_TRIM_LEARN_LIMIT, CL_TRIM_LEARN_LIMIT);
        c->learn_trim_pitch = Clamp(c->learn_trim_pitch +
            CL_TRIM_LEARN_KI * err_pitch * dt, -CL_TRIM_LEARN_LIMIT, CL_TRIM_LEARN_LIMIT);
    }
    r = Clamp(r + c->learn_trim_roll, -c->rate[0].limit, c->rate[0].limit);
    p = Clamp(p + c->learn_trim_pitch, -c->rate[1].limit, c->rate[1].limit);
#endif
    out->rate_saturated_mask = (fabsf(r) >= c->rate[0].limit-.000001f ? 1U : 0U) |
        (fabsf(p) >= c->rate[1].limit-.000001f ? 2U : 0U) |
        (fabsf(y) >= c->rate[2].limit-.000001f ? 4U : 0U);
    /* Bound correction authority during spool-up; collective zero means all zero. */
    float authority = fminf(1, c->collective / CL_ATTITUDE_AUTHORITY_COLLECTIVE);
    r *= authority; p *= authority; y *= authority;
    out->rate_correction[0] = r;
    out->rate_correction[1] = p;
    out->rate_correction[2] = y;
    float quad[4] = {c->collective+r+p+y, c->collective-r+p-y,
                     c->collective-r-p+y, c->collective+r-p-y};
    /* Virtual order is FL=M2, FR=M1, RR=M3, RL=M6. Apply field trims only
     * after spool-up authority is available. Differential trims preserve the
     * total thrust; the collective trim is the explicit up/down adjustment. */
    if (c->collective > CL_ATTITUDE_AUTHORITY_COLLECTIVE) {
        /* left/right: left pair M2/M6 up, right pair M1/M3 down */
        quad[0] += c->tuning.left_trim;
        quad[3] += c->tuning.left_trim;
        quad[1] -= c->tuning.left_trim;
        quad[2] -= c->tuning.left_trim;
        /* front/back: front pair M2/M1 up, rear pair M3/M6 down */
        quad[0] += c->tuning.front_trim;
        quad[1] += c->tuning.front_trim;
        quad[2] -= c->tuning.front_trim;
        quad[3] -= c->tuning.front_trim;
        /* yaw: CCW pair M2/M3 up, CW pair M1/M6 down */
        quad[0] += c->tuning.yaw_trim;
        quad[2] += c->tuning.yaw_trim;
        quad[1] -= c->tuning.yaw_trim;
        quad[3] -= c->tuning.yaw_trim;
        for (unsigned i = 0; i < 4; ++i) quad[i] += c->tuning.collective_trim;
    }
    for (unsigned i = 0; i < 4; ++i) if (!isfinite(quad[i])) return 0U;
#ifdef CL_TETHERED_FLIGHT
    /* Shift common thrust before clipping so high throttle preserves the
     * attitude differential. Max throttle is not four channels pinned high. */
    float low = quad[0], high = quad[0];
    for (unsigned i = 1; i < 4; ++i) {
        low = fminf(low, quad[i]); high = fmaxf(high, quad[i]);
    }
    float span = high - low;
    if (span > c->config.max_duty) {
        for (unsigned i = 0; i < 4; ++i)
            quad[i] = (quad[i] - low) * c->config.max_duty / span;
    } else {
        float shift = Clamp(0, -low, c->config.max_duty - high);
        for (unsigned i = 0; i < 4; ++i) quad[i] += shift;
    }
#endif
    for (unsigned i = 0; i < 4; ++i) {
        if (!isfinite(quad[i])) return 0U;
        quad[i] = Clamp(quad[i], 0, c->config.max_duty);
    }
    Aircraft1_MapMotors(quad, out->motor); out->collective = c->collective;
#ifdef CL_TETHERED_FLIGHT
    out->collective = .25f * (quad[0] + quad[1] + quad[2] + quad[3]);
#endif
    c->last_mix_collective = out->collective;
    return 1U;
}
void ClosedLoop_Step(ClosedLoop *c, const ClosedLoopSample *s, HoverCommand command,
                     float dt, ClosedLoopOutput *out)
{
    memset(out, 0, sizeof(*out));
    if (command == HOVER_STOP) {
        c->state = CL_IDLE; c->fault = CL_OK; c->seconds = c->collective = 0;
        c->launch_collective_base = c->launch_trim = 0.0f;
        c->near_ground_seconds = 0; c->flow_engaged = 0; c->landing_flare = 0;
        c->link_lost_landing = 0U;
        c->height_floor_landing = 0U;
#ifdef CL_FLOW_ASSIST
        FlowAssist_Reset(&c->horizontal);
#endif
        ResetPids(c); goto finish;
    }
#if CL_FLIGHT_OUTPUT_LOCKED
    /* This gate also catches an accidentally restored active controller state.
     * It is independent of sensor, radio, calibration and START readiness. */
    if (command == HOVER_START || (c->state != CL_IDLE && c->state != CL_FAULT)) {
        Fail(c, CL_FLIGHT_INHIBITED); goto finish;
    }
#endif
    /* Ground INVALID is expected while IDLE; it is never synthesized as height zero. */
    if (c->state == CL_IDLE && command != HOVER_START) goto finish;
    if (c->state == CL_FAULT) goto finish;
    if (!ConfigValid(&c->config)) { Fail(c, CL_INPUT_INVALID); goto finish; }
    if (!c->config.imu_axes_verified) { Fail(c, CL_AXES_UNVERIFIED); goto finish; }
    if (s->output_fault) { Fail(c, CL_OUTPUT_FAULT); goto finish; }
    if (!isfinite(dt) || dt <= 0 || dt > .015f) { Fail(c, CL_TIMING); goto finish; }
    unsigned radio_lost = !s->link_valid;
    unsigned fresh_near_ground = s->tof_fresh && s->tof_near_ground;
    unsigned launch_near_ground = fresh_near_ground &&
        (c->state == CL_LAUNCH || (c->state == CL_IDLE && command == HOVER_START)) &&
        s->range_valid && s->range_m < .03f;
    unsigned blind_launch_radio_loss = radio_lost && c->state == CL_LAUNCH &&
                                      !s->range_valid;
    if (radio_lost) {
        if (c->state == CL_IDLE) { Fail(c, CL_LINK_LOST); goto finish; }
        /* Blind-zone launch has no vertical estimate to support an airborne
         * landing. Only cancel it as a ground launch when fresh raw ToF gives
         * near-ground evidence; otherwise report the loss and inhibit output.
         * Once vertical height is observable, radio loss requests a normal
         * controlled landing. Sensor, attitude and output faults still trip. */
        if (blind_launch_radio_loss && !fresh_near_ground) {
            Fail(c, CL_LINK_LOST); goto finish;
        }
        /* Keep the latch through autonomous landing so logs show why it began. */
        c->link_lost_landing = 1U;
        command = HOVER_REQUEST_LAND;
    }
    if (!s->imu_valid || !isfinite(s->roll) || !isfinite(s->pitch) ||
        !isfinite(s->gyro[0]) || !isfinite(s->gyro[1]) || !isfinite(s->gyro[2])) {
        Fail(c, CL_IMU_INVALID); goto finish;
    }
    if (fabsf(s->roll) >= 25 || fabsf(s->pitch) >= 25) { Fail(c, CL_TILT); goto finish; }
    /* 短时 ToF 证据缺失容忍：2026-10-27 实机记录显示，一次 ST 原始状态 4 的坏
     * 转换就会让 tof_fresh=0，旧逻辑当拍即报 CL_TOF_LOST——起飞油门只到 581‰、
     * LAUNCH 仅 9 帧就被停机。现在：器件缺席仍立即判故障；只有"新鲜度断档"
     * 给予 CL_TOF_GAP_GRACE_SEC 的宽限，超过宽限仍无新鲜证据才报 CL_TOF_LOST。
     * 空中已有有效高度(range_valid)时不走这条宽限路径。 */
    if (!s->tof_present) { Fail(c, CL_TOF_LOST); goto finish; }
    if (!s->tof_fresh && !(c->state != CL_IDLE && s->range_valid)) {
        c->tof_gap_s += dt;
        if (c->tof_gap_s >= CL_TOF_GAP_GRACE_SEC) { Fail(c, CL_TOF_LOST); goto finish; }
    } else {
        c->tof_gap_s = 0.0f;
        c->learn_trim_roll = c->learn_trim_pitch = 0.0f;
    }
    unsigned floor_handoff = 0U;
    if (s->range_valid) {
        if (!isfinite(s->range_m) || !isfinite(s->vz) ||
            s->range_m < 0.0f || s->range_m > 1.2f) {
            Fail(c, CL_HEIGHT_INVALID); goto finish;
        }
        if (s->range_m < .03f) {
#ifdef CL_TETHERED_FLIGHT
            /* A short IMU coast can cross below30mm while a fresh raw5..39mm
             * sample independently reports the floor. Commit to terminal
             * landing instead of cutting power or chasing the old height.
             * An upward/unsupported low estimate remains an input fault. */
            floor_handoff = fresh_near_ground &&
                (c->state == CL_ASCEND || c->state == CL_HOLD || c->state == CL_LAND) &&
                (s->vz <= 0.0f || (c->state == CL_LAND && c->landing_flare));
#endif
            /* During launch, a fresh sub-30mm sample is expected ground
             * blind-zone evidence. Keep the bounded spool-up; do not treat
             * it as an airborne height or touchdown event. */
            if (!floor_handoff && !launch_near_ground) {
                Fail(c, CL_HEIGHT_INVALID); goto finish;
            }
        }
    }
    if (c->state == CL_IDLE) {
#ifdef CL_FLOW_ASSIST
        if (c->config.use_flow && !s->flow_present) { Fail(c, CL_FLOW_UNAVAILABLE); goto finish; }
        if (c->config.use_flow && !s->flow_calibrated &&
            !s->flow_control_provisional) { Fail(c, CL_FLOW_UNVERIFIED); goto finish; }
#endif
        if (!s->tof_fresh || !isfinite(s->range_m) ||
            fabsf(s->roll) > 8 || fabsf(s->pitch) > 8 ||
            fabsf(s->gyro[0]) > 10 || fabsf(s->gyro[1]) > 10 || fabsf(s->gyro[2]) > 10 ||
            s->range_m > .12f) { Fail(c, CL_START_TOO_HIGH); goto finish; }
        /* 地面零位默认关闭。2026-10-28 实飞：把地面 -2.5°/+1° 姿态当零位后，
         * 控制器认为"已水平"，实际带着 -4~-6° 下俯持续前飞（"飞到隔壁"），
         * 横向 roll +2~3° 同样造成持续侧移。需要时显式定义 CL_GROUND_ZERO_TILT。 */
#ifdef CL_GROUND_ZERO_TILT
        c->roll_zero_deg = s->roll; c->pitch_zero_deg = s->pitch;
#else
        c->roll_zero_deg = 0.0f; c->pitch_zero_deg = 0.0f;
#endif
        c->state = CL_LAUNCH; c->seconds = c->collective = 0;
        c->launch_collective_base = c->launch_trim = 0.0f;
        c->near_ground_seconds = 0; c->flow_engaged = 0; ResetPids(c);
        c->link_lost_landing = 0U;
        c->height_floor_landing = 0U;
        c->height_feedforward = c->config.hover_duty;
        c->last_mix_collective = 0; c->landing_flare = 0;
        c->target_range = s->range_valid ? s->range_m :
                          (s->tof_fresh && s->range_m >= .04f ? s->range_m : .04f);
#ifdef CL_FLOW_ASSIST
        FlowAssist_Reset(&c->horizontal);
#endif
        c->ground_range_known = s->range_valid || (s->tof_fresh && s->range_m >= .04f);
        c->ground_range = c->ground_range_known ? s->range_m : .04f;
    }
#ifdef CL_FLOW_ASSIST
    if (c->config.use_flow) {
        /* IMU attitude/rate control always runs. The relative flow hold captures
         * its XY reference on first trusted data; below the optical working
         * range it fades out and leaves attitude/vertical loops running. */
        unsigned flow_usable = s->flow_valid;
#if CL_FLOW_BRAKE_ONLY
        /* Do not interpret uncompensated turning image motion as translation.
         * This conservative trial gate is not a rotation calibration. */
        /* 2026-10-28 实机：原门限 8°/3°/s 太严，实测悬停段角速度尖峰 10~35°/s，
         * 刹车几乎从不生效。放宽到 10°/15°/s（偏航已由 yaw_trim 压到 ~0，
         * 旋转图像误判为平移的风险可控）；仍然只做速度阻尼、不追位置。 */
        /* 2026-10-28 实飞（v3 日志带 vx/vy）：悬停段水平速度仅 0.2~0.4 m/s，但
         * 三轴角速度范数被俯仰/横滚抖动顶到 20~35°/s，导致光流采信间歇性丢失。
         * 真正会把"旋转"误判成"平移"的是偏航角速度，所以只限制 gyro_z。 */
        flow_usable = flow_usable && fabsf(s->roll) <= 12.0f && fabsf(s->pitch) <= 12.0f &&
            fabsf(s->gyro[2]) <= CL_FLOW_YAW_GATE_DPS;
#endif
        /* 2026-10-31：以下门限原来只写在"刹车模式"分支里，位置保持模式根本
         * 没有生效（实测偏航 +40°/s 时光流增益仍为 1000）。提到通用路径：
         *  - 偏航角速度大：无旋转补偿，旋转会被误判成平移；
         *  - 垂直速度大：地面快速缩放也会被误判成平移（爬升段 vx 冲到 0.9m/s）。 */
        flow_usable = flow_usable && fabsf(s->gyro[2]) <= CL_FLOW_YAW_GATE_DPS;
        flow_usable = flow_usable && fabsf(s->vz) <= CL_FLOW_VZ_GATE_MPS;
        float fvx = s->vx, fvy = s->vy;
#ifdef CL_INS_VEL_FUSE
        /* 互补滤波：加速度积分给高频、光流压低频漂移。
         * 世界系水平比力（姿态旋转已抵消重力反力）：
         *   a_x = cosθ·f_x + sinθ·f_z ,  a_y = cosφ·f_y − sinφ·f_z */
        {
            const float D = .0174532925f;
            const float ct = cosf(s->pitch * D), stp = sinf(s->pitch * D);
            const float cf = cosf(s->roll * D), srf = sinf(s->roll * D);
            float ax = (ct * s->accel[0] + stp * s->accel[2]) * 9.80665f;
            float ay = (cf * s->accel[1] - srf * s->accel[2]) * 9.80665f;
            float cy = cosf(s->yaw * D), sy = sinf(s->yaw * D);
            /* 旋转到与 vx/vy 一致的水平朝向（vx 沿机头，vy 向右）。 */
            float afwd =  ax * cy + ay * sy;
            float aright = -ax * sy + ay * cy;
            if (!c->ins_ready) {
                c->ins_vx = s->vx; c->ins_vy = s->vy;
                c->ins_bx = 0.0f; c->ins_by = 0.0f; c->ins_ready = 1;
            }
            if (s->range_valid && s->range_m < 0.06f && fabsf(s->vz) < 0.10f) {
                /* 在地面上：速度必然为 0，而此刻测到的水平比力本来就是误差
                 * （倾角泄漏 + 零偏），直接当成零偏学掉，避免起飞前就把速度积歪。 */
                c->ins_vx = 0.0f; c->ins_vy = 0.0f;
                c->ins_bx = afwd; c->ins_by = aright;
            } else {
                c->ins_vx += (afwd - c->ins_bx) * dt;
                c->ins_vy += (aright - c->ins_by) * dt;
            }
            /* 光流可信时才校正（爬升时光流会因地面缩放而失真）。 */
            if (flow_usable && s->flow_valid && isfinite(s->vx) && isfinite(s->vy) &&
                fabsf(s->vz) <= CL_INS_FLOW_VZ_MAX) {
                float ex = s->vx - c->ins_vx;
                float ey = s->vy - c->ins_vy;
                c->ins_vx += CL_INS_K1 * ex * dt;
                c->ins_vy += CL_INS_K1 * ey * dt;
                c->ins_bx += CL_INS_K2 * ex * dt;
                c->ins_by += CL_INS_K2 * ey * dt;
            }
            c->ins_vx = Clamp(c->ins_vx, -CL_INS_VEL_LIMIT, CL_INS_VEL_LIMIT);
            c->ins_vy = Clamp(c->ins_vy, -CL_INS_VEL_LIMIT, CL_INS_VEL_LIMIT);
            fvx = c->ins_vx;
            fvy = c->ins_vy;
        }
#endif
        FlowAssist_Step(&c->horizontal, c->state != CL_LAND || !c->landing_flare,
            s->range_valid, s->range_m, flow_usable, s->flow_x, s->flow_y,
            fvx, fvy, s->yaw, dt);
        if (c->horizontal.request_land) command = HOVER_REQUEST_LAND;
    }
#endif
    if (floor_handoff) command = HOVER_REQUEST_LAND;
    if (command == HOVER_REQUEST_LAND && c->state != CL_LAND) {
        if (c->state == CL_LAUNCH && s->range_valid) {
            /* Use the actually emitted launch demand only for a bumpless
             * transition; it is not learned or labelled as hover throttle.
             * 2026-10-28：起飞斜坡末段油门（930~973‰）明显高于真实悬停
             * （约 700‰），直接当前馈会一路冲高；按比例缩放后再交给积分修正。 */
#ifdef CL_HOVER_FF_FIXED
            /* 2026-10-31：实测悬停油门带 800~830‰，直接固定前馈，避免"按起飞末段
             * 油门缩放"带来的不确定（起飞速段 900+，缩放后仍偏高）。 */
            c->height_feedforward = CL_HOVER_FF_FIXED;
#else
            c->height_feedforward = c->last_mix_collective * CL_HOVER_FF_SCALE;
#endif
        }
#ifdef CL_TETHERED_FLIGHT
        /* A deliberate launch cancel, or a radio loss with raw floor evidence,
         * fades the spool-up monotonically. If height is already observable,
         * use the normal descent controller instead of assuming ground. */
        c->landing_flare = c->state == CL_LAUNCH && !s->range_valid &&
                           (!radio_lost || fresh_near_ground);
#endif
        c->state = CL_LAND; c->seconds = 0; c->near_ground_seconds = 0;
        c->target_range = s->range_valid ? s->range_m : .04f;
        ResetPids(c);
    }
    if (floor_handoff) {
        c->landing_flare = 1U;
        c->height_floor_landing = 1U;
    }
    c->seconds += dt;
    if (c->state == CL_LAUNCH) {
        /* Attitude is already closed loop. Range control starts only after a
         * policy-valid ToF sample arrives; do not wait for the optical-flow
         * 8cm working distance. Below ToF's valid range, keep the bounded ramp. */
        /* 2026-10-28 修正：原来用 config.hover_duty 给斜坡封顶。自从 hover_duty
         * 改为"自学习悬停油门/0.80 兜底"，起飞斜坡被压到 800‰，整机起不来
         * （表现为"越飞越差"）。地面起飞必须允许爬到满油门，悬停油门只作为
         * 高度环前馈使用。 */
        c->launch_collective_base = fminf(c->config.max_duty,
            c->launch_collective_base + CL_LAUNCH_RAMP_DUTY_PER_SEC * dt);
        float takeoff_range = c->tuning.takeoff_range_m;
#ifdef CL_TETHERED_FLIGHT
        /* A valid 9cm ground clearance is not evidence of takeoff. */
        if (c->ground_range_known) {
            float need = c->ground_range + c->tuning.ground_clearance_margin_m;
            /* 2026-10-29：飞机被垫高或 ToF 前方有异物时，ground_range 可达 9cm，
             * 交接门限被抬到 12cm，而机体在原地只能悬到 8~11cm → 卡在 LAUNCH
             * 直到超时（实测 2.5s 一直 LAUNCH）。把门限封顶到 10cm。 */
            if (need > .10f) need = .10f;
            takeoff_range = fmaxf(takeoff_range, need);
        }
#endif
        float launch_trim_target = 0.0f;
        /* Do not feed a sub-40mm floor sample into the airborne height loop. */
        if (s->range_valid && s->range_m >= .04f) {
            /* Keep the launch ramp moving, while using measured height and
             * vertical speed to trim it from the first valid range onward. */
            c->target_range = fminf(takeoff_range, c->target_range + c->tuning.target_range_slew * dt);
            float launch_vz_target = Clamp(c->target_range - s->range_m,
                                           -c->tuning.launch_vz_target_limit, c->tuning.launch_vz_target_limit);
            launch_trim_target = FlightPid_Step(&c->vertical, launch_vz_target, s->vz, dt);
        }
        /* Track a bounded PI correction around the independent launch ramp.
         * Never accumulate an absolute PID output into the ramp every cycle. */
        const float launch_trim_slew = c->tuning.launch_trim_slew * dt;
        c->launch_trim += Clamp(launch_trim_target - c->launch_trim,
                                -launch_trim_slew, launch_trim_slew);
        c->launch_trim = Clamp(c->launch_trim, -c->tuning.launch_trim_limit, c->tuning.launch_trim_limit);
        c->collective = Clamp(c->launch_collective_base + c->launch_trim,
                              0.0f, c->config.max_duty);
        if (s->range_valid && s->range_m >= .04f && s->range_m >= takeoff_range) {
#ifdef CL_TETHERED_FLIGHT
            /* A takeoff reference, not a calibrated equilibrium hover duty.
             * 见 CL_HOVER_FF_SCALE 说明。 */
#ifdef CL_HOVER_LEARNED
            /* 自学习模式：cfg.hover_duty 由上次飞行的 HOLD 平均油门填充，
             * 比"起飞斜坡末段油门"更接近真实悬停。 */
            c->height_feedforward = c->config.hover_duty;
#else
            c->height_feedforward = c->last_mix_collective * CL_HOVER_FF_SCALE;
#endif
#endif
            c->state = CL_ASCEND; c->target_range = s->range_m; c->seconds = 0;
            FlightPid_Reset(&c->vertical);
            /* 2026-10-30：交接时不再把油门压到前馈值。实测起飞末段 868‰、
             * 真实悬停 ~850~880‰，压到前馈 800 会立刻低于悬停而掉高度。
             * 保持型控制从实际起飞油门继续，只增不减。 */
        } else if (c->seconds >= CL_LAUNCH_TIMEOUT_SEC) { Fail(c, CL_LAUNCH_TIMEOUT); goto finish; }
    }
    if (c->state == CL_ASCEND || c->state == CL_HOLD) {
        if (!s->range_valid) { Fail(c, CL_TOF_LOST); goto finish; }
        c->target_range = fminf(c->config.target_range_m, c->target_range + c->tuning.target_range_slew * dt);
        if (c->target_range >= c->config.target_range_m) c->state = CL_HOLD;
    }
    if (c->state == CL_LAND) {
        if (c->seconds >= 8.0f) { Fail(c, CL_LAND_TIMEOUT); goto finish; }
        float floor = c->ground_range_known ? c->ground_range : .04f;
        unsigned can_detect_floor = c->ground_range_known;
#ifdef CL_TETHERED_FLIGHT
        /* Unknown ground clearance can still reach the valid blind-zone edge. */
        can_detect_floor = 1U;
#endif
        /* A fresh raw status in the 5..39mm band is explicit near-ground
         * evidence even though the fused-height policy marks that range invalid. */
        if (s->tof_fresh && s->tof_near_ground) can_detect_floor = 1U;
        c->target_range = fmaxf(floor, c->target_range - .10f * dt);
        unsigned floor_evidence = 0U;
        if (s->range_valid && can_detect_floor && c->target_range <= floor &&
            s->range_m <= floor + .012f && fabsf(s->vz) <= .05f) {
            floor_evidence = 1U;
        } else if (!s->range_valid && can_detect_floor && s->tof_fresh &&
                   s->tof_near_ground && c->target_range <= floor + .012f) {
            /* The fused observer drops below its 40mm policy limit near the
             * floor. Require a fresh raw status/range in the blind-zone too;
             * loss of fused height alone is never touchdown evidence. */
            floor_evidence = 1U;
        }
        if (!c->landing_flare) {
            if (floor_evidence) c->near_ground_seconds += dt;
            else c->near_ground_seconds = 0.0f;
            if (c->near_ground_seconds >= .3f) c->landing_flare = 1U;
        }
        if (c->landing_flare) {
            c->collective = fmaxf(0, c->collective - c->tuning.land_final_fade_per_sec * dt);
            if (c->collective <= 0) {
                c->state = CL_IDLE; ResetPids(c); goto finish;
            }
        }
    }
    if (c->state != CL_LAUNCH && s->range_valid &&
        !(c->state == CL_LAND && (c->near_ground_seconds >= .3f || c->landing_flare))) {
        c->vz_f += (s->vz - c->vz_f) * (dt / (CL_HEIGHT_VZ_TAU + dt));
        float vz_target = Clamp(c->tuning.height_to_vz_gain * (c->target_range - s->range_m),
                                -c->tuning.vz_target_limit_mps, c->tuning.vz_target_limit_mps);
#ifdef CL_HEIGHT_BANG_BANG
        /* 保持型高度控制（2026-10-30）：
         *   低于目标-死区：CL_HEIGHT_FULL_CLIMB 时直接给满油门，否则按斜率缓升
         *   死区内      ：冻结当前油门（不砍不补）
         *   高于目标+死区：按 CL_HEIGHT_DESCEND_SLEW 缓慢下降
         * 目的：不再出现"超调后把油门砍到悬停值以下 → 掉高度"。 */
        (void)vz_target;
        float demanded = c->collective;
        /* 2026-10-31：爬升带着 ~0.5m/s 速度时，按"目标-预判距离"提前收油，
         * 否则惯性会把 22cm 目标冲过 34cm，然后在够不到的高度塌下来。 */
        const float lead = CL_HEIGHT_VZ_LEAD_SEC * c->vz_f;
        const float err_h = (c->target_range - lead) - s->range_m;
        if (err_h > CL_HEIGHT_DEADBAND_M) {
            if (c->vz_f > CL_HEIGHT_VEL_MAX_MPS) {
                /* 已经爬得比上限快：停止加油（避免带速度冲高）。 */
                demanded = c->collective;
            } else {
#ifdef CL_HEIGHT_FULL_CLIMB
            demanded = c->config.max_duty;
#else
            demanded = fminf(c->config.max_duty, c->collective + CL_HEIGHT_CLIMB_SLEW * dt);
#endif
            }
        } else if (c->vz_f < -0.05f) {
            /* 2026-10-30：死区内不再"死冻结"。实测在 49cm 冻住油门后开始下沉，
             * 等掉出死区才反应已来不及（下沉 0.5m/s）。只要检测到下沉趋势就补油门。 */
            demanded = fminf(c->config.max_duty, c->collective + CL_HEIGHT_SINK_SLEW * dt);
        } else if (err_h < -CL_HEIGHT_OVERSHOOT_M) {
            /* 2026-10-30：只在高处且"还没开始下沉"时才砍油门。之前无条件下砍，
             * 从 77cm 一路砍到 805‰ 后越掉越快、1 秒砸到地面（HEIGHT_INVALID）。
             * 现在一旦下沉速度超过 0.10 m/s 就冻结，让高度自然稳住。 */
            if (c->vz_f > -0.03f) {
                /* 2026-10-31：下降分支曾把油门砍到 745‰（50cm 需 ~800）→ 下沉后
                 * 进入下洗气流、满油门也救不回。禁止砍到前馈值以下太多。 */
                /* 2026-10-31：远高于目标时不再受"前馈下限"保护。
                 * 之前下限 764‰ 在 89cm 处仍能继续爬，结果一路冲到 1.2m 上限。 */
                float floor_duty;
                if (s->range_m > CL_HEIGHT_CEIL_M) {
                    /* 接近 100cm 上限：允许减到更低，避免顶到天花板被强制停机。 */
                    floor_duty = CL_HEIGHT_CEIL_FLOOR;
                } else if (err_h < -CL_HEIGHT_OVERSHOOT_M) {
                    floor_duty = CL_HEIGHT_FAR_ABOVE_FLOOR;   /* 高于目标很多：允许减油 */
                } else {
                    floor_duty = c->height_feedforward - CL_HOLD_CUT_LIMIT;
                }
                if (floor_duty < 0.0f) floor_duty = 0.0f;
                demanded = fmaxf(floor_duty, c->collective - CL_HEIGHT_DESCEND_SLEW * dt);
            }
        } else {
            /* 2026-10-31：死区内不再"死冻结油门"。之前到 26cm 时油门 914 被冻结，
             * 结果继续加速冲到 61cm、又在高处塌下来。改为按垂直速度阻尼：
             * 还在爬就减油、开始沉就加油。 */
            demanded = c->collective - CL_HEIGHT_VZ_DAMP * c->vz_f * dt;
            if (demanded < 0.0f) demanded = 0.0f;
            if (demanded > c->config.max_duty) demanded = c->config.max_duty;
        }
#else
        float demanded = Clamp(c->height_feedforward + FlightPid_Step(&c->vertical,
                               vz_target, c->vz_f, dt), 0, c->config.max_duty);
        /* 降落阶段只允许减小油门（安全要求：必须单调收敛到 0）。 */
        if (c->state == CL_LAND && demanded > c->collective) demanded = c->collective;
#endif
#ifdef CL_HOLD_CUT_LIMIT
        /* 2026-10-29：实测"到 30cm 后油门被砍到 556‰，0.4s 内掉回地面"。
         * 限制爬升/悬停阶段相对前馈的最大下砍幅度，超调时不允许把油门
         * 砍到悬停值以下太多。 */
        if (c->state == CL_ASCEND || c->state == CL_HOLD) {
            float floor_duty = c->height_feedforward - CL_HOLD_CUT_LIMIT;
            if (floor_duty < 0.0f) floor_duty = 0.0f;
            if (demanded < floor_duty) demanded = floor_duty;
        }
#endif
        /* Prevent a launch->height transition from jumping to hover throttle. */
        float slew_down = c->state == CL_LAND ?
            c->tuning.land_collective_slew_down : c->tuning.collective_slew_down;
        c->collective += Clamp(demanded - c->collective,
                               -slew_down * dt, c->tuning.collective_slew_up * dt);
    }
    float roll_target = 0, pitch_target = 0;
#ifdef CL_FLOW_ASSIST
    if (c->config.use_flow) {
        roll_target = c->horizontal.roll_target;
        pitch_target = c->horizontal.pitch_target;
        c->flow_engaged = c->horizontal.gain > 0;
    }
#else
    if (c->config.use_flow && (c->state == CL_ASCEND || c->state == CL_HOLD)) {
        if (!s->flow_valid || !isfinite(s->vx) || !isfinite(s->vy)) {
            Fail(c, CL_INPUT_INVALID); goto finish;
        }
        c->flow_engaged = 1;
        /* Body velocity damping; not an absolute XY position hold. */
        roll_target = Clamp(-5.0f * s->vy, -5, 5);
        pitch_target = Clamp(5.0f * s->vx, -5, 5);
    }
#endif
    if (!ClosedLoop_AttitudeMix(c, s, roll_target, pitch_target, dt, out)) {
        Fail(c, CL_INPUT_INVALID); goto finish;
    }
finish:
#ifdef CL_FLOW_ASSIST
    out->flow_gain = c->horizontal.gain;
    out->flow_roll_target = c->horizontal.roll_target;
    out->flow_pitch_target = c->horizontal.pitch_target;
    out->flow_mode = (unsigned)c->horizontal.mode;
    out->flow_requested = c->config.use_flow;
    out->flow_auto_land = c->horizontal.request_land;
    out->flow_position_hold = c->horizontal.position_hold;
    out->flow_position_error_forward = c->horizontal.position_error_forward;
    out->flow_position_error_right = c->horizontal.position_error_right;
    out->flow_velocity_target_forward = c->horizontal.velocity_target_forward;
    out->flow_velocity_target_right = c->horizontal.velocity_target_right;
#endif
    out->height_feedforward = c->height_feedforward;
    out->target_range = c->target_range;
    out->link_lost_landing = c->link_lost_landing;
    out->height_floor_landing = c->height_floor_landing;
    out->state = c->state; out->fault = c->fault;
}
