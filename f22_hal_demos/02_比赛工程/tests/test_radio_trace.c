#include "radio_trace.h"
#include "radio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned feed(RadioTrace *trace, const uint8_t *bytes, unsigned size,
                     uint32_t start, RadioTraceEvent *event)
{
    unsigned accepted = 0;
    for (unsigned i = 0; i < size; ++i)
        accepted += RadioTrace_Feed(trace, bytes[i], start + i, event);
    return accepted;
}
int main(void)
{
    RadioTrace trace; RadioTraceEvent event;
    uint8_t packet[12] = {0xa5, 0x5a, 2, 0xff, 0xff, 4, 1, 0xfe, 0xff, 0xff, 0xff, 0};
    packet[11] = Radio_Crc(packet, 11);
    RadioTrace_Init(&trace);
    assert(feed(&trace, packet, 12, 100, &event) == 1);
    assert(event.sequence == 65535 && event.tx_ms == 0xfffffffeU);
    assert(event.rx_ms == 111 && event.command == 4 && event.keys == 1);
    assert(memcmp(event.raw, packet, 12) == 0);
    packet[3] = packet[4] = 0; packet[7] = packet[8] = packet[9] = packet[10] = 0;
    packet[11] = Radio_Crc(packet, 11);
    assert(feed(&trace, packet, 12, 130, &event) == 1);
    assert(event.sequence == 0 && event.tx_ms == 0);
    packet[11] ^= 1;
    assert(feed(&trace, packet, 12, 160, &event) == 0);
    packet[11] ^= 1;
    assert(feed(&trace, packet, 12, 180, &event) == 1);
    const uint8_t noise[] = {0x55, 0xa5, 0x00, 0x5a};
    assert(feed(&trace, noise, 4, 200, &event) == 0);
    uint8_t v1[6]; Radio_Encode(255, RADIO_STOP, v1);
    assert(feed(&trace, v1, 6, 210, &event) == 1);
    assert(event.version == 1 && event.sequence == 255 && event.command == 3);
    assert(feed(&trace, packet, 5, 230, &event) == 0);
    assert(feed(&trace, packet + 5, 7, 280, &event) == 1); /* E49 split, gap 46ms. */
    assert(event.rx_ms == 286);
    assert(feed(&trace, packet, 12, 310, &event) == 1);
    assert(feed(&trace, packet, 5, 340, &event) == 0);
    assert(feed(&trace, packet + 5, 7, 650, &event) == 0); /* Truly stale partial. */
    assert(feed(&trace, packet, 12, 700, &event) == 1);
    RadioTrace_Init(&trace);
    assert(feed(&trace, packet, 12, 0xfffffff8U, &event) == 1);
    assert(event.rx_ms == 3U); /* Receiver tick wrap. */
    packet[5] = 5; packet[11] = Radio_Crc(packet, 11);
    assert(feed(&trace, packet, 12, 30, &event) == 0);
    puts("radio_trace CRC, corruption recovery, legacy frames, stale partial and wraps: PASS");
}
