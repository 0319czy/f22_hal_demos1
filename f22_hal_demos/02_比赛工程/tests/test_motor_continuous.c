#include "motor_continuous.h"
#include "standalone_keys.h"
#include <assert.h>
#include <stdio.h>
static void Expect(const float motor[4], float value)
{ for (unsigned i = 0; i < 4; ++i) assert(motor[i] == value); }
int main(void)
{
    MotorContinuous c; StandaloneKeys keys; float motor[4];
    MotorContinuous_Init(&c); StandaloneKeys_Init(&keys);
    MotorContinuous_Step(&c, HOVER_NONE, 0, motor); Expect(motor, 0);
    /* Holding KEY1 on boot cannot start; release then a new press is required. */
    HoverCommand cmd = StandaloneKeys_Step(&keys, 1, 0, c.state, 0);
    assert(cmd == HOVER_NONE);
    assert(StandaloneKeys_Step(&keys, 1, 0, c.state, 5000) == HOVER_NONE);
    StandaloneKeys_Step(&keys, 0, 0, c.state, 5001);
    StandaloneKeys_Step(&keys, 0, 0, c.state, 5051);
    StandaloneKeys_Step(&keys, 1, 0, c.state, 5060);
    cmd = StandaloneKeys_Step(&keys, 1, 0, c.state, 5090);
    assert(cmd == HOVER_START);
    MotorContinuous_Step(&c, cmd, 0, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    /* Sixty seconds of constant duty: no launch deadline or sensor gating. */
    for (unsigned n = 0; n < 12000; ++n) {
        MotorContinuous_Step(&c, HOVER_NONE, 0, motor);
        assert(c.state == CL_HOLD); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    }
    cmd = StandaloneKeys_Step(&keys, 1, 1, c.state, 65100);
    assert(cmd == HOVER_STOP);
    MotorContinuous_Step(&c, cmd, 1, motor); Expect(motor, 0);
    assert(StandaloneKeys_Step(&keys, 1, 0, c.state, 66000) == HOVER_NONE);
    MotorContinuous_Step(&c, HOVER_START, 0, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    MotorContinuous_Step(&c, HOVER_NONE, 1, motor); Expect(motor, 0);
    MotorContinuous_Step(&c, HOVER_START, 0, motor); Expect(motor, 0);
    MotorContinuous_Step(&c, HOVER_STOP, 0, motor); Expect(motor, 0);
    MotorContinuous_Step(&c, HOVER_START, 0, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    MotorContinuous_Step(&c, HOVER_REQUEST_LAND, 0, motor); Expect(motor, 0);
    puts("Constant motor bench: boot stopped, 60s demand, KEY2 stop, no held restart, output-fault latch, KEY1 toggle passed (synthetic inputs)");
    return 0;
}
