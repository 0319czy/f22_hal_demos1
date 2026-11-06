#ifndef F22_CONTEST_CONFIG_H
#define F22_CONTEST_CONFIG_H
/* Keep accepted calibration flags false until scale and rotation tests pass.
 * The separate tethered-flow trial may use clearly marked provisional values. */
#define IMU_MOUNT_VERIFIED 0U
#define FLOW_CALIBRATED 0U
/* SENSOR -> BODY forward/right/down. Level stationary specific force must be -Z. */
static const float imu_mount[9] = {1,0,0, 0,1,0, 0,0,1};
static const FlowCalibration flow_calibration = {
    .radians_per_count = .002131946f,
    /* Provisional signed response from the level aircraft-motion checks:
     * forward=+dy and right=+dx. FLOW_CALIBRATED remains 0 until scale and
     * rotation compensation pass the documented holdout checks. */
    .translation = {0,1,1,0},
    .rotation = {0,0,0,0},
    .calibrated = FLOW_CALIBRATED
};
#endif
