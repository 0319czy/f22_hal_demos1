#include "radio_trace.h"
#include "radio.h"
#include <string.h>

void RadioTrace_Init(RadioTrace *trace) { memset(trace, 0, sizeof(*trace)); }

unsigned RadioTrace_Feed(RadioTrace *trace, uint8_t byte, uint32_t received_ms,
                        RadioTraceEvent *event)
{
    uint32_t gap = (uint32_t)(received_ms - trace->last_byte_ms);
    if (trace->used && gap > trace->max_partial_gap_ms) trace->max_partial_gap_ms = gap;
    /* E49 splits a UART stream into RF packets: a valid application frame can
     * straddle a radio packet boundary. Its inter-byte gap can exceed 30ms.
     * Keep the partial frame across that gap; CRC/header validate it normally.
     * This is independent of the 300ms link freshness check in the caller. */
    if (gap > RADIO_TRACE_BYTE_TIMEOUT_MS && trace->used) {
        trace->timeouts++; trace->timeout_bytes += trace->used;
        trace->used = 0;
    }
    trace->last_byte_ms = received_ms;
    trace->bytes[trace->used++] = byte;
    while (trace->used >= 3U) {
        uint8_t *p = trace->bytes;
        unsigned size = p[2] == 1U ? 6U : RADIO_TRACE_SIZE;
        if (p[0] != 0xa5U || p[1] != 0x5aU || (p[2] != 1U && p[2] != 2U && p[2] != 3U)) {
            trace->rejected++;
        } else if (trace->used < size) {
            return 0;
        } else if (p[p[2] == 1U ? 4U : 5U] <= RADIO_CALIBRATE &&
                   (p[2] != 2U || p[6] <= 15U) &&
                   p[size - 1U] == Radio_Crc(p, size - 1U)) {
            memset(event, 0, sizeof(*event));
            event->version = p[2];
            event->size = (uint8_t)size;
            event->sequence = p[3];
            event->rx_ms = received_ms; /* Final CRC byte's ISR timestamp. */
            if (p[2] == 1U) event->command = p[4];
            else {
                event->sequence |= (uint16_t)p[4] << 8;
                event->command = p[5]; event->keys = p[6] & 15U;
                event->event_token = p[2] == 3U ? p[6] >> 4 : 0U;
                event->tx_ms = (uint32_t)p[7] | (uint32_t)p[8] << 8 |
                               (uint32_t)p[9] << 16 | (uint32_t)p[10] << 24;
            }
            memcpy(event->raw, p, size);
            trace->used = (uint8_t)(trace->used - size);
            memmove(p, p + size, trace->used);
            return 1;
        } else {
            trace->rejected++;
            if (p[size - 1U] != Radio_Crc(p, size - 1U)) {
                trace->crc_failures++; trace->last_bad_ms = received_ms;
                trace->last_bad_size = (uint8_t)size;
                memcpy(trace->last_bad, p, size);
            }
        }
        memmove(p, p + 1, --trace->used);
    }
    return 0;
}
