#include "imu_calibration.h"
#include <math.h>
#include <string.h>
void ImuCalibration_Init(ImuCalibration *c) { memset(c, 0, sizeof(*c)); }
unsigned ImuCalibration_CanRetain(ImuCalibrationReason reason)
{
    return reason == IMU_CAL_RATE || reason == IMU_CAL_ACCEL_NORM ||
           reason == IMU_CAL_VARIANCE || reason == IMU_CAL_SPREAD;
}
unsigned ImuCalibration_Add(ImuCalibration *c, const float acc[3], const float gyro[3])
{
    if (c->failed || c->count >= IMU_CALIBRATION_SAMPLES) return 0U;
    float norm = 0.0f;
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(acc[i]) || !isfinite(gyro[i])) {
            c->failed = IMU_CAL_NONFINITE; return 0U;
        }
        if (fabsf(gyro[i]) > 10.0f) { c->failed = IMU_CAL_RATE; return 0U; }
        norm += acc[i] * acc[i];
    }
    if (!isfinite(norm) || norm < .81f || norm > 1.21f) {
        c->failed = IMU_CAL_ACCEL_NORM; return 0U;
    }
    ++c->count;
    for (unsigned i = 0; i < 6; ++i) {
        float value = i < 3 ? acc[i] : gyro[i - 3];
        if (c->count == 1U) c->minimum[i] = c->maximum[i] = value;
        else {
            c->minimum[i] = fminf(c->minimum[i], value);
            c->maximum[i] = fmaxf(c->maximum[i], value);
        }
        float delta = value - c->mean[i];
        c->mean[i] += delta / c->count;
        c->m2[i] += delta * (value - c->mean[i]);
    }
    return 1U;
}
ImuCalibrationReason ImuCalibration_Check(const ImuCalibration *c, unsigned *axis)
{
    *axis = 6U;
    if (c->failed) return (ImuCalibrationReason)c->failed;
    if (c->count != IMU_CALIBRATION_SAMPLES) return IMU_CAL_INCOMPLETE;
    for (unsigned i = 0; i < 6; ++i) {
        float variance = c->m2[i] / c->count;
        float spread = c->maximum[i] - c->minimum[i];
        float limit = i < 3 ? .01f : .15f;
        *axis = i;
        if (!isfinite(variance) || variance < 0 || !isfinite(spread) || !isfinite(c->mean[i]))
            return IMU_CAL_NONFINITE;
        if (variance > limit * limit) return IMU_CAL_VARIANCE;
        if (spread > (i < 3 ? .05f : 1.0f)) return IMU_CAL_SPREAD;
    }
    *axis = 6U;
    return IMU_CAL_OK;
}
unsigned ImuCalibration_Finish(const ImuCalibration *c, float bias[3])
{
    unsigned axis;
    if (ImuCalibration_Check(c, &axis) != IMU_CAL_OK) return 0U;
    for (unsigned i = 0; i < 3; ++i) bias[i] = c->mean[i + 3];
    return 1U;
}
