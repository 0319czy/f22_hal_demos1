#ifndef THRUST_RAMP_H
#define THRUST_RAMP_H
#include "closed_loop.h"
/* 推力上限诊断：F2 后油门线性升到 100%，只闭合姿态、不做高度控制。
 * 用于回答"满油门到底能飞多高"，区分动力不足与控制问题。 */
void ThrustRamp_Step(ClosedLoop *c, const ClosedLoopSample *sample,
                     HoverCommand command, float dt, ClosedLoopOutput *out);
#endif
