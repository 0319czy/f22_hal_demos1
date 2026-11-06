#ifndef F22_PID_H
#define F22_PID_H
typedef struct {
    float kp, ki, kd, limit, integral_limit;
    float integral, previous_measurement, derivative;
    unsigned initialized;
} FlightPid;
void FlightPid_Reset(FlightPid *pid);
float FlightPid_Step(FlightPid *pid, float target, float measurement, float dt);
#endif
