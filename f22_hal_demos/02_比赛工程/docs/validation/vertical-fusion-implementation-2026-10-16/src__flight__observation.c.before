#include "observation.h"
#include <math.h>
#include <string.h>
#define RAD 0.0174532925199433f
void Attitude_Init(Attitude *a) { memset(a, 0, sizeof(*a)); a->q[0] = 1.0f; }
unsigned Attitude_Update(Attitude *a, const float acc[3], const float gyro[3], float dt)
{
    a->valid = 0U;
    if (!isfinite(dt) || dt <= 0 || dt > .05f) { Attitude_Init(a); return 0U; }
    for (unsigned i=0; i<3; ++i) if (!isfinite(acc[i]) || !isfinite(gyro[i])) { Attitude_Init(a); return 0U; }
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
unsigned Height_Update(HeightEstimate *h, float distance, float roll, float pitch, uint32_t now)
{
    h->valid=0U;
    if (!isfinite(distance) || distance<.04f || distance>4 || !isfinite(roll) || !isfinite(pitch) ||
        fabsf(roll)>30 || fabsf(pitch)>30) { h->initialized=0U; return 0U; }
    float height=distance*cosf(roll*RAD)*cosf(pitch*RAD);
    float dt=(float)(uint32_t)(now-h->last_ms)*.001f;
    if (!h->initialized || dt > .15f) { h->height=height; h->vz=0; }
    else {
        if (dt<=0) return 0U;
        float alpha=dt/(.08f+dt), filtered=h->height+alpha*(height-h->height);
        h->vz += dt/(.15f+dt)*((filtered-h->height)/dt-h->vz);
        h->height=filtered;
    }
    h->last_ms=now; h->initialized=h->valid=1U; return 1U;
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
