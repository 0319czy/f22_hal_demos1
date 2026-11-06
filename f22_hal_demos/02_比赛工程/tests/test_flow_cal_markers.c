#include "closed_loop_radio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void Packet(ClosedLoopRadio *in, uint16_t seq, RadioCommand command,
                   uint8_t keys, uint32_t time)
{
    uint8_t p[12] = {0xa5,0x5a,2,(uint8_t)seq,(uint8_t)(seq >> 8),
        (uint8_t)command,keys,(uint8_t)time,(uint8_t)(time >> 8),
        (uint8_t)(time >> 16),(uint8_t)(time >> 24),0};
    p[11] = Radio_Crc(p, 11);
    for (unsigned i = 0; i < sizeof(p); ++i)
        ClosedLoopRadio_Feed(in, p[i], time);
}

int main(void)
{
    ClosedLoopRadio in;
    ClosedLoopRadio_Init(&in);
    Packet(&in, 1, RADIO_HEARTBEAT, 0, 10);
    assert(in.ready);

    Packet(&in, 2, RADIO_START, 2, 20); /* F2: mark only */
    assert(in.event_id == 1 && in.event_command == RADIO_START && in.event_allowed);
    assert(in.pending == HOVER_NONE && ClosedLoopRadio_Take(&in, 20) == HOVER_NONE);

    /* A link/session latch may clear ready, but must not hide marker metadata. */
    in.fault_latched = 1U; in.ready = 0U;
    Packet(&in, 3, RADIO_LAND, 4, 30); /* F3: mark only */
    assert(in.event_id == 2 && in.event_command == RADIO_LAND && in.event_allowed);
    assert(in.fault_latched && !in.ready);
    assert(in.pending == HOVER_NONE && ClosedLoopRadio_Take(&in, 30) == HOVER_NONE);

    Packet(&in, 4, RADIO_STOP, 8, 40); /* F4: marker, clears stale diagnostic fault */
    assert(in.event_id == 3 && in.event_command == RADIO_STOP && in.event_allowed);
    assert(in.pending == HOVER_NONE && ClosedLoopRadio_Take(&in, 40) == HOVER_NONE);
    puts("Flow calibration markers: F2/F3/F4 logged as events, no flight commands queued");
    return 0;
}
