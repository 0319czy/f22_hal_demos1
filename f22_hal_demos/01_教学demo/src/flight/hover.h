#ifndef F22_HOVER_H
#define F22_HOVER_H
#include "pid.h"
typedef enum { HOVER_IDLE, HOVER_TAKEOFF, HOVER_HOLD, HOVER_LAND, HOVER_FAULT } HoverState;
typedef enum { HOVER_NONE, HOVER_START, HOVER_REQUEST_LAND, HOVER_STOP } HoverCommand;
typedef struct {
    float roll_deg, pitch_deg, yaw_deg, gyro_dps[3];
    float height_m, vertical_speed_mps;
    float x_m, y_m, vx_mps, vy_mps;
    unsigned imu_valid, height_valid, position_valid, command_link_valid;
} HoverSample;
typedef struct {
    HoverState state;
    float height_target_m, roll_target_deg, pitch_target_deg;
    float collective, correction[3], virtual_motor[4];
} HoverOutput;
typedef struct {
    HoverState state;
    float height_target_m, x_target_m, y_target_m, stable_seconds, landed_seconds;
    FlightPid vertical, rate[3];
} HoverController;
/* Teaching model only: example gains/hover throttle are NOT aircraft calibration. */
void Hover_Init(HoverController *controller);
void Hover_Step(HoverController *controller, const HoverSample *sample,
                HoverCommand command, float dt, HoverOutput *output);
/* Virtual FL/FR/RR/RL; no correspondence to board M1..M7 has been established. */
void Hover_MixQuadX(float collective, float roll, float pitch, float yaw, float motors[4]);
#endif
