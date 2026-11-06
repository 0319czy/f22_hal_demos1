#ifndef RADIO_TRACE_H
#define RADIO_TRACE_H
#include <stdint.h>

/* v2/v3: A5 5A version seqLE16 cmd keys tx_msLE32 CRC8.
 * v3 keys: low nibble key mask, high nibble per-press token. */
#define RADIO_TRACE_SIZE 12U
#define RADIO_TRACE_BYTE_TIMEOUT_MS 250U
typedef struct {
    uint8_t bytes[RADIO_TRACE_SIZE], used;
    uint32_t last_byte_ms, rejected;
    uint32_t crc_failures, timeouts, timeout_bytes, max_partial_gap_ms, last_bad_ms;
    uint8_t last_bad[RADIO_TRACE_SIZE], last_bad_size;
} RadioTrace;
typedef struct {
    uint8_t version, command, keys, event_token, size, raw[RADIO_TRACE_SIZE];
    uint16_t sequence;
    uint32_t tx_ms, rx_ms;
} RadioTraceEvent;
void RadioTrace_Init(RadioTrace *trace);
unsigned RadioTrace_Feed(RadioTrace *trace, uint8_t byte, uint32_t received_ms,
                        RadioTraceEvent *event);
#endif
