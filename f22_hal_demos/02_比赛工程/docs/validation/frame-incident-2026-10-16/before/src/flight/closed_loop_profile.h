#ifndef CLOSED_LOOP_PROFILE_H
#define CLOSED_LOOP_PROFILE_H
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
#if defined(CL_F4_SOFT_LAND) && !defined(CL_TETHERED_PWM100)
#error "The changed F4/F3 key map requires the explicit 100% tether test profile"
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
#if defined(CL_TETHERED_PWM100) || defined(CL_FLOW_ASSIST)
/* Explicit full-authority candidates; the no-flow profile remains separate. */
#define CL_HOVER_DUTY 1.0f
#define CL_MAX_DUTY 1.0f
#else
#define CL_HOVER_DUTY .90f
#define CL_MAX_DUTY .90f
#endif
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .50f
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
#define CL_LAUNCH_TIMEOUT_SEC 3.0f
#else
#define CL_LAUNCH_TIMEOUT_SEC 2.0f
#endif
#endif
