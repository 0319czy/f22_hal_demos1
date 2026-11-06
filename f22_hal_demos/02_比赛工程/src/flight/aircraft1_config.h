#ifndef AIRCRAFT1_CONFIG_H
#define AIRCRAFT1_CONFIG_H
/* This artifact integrates real sensors/radio and previews M1/M2/M3/M6.
 * Keep physical PWM disabled until axes, ground launch, gains and stop paths
 * have been measured. A working digital optical-flow read is not axis proof. */
#define AIRCRAFT1_PHYSICAL_PWM 0U
#define AIRCRAFT1_PERIOD_MS 5U
#define AIRCRAFT1_SENSOR_PERIOD_MS 20U
#define AIRCRAFT1_FLOW_MAX_AGE_MS 60U
#define AIRCRAFT1_MAX_STEP_MS 15U
#endif
