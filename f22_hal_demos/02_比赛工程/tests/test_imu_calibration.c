#include "imu_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void Fill(ImuCalibration *c, unsigned motion)
{
    ImuCalibration_Init(c);
    for (unsigned n = 0; n < IMU_CALIBRATION_SAMPLES; ++n) {
        float noise = (n & 1U) ? .04f : -.04f;
        float acc[3] = {0, 0, 1.03f};
        float gyro[3] = {3.4f + noise, .6f - noise, .4f + noise};
        if (motion == 1U) gyro[0] += (n & 1U) ? 2.0f : -2.0f;
        if (motion == 2U) acc[0] = (n & 1U) ? .1f : -.1f;
        if (motion == 3U && n == 50U) gyro[0] += 1.1f;
        if (motion == 4U) gyro[0] += (n & 1U) ? .16f : -.16f; /* std only */
        if (motion == 5U) acc[0] = (n & 1U) ? .011f : -.011f; /* std only */
        if (motion == 6U && n == 50U) acc[0] = .06f; /* spread only */
        assert(ImuCalibration_Add(c, acc, gyro));
    }
}
static void Rejected(const ImuCalibration *c)
{
    float bias[3] = {7, 8, 9};
    assert(!ImuCalibration_Finish(c, bias));
    assert(bias[0] == 7 && bias[1] == 8 && bias[2] == 9);
}
int main(int argc, char **argv)
{
    ImuCalibration c; float bias[3] = {0}; unsigned axis;
    assert(!ImuCalibration_CanRetain(IMU_CAL_OK));
    assert(!ImuCalibration_CanRetain(IMU_CAL_INCOMPLETE));
    assert(!ImuCalibration_CanRetain(IMU_CAL_NONFINITE));
    assert(!ImuCalibration_CanRetain((ImuCalibrationReason)99));
    for (unsigned reason = IMU_CAL_RATE; reason <= IMU_CAL_SPREAD; ++reason)
        assert(ImuCalibration_CanRetain((ImuCalibrationReason)reason));
    Fill(&c, 0); assert(ImuCalibration_Finish(&c, bias));
    assert(fabsf(bias[0] - 3.4f) < .001f);
    assert(fabsf(bias[1] - .6f) < .001f);
    assert(fabsf(bias[2] - .4f) < .001f);
    for (unsigned motion = 1; motion <= 6; ++motion) { Fill(&c, motion); Rejected(&c); }
    Fill(&c, 4);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_VARIANCE && axis == 3U);
    Fill(&c, 5);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_VARIANCE && axis == 0U);
    Fill(&c, 6);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_SPREAD && axis == 0U);
    /* A rejected F1 after a successful calibration must retain measured bias. */
    float accepted[3] = {bias[0], bias[1], bias[2]};
    Fill(&c, 1); assert(!ImuCalibration_Finish(&c, bias));
    for (unsigned i = 0; i < 3; ++i) assert(bias[i] == accepted[i]);
    ImuCalibration_Init(&c); Rejected(&c);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_INCOMPLETE);
    float acc[3] = {0, 0, 1}, gyro[3] = {0, 0, 0};
    for (unsigned n = 0; n < IMU_CALIBRATION_SAMPLES - 1; ++n)
        assert(ImuCalibration_Add(&c, acc, gyro));
    Rejected(&c); /* Incomplete window / failed sensor read must not commit. */
    gyro[0] = NAN; assert(!ImuCalibration_Add(&c, acc, gyro)); Rejected(&c);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_NONFINITE);
    gyro[0] = 0; assert(!ImuCalibration_Add(&c, acc, gyro)); /* failure is sticky */
    ImuCalibration_Init(&c); acc[2] = INFINITY;
    assert(!ImuCalibration_Add(&c, acc, gyro)); Rejected(&c);
    ImuCalibration_Init(&c); acc[2] = .5f;
    assert(!ImuCalibration_Add(&c, acc, gyro)); Rejected(&c);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_ACCEL_NORM);
    ImuCalibration_Init(&c); acc[2] = 1; gyro[0] = 11;
    assert(!ImuCalibration_Add(&c, acc, gyro)); Rejected(&c);
    assert(ImuCalibration_Check(&c, &axis) == IMU_CAL_RATE);
    Fill(&c, 0); assert(ImuCalibration_Finish(&c, bias)); /* Retry clears failed state. */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r"); assert(f);
        char line[1024]; assert(fgets(line, sizeof(line), f));
        ImuCalibration_Init(&c);
        for (unsigned n = 0; n < IMU_CALIBRATION_SAMPLES; ++n) {
            assert(fgets(line, sizeof(line), f));
            assert(sscanf(line, "%f,%f,%f,%f,%f,%f", &acc[0], &acc[1], &acc[2],
                          &gyro[0], &gyro[1], &gyro[2]) == 6);
            gyro[0] += 3.4f; /* Recorded corrected noise + injected measured-size bias. */
            assert(ImuCalibration_Add(&c, acc, gyro));
        }
        assert(fclose(f) == 0); assert(ImuCalibration_Finish(&c, bias));
        assert(fabsf(bias[0] - 3.4f) < .05f);
        puts("recorded IMU noise replay + injected 3.4 deg/s offset passed (host only)");
    }
    puts("IMU calibration passed: stable bias, motion/spike, incomplete/nonfinite, atomic commit");
}
