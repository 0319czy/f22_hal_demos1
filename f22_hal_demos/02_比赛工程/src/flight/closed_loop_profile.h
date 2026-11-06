#ifndef CLOSED_LOOP_PROFILE_H
#define CLOSED_LOOP_PROFILE_H
#if defined(CL_HOST_REPLAY_ONLY) && (defined(STM32F103xE) || \
    defined(__arm__) || defined(__thumb__) || defined(USE_HAL_DRIVER))
#error "Host replay cannot enable an embedded ARM flight image"
#endif
/* Tether flight output stays locked unless an explicit motor-output image is
 * selected. Provisional flow is only allowed in its dedicated tether image;
 * it does not change the project calibration acceptance flag. */
#ifndef CL_FLOW_PROVISIONAL_CONTROL
#define CL_FLOW_PROVISIONAL_CONTROL 0U
#endif
#ifndef CL_FLOW_BRAKE_ONLY
#define CL_FLOW_BRAKE_ONLY 0U
#endif
#if defined(CL_LOW_HOVER_TEST) && \
    (!defined(CL_TETHERED_FLIGHT) || !defined(CL_TETHERED_PWM100) || \
     !defined(CL_TETHERED_DEBUG_OUTPUT) || defined(CL_FLOW_ASSIST))
#error "Low-hover test requires the explicit no-flow tethered output profile"
#endif
#if CL_FLOW_BRAKE_ONLY && (!defined(CL_FLOW_ASSIST) || !CL_FLOW_PROVISIONAL_CONTROL)
#error "Flow brake trial requires the explicit provisional flow-assist profile"
#endif
/* First powered roll tuning step: double roll authority only. Keep the
 * existing P/I gains, pitch/yaw limits, launch ramp and ceiling unchanged. */
/* 2026-10-30：实测飞行段 roll 长期 +5~8°，光流位置环倾角饱和在 5°，
 * 姿态环也在 4~8% 差动上饱和。允许用 -D 放开横滚权限。 */
#ifndef CL_ROLL_DUTY_LIMIT
#if defined(CL_TETHERED_FLOW_DEBUG_OUTPUT) || defined(CL_TETHERED_DEBUG_OUTPUT)
#define CL_ROLL_DUTY_LIMIT .08f
#else
#define CL_ROLL_DUTY_LIMIT .04f
#endif
#endif
#if CL_FLOW_PROVISIONAL_CONTROL && \
    (!defined(CL_TETHERED_FLIGHT) || !defined(CL_FLOW_ASSIST) || defined(FLOW_ID_PROBE))
#error "Provisional flow requires the tethered flow-assist profile"
#endif
#if defined(CL_TETHERED_DEBUG_OUTPUT) && \
    (!defined(CL_TETHERED_FLIGHT) || !defined(CL_TETHERED_PWM100) || \
     defined(CL_FLOW_ASSIST) || defined(FLOW_ID_PROBE))
#error "Debug motor output requires the explicit no-flow 100% tether profile"
#endif
#if defined(CL_TETHERED_FLOW_DEBUG_OUTPUT) && \
    (!defined(CL_TETHERED_FLIGHT) || !defined(CL_FLOW_ASSIST) || \
     !CL_FLOW_PROVISIONAL_CONTROL || defined(FLOW_ID_PROBE))
#error "Flow motor output requires the explicit tethered provisional-flow profile"
#endif
#if defined(CL_TETHERED_FLIGHT) && !defined(CL_HOST_REPLAY_ONLY) && \
    !defined(CL_TETHERED_DEBUG_OUTPUT) && !defined(CL_TETHERED_FLOW_DEBUG_OUTPUT)
#define CL_FLIGHT_OUTPUT_LOCKED 1U
#else
#define CL_FLIGHT_OUTPUT_LOCKED 0U
#endif
/* Controller demands and the physical PWM ceiling must use the same profile. */
#if defined(CL_DIRECTION_CHECK) && defined(CL_ATTITUDE_CHECK)
#error "Choose one explicit bench profile"
#endif
#if defined(CL_TETHERED_FLIGHT) && (defined(CL_DIRECTION_CHECK) || defined(CL_ATTITUDE_CHECK))
#error "Tethered flight and propeller-free diagnostics are separate profiles"
#endif
#if defined(CL_TETHERED_PWM100) && !defined(CL_TETHERED_FLIGHT)
#error "The 100% PWM profile requires the explicit tethered-flight profile"
#endif
#if defined(CL_TETHERED_PWM100) && defined(CL_FLOW_ASSIST)
#error "Choose either the no-flow 100% test profile or the flow-assist profile"
#endif
#ifdef CL_DIRECTION_CHECK
#define CL_HOVER_DUTY .05f
#define CL_MAX_DUTY .05f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .05f
#elif defined(CL_ATTITUDE_CHECK)
#define CL_HOVER_DUTY .15f
#define CL_MAX_DUTY .20f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .10f
#elif defined(CL_TETHERED_FLIGHT)
/* Launch reference only; height control captures the emitted takeoff demand. */
#if defined(CL_TUNE_LOW_OUTPUT)
/* Low-output direction check: enough to observe the correction direction,
 * deliberately below flight thrust. Restore the full-authority profile only
 * after the low-output attitude test is accepted. */
#define CL_HOVER_DUTY .25f
#define CL_MAX_DUTY .25f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .10f
#elif defined(CL_TETHERED_PWM100) || defined(CL_FLOW_ASSIST)
/* Explicit full-authority candidates; the no-flow profile remains separate. */
#define CL_HOVER_DUTY 1.0f
#define CL_MAX_DUTY 1.0f
#else
#define CL_HOVER_DUTY .90f
#define CL_MAX_DUTY .90f
#endif
#ifndef CL_TUNE_LOW_OUTPUT
#ifndef CL_LAUNCH_RAMP_DUTY_PER_SEC
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .50f
#endif
#endif
#elif defined(CL_RADIO_POWER_PROFILE)
#define CL_HOVER_DUTY .90f
#define CL_MAX_DUTY .90f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .50f
#else
#define CL_HOVER_DUTY .15f
#define CL_MAX_DUTY .20f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .10f
#endif
#ifdef CL_TETHERED_FLIGHT
#ifndef CL_LAUNCH_TIMEOUT_SEC
#define CL_LAUNCH_TIMEOUT_SEC 3.0f
#endif
#else
#ifndef CL_LAUNCH_TIMEOUT_SEC
#define CL_LAUNCH_TIMEOUT_SEC 2.0f
#endif
#endif
#endif
