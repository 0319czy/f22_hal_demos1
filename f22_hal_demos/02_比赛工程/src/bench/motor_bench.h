#ifndef F22_MOTOR_BENCH_H
#define F22_MOTOR_BENCH_H
#include "radio.h"
typedef enum { BENCH_RELEASE, BENCH_IDLE, BENCH_HOLD, BENCH_RUNNING } MotorBenchState;
typedef struct {
    RadioReceiver radio;
    MotorBenchState state;
    uint32_t since_ms;
    unsigned stop_pending;
} MotorBench;
void MotorBench_Init(MotorBench *b);
void MotorBench_Feed(MotorBench *b, uint8_t byte, uint32_t arrival_ms);
/* Only a fresh radio link + released boot keys + 2 s local hold may run.
 * STOP is latched across subsequent heartbeats; max 3 s, then release required. */
unsigned MotorBench_Step(MotorBench *b, unsigned key1, unsigned key2, uint32_t now);
#endif
