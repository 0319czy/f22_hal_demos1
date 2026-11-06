#include "motor_bench.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint8_t sequence;
static void Packet(MotorBench *b, RadioCommand command, uint32_t now)
{
    uint8_t p[6]; Radio_Encode(++sequence, command, p);
    for (unsigned i = 0; i < 6; ++i) MotorBench_Feed(b, p[i], now);
}
static uint32_t Run(MotorBench *b, uint32_t base)
{
    MotorBench_Init(b); Packet(b, RADIO_HEARTBEAT, base);
    assert(MotorBench_Step(b, 0, 0, base) == 0);
    assert(MotorBench_Step(b, 1, 0, base + 1) == 0);
    for (unsigned elapsed = 101; elapsed < 2001; elapsed += 100) {
        Packet(b, RADIO_HEARTBEAT, base + elapsed);
        assert(MotorBench_Step(b, 1, 0, base + elapsed) == 0);
    }
    Packet(b, RADIO_HEARTBEAT, base + 2001);
    assert(MotorBench_Step(b, 1, 0, base + 2001) == 15);
    return base + 2001;
}
int main(void)
{
    MotorBench b; MotorBench_Init(&b);
    assert(MotorBench_Step(&b, 1, 0, 9999) == 0); /* no link */
    for (unsigned now = 0; now < 4000; now += 100) {
        Packet(&b, RADIO_START, now);
        assert(MotorBench_Step(&b, 1, 0, now) == 0); /* held at boot, RF cannot start */
    }
    uint32_t now = Run(&b, 0);
    assert(MotorBench_Step(&b, 1, 0, now + 299) == 15);
    assert(MotorBench_Step(&b, 1, 0, now + 300) == 0); /* exact link deadline */
    Packet(&b, RADIO_HEARTBEAT, now + 301);
    assert(MotorBench_Step(&b, 1, 0, now + 301) == 0); /* reconnect cannot rearm held key */
    now = Run(&b, 0);
    Packet(&b, RADIO_STOP, now + 1); Packet(&b, RADIO_HEARTBEAT, now + 2);
    assert(MotorBench_Step(&b, 1, 0, now + 2) == 0); /* STOP is not overwritten */
    assert(MotorBench_Step(&b, 1, 0, now + 3) == 0);
    now = Run(&b, 0); assert(MotorBench_Step(&b, 0, 0, now + 1) == 0);
    now = Run(&b, 0); assert(MotorBench_Step(&b, 1, 1, now + 1) == 0);
    now = Run(&b, 0);
    for (unsigned elapsed = 100; elapsed < 2999; elapsed += 100) {
        Packet(&b, RADIO_HEARTBEAT, now + elapsed);
        assert(MotorBench_Step(&b, 1, 0, now + elapsed) == 15);
    }
    Packet(&b, RADIO_HEARTBEAT, now + 2999);
    assert(MotorBench_Step(&b, 1, 0, now + 2999) == 15);
    assert(MotorBench_Step(&b, 1, 0, now + 3000) == 0);
    Packet(&b, RADIO_START, now + 3001);
    assert(MotorBench_Step(&b, 1, 0, now + 3001) == 0);
    now = Run(&b, 0); Packet(&b, RADIO_HEARTBEAT, now + 400);
    assert(MotorBench_Step(&b, 1, 0, now + 400) == 0); /* missed gap + reconnect */
    now = Run(&b, UINT32_MAX - 1000U); /* hold crosses clock wrap */
    assert(MotorBench_Step(&b, 1, 0, now + 300) == 0);
    now = Run(&b, 0);
    MotorBench_Init(&b); /* RX error / queue overflow discards link and arm state */
    Packet(&b, RADIO_HEARTBEAT, now + 1);
    assert(MotorBench_Step(&b, 1, 0, now + 1) == 0);
    now = Run(&b, 0);
    uint8_t old[6]; Radio_Encode(sequence, RADIO_HEARTBEAT, old);
    for (unsigned i = 0; i < 6; ++i) MotorBench_Feed(&b, old[i], now + 299);
    assert(MotorBench_Step(&b, 1, 0, now + 300) == 0); /* replay cannot sustain output */
    now = Run(&b, 0);
    Packet(&b, RADIO_HEARTBEAT, now + 50); /* arrival time, not late drain time */
    assert(MotorBench_Step(&b, 1, 0, now + 350) == 0);
    puts("motor bench host gates passed: boot hold, timeout, STOP latch, release, cap, wrap, RX reset, replay");
}
