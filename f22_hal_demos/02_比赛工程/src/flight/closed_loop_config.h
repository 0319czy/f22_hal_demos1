#ifndef CLOSED_LOOP_CONFIG_H
#define CLOSED_LOOP_CONFIG_H
#include "closed_loop_profile.h"
#include "closed_loop_tuning.h"
/* Operator confirmed four static tilt/speed directions and physical F4 stop.
 * Only explicit bench/tether profiles use that acceptance; no flight proven. */
#if defined(CL_ATTITUDE_CHECK) || defined(CL_TETHERED_FLIGHT)
#define CL_IMU_AXES_VERIFIED 1U
#else
#define CL_IMU_AXES_VERIFIED 0U
#endif
#define CL_FLOW_AXES_VERIFIED 0U
/* Latest 2026-10-15 motor-labelled check: M1/M3 side down (left high,
 * right low) gives OLD roll-, while nose down gives OLD pitch+.
 * Candidate correction rotates the old horizontal reference by 180 degrees:
 * FRD = -sensorY,-sensorX,-sensorZ. Gravity down and yaw sense are unchanged.
 * Acceleration and angular velocity use this same proper rotation. Earlier
 * mixed-pose reports were ambiguous. Subsequent four-direction bench acceptance
 * is recorded in docs/validation/attitude-check20-2026-10-15/. */
static const float cl_imu_mount[9] = {0,-1,0, -1,0,0, 0,0,-1};
#endif
