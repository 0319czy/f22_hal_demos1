#include "flow_capture.h"
#include <math.h>
#include <string.h>
void FlowCapture_Init(FlowCapture *c) { memset(c, 0, sizeof(*c)); }
void FlowCapture_Break(FlowCapture *c) { ++c->epoch; }
static void FlowCapture_RollWindow(FlowCapture *c)
{
    ++c->epoch;
    c->raw_samples = c->accepted = c->rejected = c->io_errors = 0U;
    c->raw_dx = c->raw_dy = c->dx = c->dy = 0;
    c->seconds = c->hx = c->hy = 0.f;
    memset(c->angle, 0, sizeof(c->angle));
    memset(c->hangle, 0, sizeof(c->hangle));
    c->invalid_reasons = c->saturated = 0U;
}
float FlowCapture_ProjectHeight(float range_m, float roll_deg, float pitch_deg)
{
    if (!isfinite(range_m) || range_m < .04f || range_m > 4.f ||
        !isfinite(roll_deg) || !isfinite(pitch_deg) ||
        fabsf(roll_deg) > 30.f || fabsf(pitch_deg) > 30.f) return NAN;
    const float rad = .0174532925199433f;
    return range_m * cosf(roll_deg * rad) * cosf(pitch_deg * rad);
}
void FlowCapture_Add(FlowCapture *c, const FlowCaptureSample *s)
{
    unsigned reason = 0U;
    /* raw_dx/raw_dy are signed 32-bit sums of int16 sensor windows. Rather
     * than saturating once after several minutes (and then calling every
     * later sample "bad"), segment the cumulative telemetry before the safe
     * bound. Trial endpoints on opposite sides are rejected by epoch checks. */
    if (c->raw_samples >= FLOW_CAPTURE_WINDOW_LIMIT ||
        c->accepted >= FLOW_CAPTURE_WINDOW_LIMIT)
        FlowCapture_RollWindow(c);
    ++c->polls; c->last_ms = s->ms;
    if (!s->io_error && c->raw_samples < 65535U) {
        ++c->raw_samples; c->raw_dx += s->dx; c->raw_dy += s->dy;
    } else if (c->raw_samples >= 65535U) {
        c->saturated = 1U; reason |= 32U;
    }
    if (s->io_error) { ++c->io_errors; reason |= 1U; }
    if (!s->flow_ok) reason |= 2U;
    if (!s->range_ok || !isfinite(s->height_m) || s->height_m < .08f || s->height_m > 4.f) reason |= 4U;
    if (!s->imu_ok) reason |= 8U;
    if (!s->same_window || !isfinite(s->dt) || s->dt <= 0.f || s->dt > .05f) reason |= 16U;
    for (unsigned i = 0; i < 3; ++i) if (!isfinite(s->gyro_dps[i])) reason |= 8U;
    /* At most 65535 int16 samples: signed raw sums cannot overflow int32.
     * Saturation is explicit; never wrap a calibration cumulative value. */
    if (c->accepted >= 65535U) { c->saturated = 1U; reason |= 32U; }
    if (reason) { ++c->rejected; c->invalid_reasons |= reason; return; }
    ++c->accepted; c->dx += s->dx; c->dy += s->dy;
    c->seconds += s->dt;
    c->hx += s->height_m * s->dx; c->hy += s->height_m * s->dy;
    for (unsigned i = 0; i < 3; ++i) {
        float angle = s->gyro_dps[i] * .0174532925199433f * s->dt;
        c->angle[i] += angle; c->hangle[i] += s->height_m * angle;
    }
}
