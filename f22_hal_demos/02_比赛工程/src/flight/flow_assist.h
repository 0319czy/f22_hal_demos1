#ifndef F22_FLOW_ASSIST_H
#define F22_FLOW_ASSIST_H
typedef enum {
    FLOW_ASSIST_OFF, FLOW_ASSIST_NEAR_GROUND, FLOW_ASSIST_WAIT_DATA,
    FLOW_ASSIST_BLEND, FLOW_ASSIST_ACTIVE, FLOW_ASSIST_LOST, FLOW_ASSIST_LANDING
} FlowAssistMode;
typedef struct {
    float gain, missing_seconds, roll_target, pitch_target;
    float reference_x, reference_y;
    float position_error_forward, position_error_right;
    float velocity_target_forward, velocity_target_right;
    FlowAssistMode mode;
    unsigned request_land, range_usable, reference_valid, position_hold;
} FlowAssist;
void FlowAssist_Reset(FlowAssist *assist);
/* Relative XY hold around the first trusted optical-flow point. The map is
 * texture only; this does not recognize map coordinates or an absolute origin.
 * range_m is the projected ToF distance proxy; flow validity includes sample
 * age, quality, optical range and calibration checks performed by the caller. */
void FlowAssist_Step(FlowAssist *assist, unsigned active, unsigned usable_range,
                     float range_m, unsigned flow_valid,
                     float x, float y, float vx, float vy, float yaw_deg, float dt);
#endif
