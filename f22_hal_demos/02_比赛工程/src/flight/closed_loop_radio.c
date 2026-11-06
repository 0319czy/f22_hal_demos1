#include "closed_loop_radio.h"
#include <string.h>

void ClosedLoopRadio_Init(ClosedLoopRadio *in)
{ memset(in, 0, sizeof(*in)); }

unsigned ClosedLoopRadio_LinkValid(const ClosedLoopRadio *in, uint32_t now)
{ return !in->fault_latched && Radio_LinkValid(&in->radio, now); }

void ClosedLoopRadio_Feed(ClosedLoopRadio *in, uint8_t byte, uint32_t now)
{
    RadioTraceEvent event;
    if (!RadioTrace_Feed(&in->trace, byte, now, &event)) return;
    if (event.version != 2U && event.version != 3U) { in->radio.rejected++; return; }
    unsigned seen = in->radio.seen;
    unsigned live = Radio_LinkValid(&in->radio, now);
    /* Device time is used only to detect reboot, never to calculate RF delay.
     * Unsigned difference permits the normal 32-bit uptime wrap. */
    unsigned restarted = seen && (uint32_t)(event.tx_ms - in->tx_ms) >= 0x80000000UL;
    if (live) {
        uint16_t advance = (uint16_t)(event.sequence - in->sequence);
        if (restarted || event.version != in->version || !advance || advance >= 0x8000U) {
            in->radio.rejected++;
            if (restarted || event.version != in->version) {
                if (!in->fault_latched) in->restarts++;
                in->fault_latched = 1U; in->ready = 0U; in->calibrate = 0U;
                if (in->pending != HOVER_STOP) in->pending = HOVER_NONE;
            }
            /* Never advance the live session frontier or execute an old STOP.
             * Reboot recovery starts only once this session actually expires. */
            return;
        }
    }
    if (seen && (!live || restarted)) {
        if (restarted && !in->fault_latched) in->restarts++;
        in->fault_latched = 1U; in->ready = 0U; in->calibrate = 0U;
        if (in->pending != HOVER_STOP) in->pending = HOVER_NONE;
        in->previous_command = RADIO_HEARTBEAT; in->keys = 0U;
        in->event_token_seen = 0U;
    }
    unsigned bit = event.command == RADIO_CALIBRATE ? 1U :
                   event.command == RADIO_START ? 2U :
                   event.command == RADIO_LAND ? 4U : 8U;
    unsigned new_event = event.command != RADIO_HEARTBEAT &&
        (event.command != in->previous_command || ((event.keys & ~in->keys) & bit));
    if (event.version == 3U && event.command != RADIO_HEARTBEAT) {
        new_event = !in->event_token_seen || event.event_token != in->event_token;
        if (new_event) { in->event_token = event.event_token; in->event_token_seen = 1U; }
    }
    in->version = event.version;
    in->sequence = event.sequence; in->tx_ms = event.tx_ms;
    in->keys = event.keys; in->previous_command = event.command;
    in->radio.seen = 1U; in->radio.sequence = (uint8_t)event.sequence;
    in->radio.command = event.command; in->radio.last_packet_ms = event.rx_ms;
    in->radio.accepted++;
    unsigned allowed = 0U;
#ifdef FLOW_CAL_MARKERS
    /* Flow calibration accepts F1-F4 only as timestamp markers. In particular,
     * F2/F3 must not enqueue START/LAND: the diagnostic firmware is motor locked. */
    if (event.command == RADIO_STOP) {
        in->pending = HOVER_NONE; in->calibrate = 0U;
        in->ready = 0U; in->fault_latched = 0U; allowed = 1U;
    } else if (event.command == RADIO_HEARTBEAT) {
        if (!event.keys && !in->fault_latched) in->ready = 1U;
    } else if (new_event) {
        /* Marker delivery is diagnostic metadata only. A latched radio-ready
         * fault must not suppress a fresh F2/F3 timestamp in the locked probe. */
        allowed = 1U;
    }
#else
    if (event.command == RADIO_STOP) {
        /* Every fresh STOP remains effective even while held or inhibited. */
        in->pending = HOVER_STOP; in->pending_ms = event.rx_ms;
        in->calibrate = 0U; in->ready = 0U; in->fault_latched = 0U; allowed = 1U;
    } else if (event.command == RADIO_HEARTBEAT) {
        if (!event.keys && !in->fault_latched && in->pending != HOVER_STOP)
            in->ready = 1U;
    } else if (new_event && !in->fault_latched) {
        if (event.command == RADIO_LAND) {
            if (in->pending != HOVER_STOP) {
                in->pending = HOVER_REQUEST_LAND; in->pending_ms = event.rx_ms; allowed = 1U;
            }
            in->calibrate = 0U;
        } else if (in->ready && in->pending == HOVER_NONE) {
            if (event.command == RADIO_START) {
                in->pending = HOVER_START; in->pending_ms = event.rx_ms; allowed = 1U;
            } else if (event.command == RADIO_CALIBRATE) {
                in->calibrate = 1U; in->calibrate_ms = event.rx_ms; allowed = 1U;
            }
        }
    }
#endif
    if (new_event) {
        in->event_id++; in->event_command = event.command; in->event_allowed = allowed;
    }
}

HoverCommand ClosedLoopRadio_Take(ClosedLoopRadio *in, uint32_t now)
{
    HoverCommand command = in->pending; in->pending = HOVER_NONE;
    if (command == HOVER_STOP) { in->ready = 0U; in->calibrate = 0U; return command; }
    if (!ClosedLoopRadio_LinkValid(in, now) ||
        (uint32_t)(now - in->pending_ms) >= RADIO_LINK_TIMEOUT_MS) return HOVER_NONE;
    return command;
}

unsigned ClosedLoopRadio_TakeCalibration(ClosedLoopRadio *in, unsigned idle_and_zero,
                                        uint32_t now)
{
    unsigned requested = in->calibrate; in->calibrate = 0U;
    return requested && idle_and_zero && in->pending == HOVER_NONE &&
        ClosedLoopRadio_LinkValid(in, now) &&
        (uint32_t)(now - in->calibrate_ms) < RADIO_LINK_TIMEOUT_MS;
}
