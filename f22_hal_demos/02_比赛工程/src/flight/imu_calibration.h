#ifndef F22_IMU_CALIBRATION_H
#define F22_IMU_CALIBRATION_H
#define IMU_CALIBRATION_SAMPLES 100U
typedef enum {
    IMU_CAL_OK, IMU_CAL_INCOMPLETE, IMU_CAL_NONFINITE, IMU_CAL_RATE,
    IMU_CAL_ACCEL_NORM, IMU_CAL_VARIANCE, IMU_CAL_SPREAD
} ImuCalibrationReason;
typedef struct {
    unsigned count, failed;
    float mean[6], m2[6], minimum[6], maximum[6];
} ImuCalibration;
void ImuCalibration_Init(ImuCalibration *c);
/* Raw sensor axes: acceleration g, angular velocity deg/s. Keep physically still.
 * Constant slow rotation about gravity cannot be distinguished from gyro bias. */
unsigned ImuCalibration_Add(ImuCalibration *c, const float acc[3], const float gyro[3]);
/* axis: acceleration 0..2, gyro 3..5; 6 when no single axis applies. */
ImuCalibrationReason ImuCalibration_Check(const ImuCalibration *c, unsigned *axis);
/* Only ordinary motion/noise rejection may retain an earlier accepted bias. */
unsigned ImuCalibration_CanRetain(ImuCalibrationReason reason);
/* Commit only a complete stable window; failure leaves caller bias unchanged. */
unsigned ImuCalibration_Finish(const ImuCalibration *c, float bias[3]);
#endif
