#ifndef CLOSED_LOOP_PROFILE_H
#define CLOSED_LOOP_PROFILE_H
/* Controller demands and the physical PWM ceiling must use the same profile. */
#ifdef CL_RADIO_POWER_PROFILE
#define CL_HOVER_DUTY .70f
#define CL_MAX_DUTY .70f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .50f
#else
#define CL_HOVER_DUTY .15f
#define CL_MAX_DUTY .20f
#define CL_LAUNCH_RAMP_DUTY_PER_SEC .10f
#endif
#endif
