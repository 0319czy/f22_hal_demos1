/* 推力上限诊断实现（2026-10-29）。
 * 现象：飞控能到 20~25cm，之后"油门越大高度越低"。为区分"动力不足"与
 * "控制环问题"，这里绕开高度环：F2 后油门按固定斜率升到 100%，只做姿态
 * 保持，飞行数据记录会把 (油门, 高度) 曲线完整写进 Flash。
 *   - 高度随油门单调上升，最终到顶不再上升 → 推力到顶（动力问题）
 *   - 高度一路上升没有饱和             → 推力有余量（控制问题）
 * 安全性：高度超过 1.1m 或通电超过 8s 自动平缓收油门；F4 立即停桨。 */
#include "thrust_ramp.h"
#include "closed_loop_profile.h"
#include "closed_loop_tuning.h"
#include "aircraft1_pwm.h"
#include <math.h>
#include <string.h>

#ifndef CL_THRUST_RAMP
#error "Thrust ramp diagnostic requires its explicit build profile"
#endif

#define RAMP_LIMIT_M   1.10f   /* 超过此高度立即收油门，保护机体 */
#define RAMP_TIMEOUT_S 8.0f    /* 最长通电时间 */

void ThrustRamp_Step(ClosedLoop *c, const ClosedLoopSample *s,
                     HoverCommand command, float dt, ClosedLoopOutput *out)
{
    memset(out, 0, sizeof(*out));

    /* 关键顺序：F2 必须先解除上一轮遗留的 PWM 保护锁存，再判断 output_fault。
     * 之前把 Clear 放在故障判断之后，导致带锁存时 F2 永远被判 OUTPUT_FAULT，
     * 电机完全不给电（表现为"这一版飞不起来"）。 */
    if (command == HOVER_START) {
        Aircraft1Pwm_Clear();
        c->state = CL_ASCEND;
        c->seconds = 0;
        c->collective = 0;
        c->fault = CL_OK;
        s = s; /* 下面的故障判断改用本拍实测，不再受旧锁存影响 */
    } else if (command == HOVER_STOP) {
        Aircraft1Pwm_Clear();
        c->state = CL_IDLE; c->fault = CL_OK; c->collective = 0; c->seconds = 0;
        goto done;
    }

    if (c->state == CL_FAULT) goto done;          /* 真实故障：等操作员 F4 */
    if (c->state == CL_IDLE) goto done;           /* 未启动：不转电机     */

    {
        ClosedLoopFault fault = s->output_fault ? CL_OUTPUT_FAULT :
            !s->link_valid ? CL_LINK_LOST :
            (!isfinite(dt) || dt <= 0.0f || dt > .015f) ? CL_TIMING : CL_OK;
        if (fault != CL_OK) {
            c->state = CL_FAULT; c->fault = fault; c->collective = 0;
            goto done;
        }
    }

    c->seconds += dt;
    c->collective = fminf(1.0f, c->collective + CL_THRUST_RAMP_RATE * dt);
    (void)ClosedLoop_AttitudeMix(c, s, 0.0f, 0.0f, dt, out);

    if ((s->range_valid && s->range_m > RAMP_LIMIT_M) || c->seconds > RAMP_TIMEOUT_S) {
        /* 到顶或超时：平缓收油门，等操作员落地 */
        c->collective = fmaxf(0.0f, c->collective - 2.0f * dt);
        (void)ClosedLoop_AttitudeMix(c, s, 0.0f, 0.0f, dt, out);
        if (c->collective <= 0.0f) {
            c->state = CL_IDLE;
            memset(out->motor, 0, sizeof(out->motor));
        }
    }
done:
    out->state = c->state; out->fault = c->fault; out->collective = c->collective;
}
