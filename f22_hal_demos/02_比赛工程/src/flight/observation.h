#ifndef F22_OBSERVATION_H
#define F22_OBSERVATION_H
#include <stdint.h>
typedef struct {
    float q[4], roll, pitch, yaw;
    float align_sum[3], align_sum_sq[3];
    unsigned align_samples, initialized, valid;
} Attitude;
void Attitude_Init(Attitude *a);
/* Specific force in g, gyro in deg/s, BODY forward/right/down axes.
 * At rest level accZ=-1g. Quaternion is body-to-local NED. No magnetometer. */
unsigned Attitude_Update(Attitude *a, const float acc[3], const float gyro[3], float dt);
#define HEIGHT_PREDICTION_HOLD_MS 100U
typedef struct {
    float height, vz, last_tof_height, vertical_accel;
    uint32_t last_ms, state_ms;
    uint32_t tof_innovation_rejects;
    unsigned valid, initialized, coast_valid;
} HeightEstimate;
void Height_Reset(HeightEstimate *h);
/* Predict height upward from FRD specific force in g and a body-to-NED attitude.
 * Prediction is only trusted for a short interval after a valid ToF update. */
unsigned Height_Predict(HeightEstimate *h, const Attitude *attitude,
                        const float specific_force_g[3], uint32_t now);
void Height_Invalidate(HeightEstimate *h);
/* Correct from a fresh, policy-valid ToF distance in meters for the inertial observer. */
unsigned Height_Correct(HeightEstimate *h, float distance_m, float roll, float pitch, uint32_t now);
/* Legacy ToF-only height/velocity filter retained for non-flight demos. */
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
