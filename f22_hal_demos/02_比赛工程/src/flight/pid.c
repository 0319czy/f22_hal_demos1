#include "pid.h"
#include <math.h>
static float Clamp(float x, float limit)
{
    return x > limit ? limit : (x < -limit ? -limit : x);
}
void FlightPid_Reset(FlightPid *pid)
{
    pid->integral = pid->previous_measurement = pid->derivative = 0.0f;
    pid->initialized = 0U;
}
float FlightPid_Step(FlightPid *pid, float target, float measurement, float dt)
{
    float error, integral, output;
    if (!isfinite(target) || !isfinite(measurement) || !isfinite(dt) ||
        dt <= 0.0f || dt > 0.1f) { FlightPid_Reset(pid); return 0.0f; }
    error = target - measurement;
    /* Derivative on measurement avoids a setpoint-change impulse. */
    if (pid->initialized) {
        float raw = -(measurement - pid->previous_measurement) / dt;
        float alpha = dt / (0.03f + dt);
        pid->derivative += alpha * (raw - pid->derivative);
    }
    pid->previous_measurement = measurement;
    pid->initialized = 1U;
    integral = Clamp(pid->integral + pid->ki * error * dt, pid->integral_limit);
    output = pid->kp * error + integral + pid->kd * pid->derivative;
    /* Reject integration that pushes further into output saturation. */
    if (!((output > pid->limit && error > 0.0f) ||
          (output < -pid->limit && error < 0.0f))) pid->integral = integral;
    output = pid->kp * error + pid->integral + pid->kd * pid->derivative;
    return Clamp(output, pid->limit);
}
