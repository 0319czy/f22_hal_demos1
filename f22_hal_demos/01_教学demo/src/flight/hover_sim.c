#include "hover_sim.h"
#include <math.h>
#include <string.h>
void HoverSim_Init(HoverSample *s)
{
    memset(s, 0, sizeof(*s));
    s->imu_valid = s->height_valid = s->position_valid = s->command_link_valid = 1U;
}
void HoverSim_Step(HoverSample *s, const HoverOutput *out, float dt)
{
    float ax, ay, yaw;
    unsigned i;
    if (dt <= 0.0f || dt > 0.05f) return;
    for (i = 0U; i < 3U; ++i)
        s->gyro_dps[i] += (400.0f * out->correction[i] - 2.0f * s->gyro_dps[i]) * dt;
    s->roll_deg += s->gyro_dps[0] * dt;
    s->pitch_deg += s->gyro_dps[1] * dt;
    s->yaw_deg += s->gyro_dps[2] * dt;
    s->vertical_speed_mps += (20.0f * (out->collective - 0.45f) -
                              1.5f * s->vertical_speed_mps) * dt;
    s->height_m += s->vertical_speed_mps * dt;
    if (s->height_m < 0.0f) { s->height_m = 0.0f; s->vertical_speed_mps = 0.0f; }
    yaw = s->yaw_deg * 0.0174532925f;
    ax = -9.81f * sinf(s->pitch_deg * 0.0174532925f);
    ay = 9.81f * sinf(s->roll_deg * 0.0174532925f);
    s->vx_mps += (cosf(yaw) * ax - sinf(yaw) * ay - 0.5f * s->vx_mps) * dt;
    s->vy_mps += (sinf(yaw) * ax + cosf(yaw) * ay - 0.5f * s->vy_mps) * dt;
    s->x_m += s->vx_mps * dt; s->y_m += s->vy_mps * dt;
}
