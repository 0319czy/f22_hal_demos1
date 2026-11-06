#include "observation.h"
#include <math.h>
#include <string.h>
#define RAD 0.0174532925199433f
#define HEIGHT_TOF_GATE_BASE_M 0.10f
#define HEIGHT_TOF_GATE_RATE_MPS 0.5f
#define ATTITUDE_ALIGN_SAMPLES 40U
#define ATTITUDE_ALIGN_MAX_GYRO_DPS 5.0f
#define ATTITUDE_ALIGN_MAX_ACC_VARIANCE_G2 0.0016f
void Attitude_Init(Attitude *a) { memset(a, 0, sizeof(*a)); a->q[0] = 1.0f; }
static void Attitude_AlignmentReset(Attitude *a)
{
    memset(a->align_sum, 0, sizeof(a->align_sum));
    memset(a->align_sum_sq, 0, sizeof(a->align_sum_sq));
    a->align_samples = 0U;
}
static unsigned Attitude_AlignFromGravity(Attitude *a, const float force[3])
{
    float norm = sqrtf(force[0]*force[0] + force[1]*force[1] + force[2]*force[2]);
    if (!isfinite(norm) || norm < .90f || norm > 1.10f) return 0U;
    float fx = force[0] / norm, fy = force[1] / norm, fz = force[2] / norm;
    /* At rest specific force points up in FRD; align it with NED up. Reject
     * inverted/near-inverted starts where gravity alone cannot select yaw. */
    float dot = -fz;
    if (!isfinite(dot) || dot <= 0.0f) return 0U;
    float qw = sqrtf(.5f * (1.0f + dot));
    if (!isfinite(qw) || qw < .01f) return 0U;
    a->q[0] = qw;
    a->q[1] = -fy / (2.0f * qw);
    a->q[2] =  fx / (2.0f * qw);
    a->q[3] = 0.0f;
    norm = sqrtf(a->q[0]*a->q[0] + a->q[1]*a->q[1] +
                 a->q[2]*a->q[2] + a->q[3]*a->q[3]);
    if (!isfinite(norm) || norm < .01f) return 0U;
    for (unsigned i=0U; i<4U; ++i) a->q[i] /= norm;
    float w=a->q[0], x=a->q[1], y=a->q[2], z=a->q[3];
    a->roll=atan2f(2*(w*x+y*z), 1-2*(x*x+y*y))/RAD;
    float sp=2*(w*y-z*x); sp=fminf(1,fmaxf(-1,sp)); a->pitch=asinf(sp)/RAD;
    a->yaw=atan2f(2*(w*z+x*y), 1-2*(y*y+z*z))/RAD;
    a->initialized = 1U;
    a->valid = 1U;
    Attitude_AlignmentReset(a);
    return 1U;
}
unsigned Attitude_Update(Attitude *a, const float acc[3], const float gyro[3], float dt)
{
    a->valid = 0U;
    if (!isfinite(dt) || dt <= 0 || dt > .05f) { Attitude_Init(a); return 0U; }
    for (unsigned i=0; i<3; ++i) if (!isfinite(acc[i]) || !isfinite(gyro[i])) { Attitude_Init(a); return 0U; }
    if (!a->initialized) {
        float acc_norm = sqrtf(acc[0]*acc[0] + acc[1]*acc[1] + acc[2]*acc[2]);
        float gyro_norm = sqrtf(gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]);
        if (!isfinite(acc_norm) || !isfinite(gyro_norm) || acc_norm < .90f ||
            acc_norm > 1.10f || gyro_norm > ATTITUDE_ALIGN_MAX_GYRO_DPS) {
            Attitude_AlignmentReset(a);
            return 0U;
        }
        for (unsigned i=0U; i<3U; ++i) {
            a->align_sum[i] += acc[i];
            a->align_sum_sq[i] += acc[i] * acc[i];
        }
        ++a->align_samples;
        if (a->align_samples < ATTITUDE_ALIGN_SAMPLES) return 0U;
        float mean[3];
        for (unsigned i=0U; i<3U; ++i) {
            mean[i] = a->align_sum[i] / (float)a->align_samples;
            float variance = a->align_sum_sq[i] / (float)a->align_samples - mean[i]*mean[i];
            if (!isfinite(variance) || variance > ATTITUDE_ALIGN_MAX_ACC_VARIANCE_G2) {
                Attitude_AlignmentReset(a);
                return 0U;
            }
        }
        if (!Attitude_AlignFromGravity(a, mean)) {
            Attitude_AlignmentReset(a);
            return 0U;
        }
        return 1U;
    }
    float w=a->q[0], x=a->q[1], y=a->q[2], z=a->q[3];
    float gx=gyro[0]*RAD, gy=gyro[1]*RAD, gz=gyro[2]*RAD;
    float norm=sqrtf(acc[0]*acc[0]+acc[1]*acc[1]+acc[2]*acc[2]);
    if (norm > .75f && norm < 1.25f) {
        /* FRD/NED: stationary specific force is opposite to gravity (accZ=-1g). */
        float ax=-acc[0]/norm, ay=-acc[1]/norm, az=-acc[2]/norm;
        float vx=2*(x*z-w*y), vy=2*(w*x+y*z), vz=w*w-x*x-y*y+z*z;
        gx += 2*(ay*vz-az*vy); gy += 2*(az*vx-ax*vz); gz += 2*(ax*vy-ay*vx);
    }
    float half=.5f*dt;
    a->q[0] += (-x*gx-y*gy-z*gz)*half;
    a->q[1] += (w*gx+y*gz-z*gy)*half;
    a->q[2] += (w*gy-x*gz+z*gx)*half;
    a->q[3] += (w*gz+x*gy-y*gx)*half;
    norm=sqrtf(a->q[0]*a->q[0]+a->q[1]*a->q[1]+a->q[2]*a->q[2]+a->q[3]*a->q[3]);
    if (!isfinite(norm) || norm < .01f) { Attitude_Init(a); return 0U; }
    for (unsigned i=0; i<4; ++i) a->q[i]/=norm;
    w=a->q[0]; x=a->q[1]; y=a->q[2]; z=a->q[3];
    a->roll=atan2f(2*(w*x+y*z), 1-2*(x*x+y*y))/RAD;
    float sp=2*(w*y-z*x); sp=fminf(1,fmaxf(-1,sp)); a->pitch=asinf(sp)/RAD;
    a->yaw=atan2f(2*(w*z+x*y), 1-2*(y*y+z*z))/RAD;
    a->valid=1U; return 1U;
}
void Height_Reset(HeightEstimate *h) { memset(h, 0, sizeof(*h)); }
void Height_Invalidate(HeightEstimate *h)
{
    h->valid = 0U;
    h->coast_valid = 0U;
}
unsigned Height_Predict(HeightEstimate *h, const Attitude *a,
                        const float specific_force_g[3], uint32_t now)
{
    h->valid = 0U;
    if (!h->initialized || !h->coast_valid || !a || !a->valid) return 0U;
    for (unsigned i = 0U; i < 3U; ++i)
        if (!isfinite(specific_force_g[i])) { Height_Invalidate(h); return 0U; }

    uint32_t elapsed_ms = (uint32_t)(now - h->state_ms);
    if (elapsed_ms > 50U) {
        /* A long control-loop gap invalidates inertial coast until ToF corrects it. */
        h->state_ms = now;
        Height_Invalidate(h);
        return 0U;
    }
    if (elapsed_ms != 0U) {
        float dt = elapsed_ms * .001f;
        float w = a->q[0], x = a->q[1], y = a->q[2], z = a->q[3];
        /* Third row of body-to-NED rotation, dotted with body specific force. */
        float specific_down_g = 2.0f * (x*z - w*y) * specific_force_g[0] +
                                2.0f * (w*x + y*z) * specific_force_g[1] +
                                (w*w - x*x - y*y + z*z) * specific_force_g[2];
        /* FRD/NED: gravity is +down and a level stationary accelerometer reads -1g. */
        float accel_up = -(specific_down_g + 1.0f) * 9.80665f;
        if (!isfinite(accel_up) || !isfinite(h->height) || !isfinite(h->vz)) {
            Height_Invalidate(h);
            return 0U;
        }
        h->height += h->vz * dt + .5f * accel_up * dt * dt;
        h->vz += accel_up * dt;
        h->vertical_accel = accel_up;
        h->state_ms = now;
    }
    h->valid = (uint32_t)(now - h->last_ms) <= HEIGHT_PREDICTION_HOLD_MS;
    return h->valid;
}
unsigned Height_Correct(HeightEstimate *h, float distance, float roll, float pitch, uint32_t now)
{
    unsigned prediction_valid = h->valid;
    h->valid=0U;
    if (!isfinite(distance) || distance<.04f || distance>4 || !isfinite(roll) || !isfinite(pitch) ||
        fabsf(roll)>30 || fabsf(pitch)>30) return 0U;
    float height=distance*cosf(roll*RAD)*cosf(pitch*RAD);
    uint32_t elapsed_ms = (uint32_t)(now - h->last_ms);
    if (!h->initialized || !h->coast_valid || !isfinite(h->height) ||
        elapsed_ms > HEIGHT_PREDICTION_HOLD_MS) {
        h->height = h->last_tof_height = height;
        h->vz = h->vertical_accel = 0.0f;
        h->initialized = h->coast_valid = 1U;
    } else {
        if (elapsed_ms == 0U) return 0U;
        float dt = elapsed_ms * .001f;
        float innovation = height - h->height;
        float gate = HEIGHT_TOF_GATE_BASE_M + HEIGHT_TOF_GATE_RATE_MPS * dt;
        if (!isfinite(innovation) || fabsf(innovation) > gate) {
            if (h->tof_innovation_rejects != UINT32_MAX) ++h->tof_innovation_rejects;
            /* Keep the short inertial coast alive after an isolated bad range. */
            h->valid = prediction_valid && elapsed_ms <= HEIGHT_PREDICTION_HOLD_MS;
            return 0U;
        }
        float range_v = (height - h->last_tof_height) / dt;
        if (!isfinite(range_v)) return 0U;
        /* ToF is the low-frequency height reference; IMU supplies fast motion
         * between samples. The velocity correction is bounded to reject a
         * single noisy range difference without adding the old 80/150ms LPFs. */
        range_v = fminf(2.0f, fmaxf(-2.0f, range_v));
        float beta = dt / (.08f + dt);
        h->vz += beta * (range_v - h->vz);
        h->height = height;
        h->last_tof_height = height;
    }
    h->last_ms = h->state_ms = now;
    h->initialized = h->coast_valid = h->valid = 1U;
    return 1U;
}
unsigned Height_Update(HeightEstimate *h, float distance, float roll, float pitch, uint32_t now)
{
    h->valid = 0U;
    if (!isfinite(distance) || distance < .04f || distance > 4.0f ||
        !isfinite(roll) || !isfinite(pitch) || fabsf(roll) > 30.0f || fabsf(pitch) > 30.0f) {
        h->initialized = h->coast_valid = 0U;
        return 0U;
    }
    float measured = distance * cosf(roll * RAD) * cosf(pitch * RAD);
    float dt = (float)(uint32_t)(now - h->last_ms) * .001f;
    if (!h->initialized || dt > .15f) {
        h->height = measured;
        h->vz = 0.0f;
    } else {
        if (dt <= 0.0f) return 0U;
        float alpha = dt / (.08f + dt);
        float filtered = h->height + alpha * (measured - h->height);
        h->vz += dt / (.15f + dt) * ((filtered - h->height) / dt - h->vz);
        h->height = filtered;
    }
    h->last_ms = h->state_ms = now;
    h->last_tof_height = measured;
    h->vertical_accel = 0.0f;
    h->initialized = h->coast_valid = h->valid = 1U;
    return 1U;
}
void FlowEstimate_Reset(FlowEstimate *e) { memset(e,0,sizeof(*e)); }
unsigned FlowEstimate_Update(FlowEstimate *e, const FlowCalibration *c,
    int16_t dx, int16_t dy, float height, const float gyro[3], float yaw, float dt)
{
    e->valid=0U; e->vx=e->vy=0;
    if (!c->calibrated || !isfinite(c->radians_per_count) || c->radians_per_count<=0 ||
        c->radians_per_count>.02f || !isfinite(height) || height<.08f || height>4 ||
        !isfinite(yaw) || !isfinite(dt) || dt<=0 || dt>.05f) return 0U;
    for (unsigned i=0;i<4;i++) if (!isfinite(c->translation[i]) || !isfinite(c->rotation[i])) return 0U;
    for (unsigned i=0;i<3;i++) if (!isfinite(gyro[i])) return 0U;
    float forward=(c->translation[0]*dx+c->translation[1]*dy)*c->radians_per_count;
    float right=(c->translation[2]*dx+c->translation[3]*dy)*c->radians_per_count;
    forward -= (c->rotation[0]*gyro[0]+c->rotation[1]*gyro[1])*RAD*dt;
    right -= (c->rotation[2]*gyro[0]+c->rotation[3]*gyro[1])*RAD*dt;
    float bf=height*forward/dt, br=height*right/dt, angle=yaw*RAD;
    float vx=cosf(angle)*bf-sinf(angle)*br, vy=sinf(angle)*bf+cosf(angle)*br;
    if (!isfinite(vx) || !isfinite(vy) || hypotf(vx,vy)>2) return 0U;
    e->vx=vx; e->vy=vy; e->x+=vx*dt; e->y+=vy*dt;
    e->valid=1U; return 1U;
}
