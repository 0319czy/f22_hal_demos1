#include "hover.h"
#include <math.h>
#include <string.h>
static float Clamp(float x, float low, float high)
{
    return x < low ? low : (x > high ? high : x);
}
static void ResetPids(HoverController *c)
{
    unsigned i;
    FlightPid_Reset(&c->vertical);
    for (i = 0U; i < 3U; ++i) FlightPid_Reset(&c->rate[i]);
}
void Hover_Init(HoverController *c)
{
    unsigned i;
    memset(c, 0, sizeof(*c));
    c->vertical.kp = 0.35f; c->vertical.ki = 0.10f;
    c->vertical.limit = 0.25f; c->vertical.integral_limit = 0.10f;
    for (i = 0U; i < 3U; ++i) {
        c->rate[i].kp = 0.015f; c->rate[i].ki = 0.005f;
        c->rate[i].kd = 0.0003f;
        c->rate[i].limit = 0.15f; c->rate[i].integral_limit = 0.04f;
    }
}
void Hover_MixQuadX(float t, float r, float p, float y, float m[4])
{
    float low, high, shift, span;
    unsigned i;
    if (!isfinite(t) || !isfinite(r) || !isfinite(p) || !isfinite(y)) {
        for (i = 0U; i < 4U; ++i) m[i] = 0.0f;
        return;
    }
    m[0] = r + p + y; m[1] = -r + p - y;
    m[2] = -r - p + y; m[3] = r - p - y;
    for (i = 0U; i < 4U; ++i) if (!isfinite(m[i])) {
        for (i = 0U; i < 4U; ++i) m[i] = 0.0f;
        return;
    }
    low = high = m[0];
    for (i = 1U; i < 4U; ++i) { low = fminf(low, m[i]); high = fmaxf(high, m[i]); }
    span = high - low;
    if (!isfinite(span)) {
        for (i = 0U; i < 4U; ++i) m[i] = 0.0f;
        return;
    }
    if (span > 1.0f) {
        for (i = 0U; i < 4U; ++i) m[i] /= span;
        low /= span; high /= span;
    }
    shift = Clamp(t, -low, 1.0f - high);
    for (i = 0U; i < 4U; ++i) m[i] = Clamp(m[i] + shift, 0.0f, 1.0f);
}
static unsigned Valid(const HoverSample *s)
{
    return s->imu_valid && s->height_valid && s->position_valid && s->command_link_valid &&
        isfinite(s->roll_deg) && isfinite(s->pitch_deg) && isfinite(s->yaw_deg) &&
        isfinite(s->gyro_dps[0]) && isfinite(s->gyro_dps[1]) && isfinite(s->gyro_dps[2]) &&
        isfinite(s->height_m) && isfinite(s->vertical_speed_mps) &&
        isfinite(s->x_m) && isfinite(s->y_m) && isfinite(s->vx_mps) && isfinite(s->vy_mps) &&
        s->height_m >= -0.02f && s->height_m <= 1.0f &&
        fabsf(s->roll_deg) < 35.0f && fabsf(s->pitch_deg) < 35.0f;
}
void Hover_Step(HoverController *c, const HoverSample *s, HoverCommand command,
                float dt, HoverOutput *out)
{
    float yaw, ex, ey, vx_target, vy_target, vz_target, roll_target, pitch_target;
    unsigned i;
    memset(out, 0, sizeof(*out));
    if (command == HOVER_STOP) {
        c->state = HOVER_IDLE;
        c->stable_seconds = c->landed_seconds = 0.0f;
        ResetPids(c);
    } else if (!isfinite(dt) || dt <= 0.0f || dt > 0.05f || !Valid(s)) {
        if (c->state != HOVER_IDLE || command == HOVER_START) c->state = HOVER_FAULT;
        ResetPids(c);
    } else if (c->state == HOVER_IDLE && command == HOVER_START) {
        /* Ground start only. Fault recovery requires STOP then a new START. */
        if (s->height_m > 0.10f || fabsf(s->roll_deg) > 10.0f || fabsf(s->pitch_deg) > 10.0f) {
            c->state = HOVER_FAULT;
        } else {
            c->state = HOVER_TAKEOFF;
            c->height_target_m = fmaxf(0.0f, s->height_m);
            c->x_target_m = s->x_m; c->y_target_m = s->y_m;
            c->stable_seconds = c->landed_seconds = 0.0f;
            ResetPids(c);
        }
    }
    out->state = c->state;
    if (c->state == HOVER_IDLE || c->state == HOVER_FAULT) return;
    if (command == HOVER_REQUEST_LAND) c->state = HOVER_LAND;
    if (c->state == HOVER_TAKEOFF) {
        c->height_target_m = fminf(0.5f, c->height_target_m + 0.2f * dt);
        if (c->height_target_m >= 0.5f && fabsf(s->height_m - 0.5f) < 0.05f)
            c->state = HOVER_HOLD;
    } else if (c->state == HOVER_HOLD) {
        if (fabsf(s->height_m - 0.5f) < 0.05f &&
            hypotf(s->x_m - c->x_target_m, s->y_m - c->y_target_m) < 0.10f &&
            fabsf(s->vertical_speed_mps) < 0.10f &&
            fabsf(s->roll_deg) < 10.0f && fabsf(s->pitch_deg) < 10.0f)
            c->stable_seconds += dt;
        else c->stable_seconds = 0.0f;
        /* Qualification is observable; competition landing requires a command. */
        c->stable_seconds = fminf(c->stable_seconds, 3600.0f);
    }
    if (c->state == HOVER_LAND) {
        c->height_target_m = fmaxf(0.0f, c->height_target_m - 0.15f * dt);
        if (s->height_m < 0.03f && fabsf(s->vertical_speed_mps) < 0.10f)
            c->landed_seconds += dt;
        else c->landed_seconds = 0.0f;
        if (c->height_target_m <= 0.0f && c->landed_seconds >= 0.5f) {
            c->state = HOVER_IDLE; ResetPids(c); out->state = c->state; return;
        }
    }
    vx_target = Clamp(1.0f * (c->x_target_m - s->x_m), -0.4f, 0.4f);
    vy_target = Clamp(1.0f * (c->y_target_m - s->y_m), -0.4f, 0.4f);
    yaw = s->yaw_deg * 0.0174532925f;
    ex = vx_target - s->vx_mps; ey = vy_target - s->vy_mps;
    /* NED body axes: forward/right/down; height and vertical speed positive UP. */
    pitch_target = Clamp(-8.0f * (cosf(yaw) * ex + sinf(yaw) * ey), -10.0f, 10.0f);
    roll_target = Clamp(8.0f * (-sinf(yaw) * ex + cosf(yaw) * ey), -10.0f, 10.0f);
    out->correction[0] = FlightPid_Step(&c->rate[0],
        Clamp(4.0f * (roll_target - s->roll_deg), -60.0f, 60.0f), s->gyro_dps[0], dt);
    out->correction[1] = FlightPid_Step(&c->rate[1],
        Clamp(4.0f * (pitch_target - s->pitch_deg), -60.0f, 60.0f), s->gyro_dps[1], dt);
    /* Rate damping only: no compass-based heading hold is claimed. */
    out->correction[2] = FlightPid_Step(&c->rate[2], 0.0f, s->gyro_dps[2], dt);
    vz_target = Clamp(1.5f * (c->height_target_m - s->height_m), -0.25f, 0.25f);
    out->collective = Clamp(0.45f + FlightPid_Step(&c->vertical,
        vz_target, s->vertical_speed_mps, dt), 0.0f, 0.8f);
    Hover_MixQuadX(out->collective, out->correction[0], out->correction[1],
                   out->correction[2], out->virtual_motor);
    /* Make all emitted numbers finite before presenting the example output. */
    for (i = 0U; i < 4U; ++i) if (!isfinite(out->virtual_motor[i])) {
        c->state = HOVER_FAULT; ResetPids(c); memset(out, 0, sizeof(*out)); break;
    }
    out->state = c->state;
    out->height_target_m = c->height_target_m;
    out->roll_target_deg = roll_target; out->pitch_target_deg = pitch_target;
}
