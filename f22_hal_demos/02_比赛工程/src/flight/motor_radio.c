#include "motor_radio.h"
#include <string.h>
void MotorRadio_Init(MotorRadio *c)
{ memset(c, 0, sizeof(*c)); Radio_Init(&c->receiver); MotorContinuous_Init(&c->motor); }
void MotorRadio_Feed(MotorRadio *c, uint8_t byte, uint32_t arrived)
{
    unsigned seen = c->receiver.seen;
    unsigned linked = Radio_LinkValid(&c->receiver, arrived);
    uint8_t seq = c->receiver.sequence;
    uint32_t prior = c->receiver.last_packet_ms;
    if (!Radio_Feed(&c->receiver, byte, arrived)) return;
    if (seen) {
        c->last_gap_ms = arrived - prior;
        if (c->last_gap_ms > c->max_gap_ms) c->max_gap_ms = c->last_gap_ms;
    }
    if (linked) c->sequence_missing += (uint8_t)(c->receiver.sequence - seq) - 1U;
    switch ((RadioCommand)c->receiver.command) {
    case RADIO_HEARTBEAT:
        if (c->motor.state == CL_IDLE) c->start_allowed = 1;
        break;
    case RADIO_CALIBRATE: /* TLE100 F1/P1 -> START in this dedicated bench. */
        c->start_requests++;
        if (c->pending != HOVER_STOP && c->motor.state == CL_IDLE && c->start_allowed) {
            c->pending = HOVER_START; c->pending_ms = arrived;
        }
        break;
    case RADIO_STOP:
    case RADIO_LAND: /* F3/F4 both stop immediately: there is no landing loop. */
        c->pending = HOVER_STOP; c->pending_ms = arrived; break;
    case RADIO_START: break; /* F2 has no start role in this bench profile. */
    }
}
void MotorRadio_Step(MotorRadio *c, unsigned local_stop, unsigned trip,
                     unsigned rx_error, uint32_t now, float motor[4])
{
    HoverCommand command = c->pending; c->pending = HOVER_NONE; c->clear_output = 0;
    unsigned linked = Radio_LinkValid(&c->receiver, now);
    if (local_stop || command == HOVER_STOP) {
        c->clear_output = 1; c->start_allowed = 0;
        c->stop_reason = local_stop ? 1U : 2U; c->stops++;
        MotorContinuous_Step(&c->motor, HOVER_STOP, 0, motor); return;
    }
    if (rx_error || trip) {
        c->start_allowed = 0; c->stop_reason = rx_error ? 4U : 5U;
        MotorContinuous_Step(&c->motor, HOVER_NONE, 1, motor); return;
    }
    if (!linked) {
        if (c->motor.state == CL_HOLD) {
            c->disconnects++; c->clear_output = 1; c->stop_reason = 3;
            MotorContinuous_Step(&c->motor, HOVER_STOP, 0, motor);
        } else MotorContinuous_Step(&c->motor, HOVER_NONE, 0, motor);
        c->start_allowed = 0; return;
    }
    if (command == HOVER_START) {
        if (!c->start_allowed || (uint32_t)(now - c->pending_ms) >= RADIO_LINK_TIMEOUT_MS)
            command = HOVER_NONE;
        else { c->start_allowed = 0; c->starts++; c->stop_reason = 0; }
    }
    MotorContinuous_Step(&c->motor, command, 0, motor);
}
