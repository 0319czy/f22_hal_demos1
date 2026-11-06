#ifndef F22_OBSERVATION_H
#define F22_OBSERVATION_H
#include <stdint.h>
typedef struct { float q[4], roll, pitch, yaw; unsigned valid; } Attitude;
void Attitude_Init(Attitude *a);
/* Specific force in g, gyro in deg/s, BODY forward/right/down axes.
 * At rest level accZ=-1g. Quaternion is body-to-local NED. No magnetometer. */
unsigned Attitude_Update(Attitude *a, const float acc[3], const float gyro[3], float dt);
typedef struct { float height, vz; uint32_t last_ms; unsigned valid, initialized; } HeightEstimate;
void Height_Reset(HeightEstimate *h);
unsigned Height_Update(HeightEstimate *h, float distance_m, float roll, float pitch, uint32_t now);
typedef struct {
    float radians_per_count;
    float translation[4]; /* dx/dy -> body forward/right signs and axis swap */
    float rotation[4];    /* roll/pitch angular increments -> image forward/right */
    unsigned calibrated;
} FlowCalibration;
typedef struct { float x, y, vx, vy; unsigned valid; } FlowEstimate;
void FlowEstimate_Reset(FlowEstimate *e);
unsigned FlowEstimate_Update(FlowEstimate *e, const FlowCalibration *c,
    int16_t dx, int16_t dy, float height, const float gyro[3], float yaw, float dt);
#endif
