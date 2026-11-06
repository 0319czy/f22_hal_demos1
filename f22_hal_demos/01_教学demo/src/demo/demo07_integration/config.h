#ifndef F22_INTEGRATION_CONFIG_H
#define F22_INTEGRATION_CONFIG_H
/* Change ONLY after signed-axis and known-distance experiments described in README. */
#define IMU_MOUNT_VERIFIED 0U
#define FLOW_CALIBRATED 0U
/* Row-major orthogonal mapping SENSOR -> BODY. Identity is an UNVERIFIED hypothesis. Level FRD specific force MUST have accZ=-1g. */
static const float imu_mount[9] = {1,0,0, 0,1,0, 0,0,1};
static const FlowCalibration flow_calibration = {
    .radians_per_count = .002131946f, /* manual: .2131946cm/count at 1m, initial guess */
    .translation = {1,0,0,1},
    .rotation = {0,0,0,0}, /* empirical roll/pitch compensation still required */
    .calibrated = FLOW_CALIBRATED
};
#endif
