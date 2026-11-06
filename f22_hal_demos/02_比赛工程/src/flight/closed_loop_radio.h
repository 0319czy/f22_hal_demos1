#ifndef CLOSED_LOOP_RADIO_H
#define CLOSED_LOOP_RADIO_H
#include "radio_trace.h"
#include "radio.h"
#include "hover.h"
typedef struct {
    RadioReceiver radio; /* Common link/statistics view; legacy decoder unused. */
    RadioTrace trace;
    uint16_t sequence;
    uint32_t tx_ms;
    uint8_t keys, previous_command, ready, fault_latched;
    uint8_t version, event_token, event_token_seen;
    HoverCommand pending;
    unsigned calibrate;
    uint32_t pending_ms, calibrate_ms;
    unsigned event_id, event_command, event_allowed, restarts;
} ClosedLoopRadio;
void ClosedLoopRadio_Init(ClosedLoopRadio *input);
void ClosedLoopRadio_Feed(ClosedLoopRadio *input, uint8_t byte, uint32_t arrival_ms);
unsigned ClosedLoopRadio_LinkValid(const ClosedLoopRadio *input, uint32_t now);
HoverCommand ClosedLoopRadio_Take(ClosedLoopRadio *input, uint32_t now);
unsigned ClosedLoopRadio_TakeCalibration(ClosedLoopRadio *input, unsigned idle_and_zero,
                                        uint32_t now);
#endif
