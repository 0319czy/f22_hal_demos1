#ifndef MOTOR_RADIO_H
#define MOTOR_RADIO_H
#include "radio.h"
#include "motor_continuous.h"
typedef struct {
    RadioReceiver receiver;
    MotorContinuous motor;
    HoverCommand pending;
    uint32_t pending_ms, last_gap_ms, max_gap_ms;
    unsigned start_allowed, clear_output, sequence_missing;
    unsigned start_requests, starts, stops, disconnects, stop_reason;
} MotorRadio;
void MotorRadio_Init(MotorRadio *control);
void MotorRadio_Feed(MotorRadio *control, uint8_t byte, uint32_t arrived_ms);
void MotorRadio_Step(MotorRadio *control, unsigned local_stop,
                     unsigned output_tripped, unsigned rx_error,
                     uint32_t now_ms, float motor[4]);
#endif
