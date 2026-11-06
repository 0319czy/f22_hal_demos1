#include "motor_bench.h"
#include <string.h>
void MotorBench_Init(MotorBench *b) { memset(b, 0, sizeof(*b)); Radio_Init(&b->radio); }
void MotorBench_Feed(MotorBench *b, uint8_t byte, uint32_t arrival_ms)
{
    unsigned expired = b->radio.seen && !Radio_LinkValid(&b->radio, arrival_ms);
    if (Radio_Feed(&b->radio, byte, arrival_ms)) {
        /* A reconnect cannot hide a missed timeout while the consumer was busy. */
        if (expired) { b->state = BENCH_RELEASE; b->stop_pending = 1U; }
        if (b->radio.command == RADIO_STOP) b->stop_pending = 1U;
    }
}
unsigned MotorBench_Step(MotorBench *b, unsigned key1, unsigned key2, uint32_t now)
{
    unsigned stop = b->stop_pending; b->stop_pending = 0U;
    if (key2 || stop || !Radio_LinkValid(&b->radio, now)) {
        b->state = BENCH_RELEASE; return 0U;
    }
    switch (b->state) {
    case BENCH_RELEASE: if (!key1) b->state = BENCH_IDLE; break;
    case BENCH_IDLE:
        if (key1) { b->state = BENCH_HOLD; b->since_ms = now; }
        break;
    case BENCH_HOLD:
        if (!key1) b->state = BENCH_IDLE;
        else if ((uint32_t)(now - b->since_ms) >= 2000U) {
            b->state = BENCH_RUNNING; b->since_ms = now;
        }
        break;
    case BENCH_RUNNING:
        if (!key1 || (uint32_t)(now - b->since_ms) >= 3000U) b->state = BENCH_RELEASE;
        break;
    default: b->state = BENCH_RELEASE; break;
    }
    return b->state == BENCH_RUNNING ? 15U : 0U;
}
