#include "../remote/tle100/key_events.h"
#include <assert.h>
#include <stdio.h>

static void Samples(KeyEvents *s, unsigned raw, unsigned ms)
{ while (ms--) KeyEvents_Sample(s, (KeyByte)raw); }
static KeyEventPacket Next(KeyEvents *s)
{ KeyEventPacket p; KeyEvents_Next(s, &p); return p; }
static void Repeat(KeyEvents *s, unsigned command, unsigned keys, unsigned token)
{
    for (unsigned i = 0; i < 3; i++) {
        KeyEventPacket p = Next(s);
        assert(p.command == command && p.keys == keys && p.token == token);
    }
}
static void Release(KeyEvents *s)
{ KeyEventPacket p = Next(s); assert(!p.command && !p.keys); }
static void Init(KeyEvents *s, unsigned held)
{
    KeyEvents_Init(s, (KeyByte)held);
    if (!held) Release(s); /* The normal initial released heartbeat. */
}
static void BusyTx(void)
{
    KeyEvents s; KeyEventPacket sent[16]; unsigned at[16], used = 0, last = 0, busy = 0;
    KeyEvents_Init(&s, 0);
    for (unsigned ms = 0; ms <= 290; ms++) {
        unsigned raw = ms >= 1 && ms < 8 ? 1 : ms >= 37 && ms < 45 ? 2 : 0;
        Samples(&s, raw, 1); /* ISR continues throughout a blocking UART frame. */
        if (ms < busy) continue;
        unsigned guard = KeyEvents_Urgent(&s) ? 35 : 50;
        if (used && ms - last < guard) continue;
        assert(used < 16); at[used] = ms; sent[used++] = Next(&s);
        last = ms; busy = ms + 13; /* 12.5ms at UART9600. */
    }
    assert(used == 9 && sent[0].command == 0);
    for (unsigned i = 1; i < used; i++) assert(at[i] - at[i-1] >= 35);
    for (unsigned i = 1; i <= 3; i++) assert(sent[i].command == 4 && sent[i].token == 1);
    assert(!sent[4].command && !sent[4].keys);
    for (unsigned i = 5; i <= 7; i++) assert(sent[i].command == 1 && sent[i].token == 2);
    assert(!sent[8].command && !sent[8].keys);
}

int main(void)
{
    KeyEvents s; Init(&s, 0);
    BusyTx();
    /* Two different presses inside a 35ms UART guard survive in order. */
    Samples(&s, 1, 5); Samples(&s, 0, 5); Samples(&s, 2, 5);
    assert(s.count == 2 && KeyEvents_Urgent(&s));
    Repeat(&s, 4, 1, 1);
    KeyEventPacket released = Next(&s);
    assert(!released.command && !released.keys && released.token == 1);
    Repeat(&s, 1, 2, 2);
    assert(!s.count && !s.repeats);

    /* Same key twice while TX is busy: distinct tokens, real release HB. */
    Init(&s, 0);
    Samples(&s, 2, 5); Samples(&s, 0, 5); Samples(&s, 2, 5);
    Repeat(&s, 1, 2, 1); Release(&s); Repeat(&s, 1, 2, 2);

    /* A short F4 already released before any TX still sends all STOP retries. */
    Init(&s, 0); Samples(&s, 8, 5); Samples(&s, 0, 5);
    Repeat(&s, 3, 8, 1); Release(&s); Release(&s);

    /* STOP preempts active retries and queued actions. START pressed while
     * F4 is held cannot accumulate, nor start merely when F4 is released. */
    Init(&s, 0); Samples(&s, 2, 5);
    assert(Next(&s).command == 1 && s.repeats == 2);
    Samples(&s, 10, 5); assert(!s.repeats && !s.count);
    Samples(&s, 8, 5); Samples(&s, 10, 5); Samples(&s, 2, 5);
    assert(!s.count && s.stop_blocked);
    Repeat(&s, 3, 10, 2);
    assert(Next(&s).command == 0); /* held F2 is only a non-release heartbeat */
    Samples(&s, 0, 5); Samples(&s, 2, 5);
    Release(&s); Repeat(&s, 1, 2, 3);

    /* Held F4 continuously emits STOP after the initial repeat budget. */
    Init(&s, 0); Samples(&s, 8, 5); Repeat(&s, 3, 8, 1);
    for (unsigned i = 0; i < 8; i++) assert(Next(&s).command == 3);
    Samples(&s, 0, 5); Release(&s);

    /* Eight-item capacity is exact, no overwrite; STOP bypasses a full FIFO. */
    Init(&s, 0);
    for (unsigned i = 0; i < 8; i++) { Samples(&s, 1, 5); Samples(&s, 0, 5); }
    assert(s.count == 8 && !s.dropped);
    Samples(&s, 1, 5); Samples(&s, 0, 5);
    assert(s.count == 8 && s.dropped == 1);
    for (unsigned i = 1; i <= 8; i++) { Repeat(&s, 4, 1, i); Release(&s); }
    assert(!s.count && s.dropped == 1);
    for (unsigned i = 0; i < 9; i++) { Samples(&s, 1, 5); Samples(&s, 0, 5); }
    assert(s.count == 8);
    Samples(&s, 8, 5); assert(!s.count && s.stop_repeats == 3);

    /* Debounce every bit independently; no event before five matching ticks. */
    Init(&s, 0); Samples(&s, 1, 4); assert(!s.count);
    Samples(&s, 0, 1); Samples(&s, 1, 4); assert(!s.count);
    Samples(&s, 3, 1); assert(s.count == 1 && s.stable == 1);
    Samples(&s, 3, 4); assert(s.count == 2 && s.stable == 3);
    Repeat(&s, 4, 1, 1); Repeat(&s, 1, 3, 2);

    /* Held-at-start keys do not create actions; a release and new press do. */
    Init(&s, 2); assert(Next(&s).command == 0 && !s.count);
    Samples(&s, 0, 5); Release(&s); Samples(&s, 2, 5); Repeat(&s, 1, 2, 1);

    /* Tokens wrap only between events; retransmission tokens remain fixed. */
    Init(&s, 0);
    for (unsigned i = 1; i <= 33; i++) {
        Samples(&s, 4, 5); Repeat(&s, 2, 4, i & 15U);
        Samples(&s, 0, 5); Release(&s);
    }
    s.dropped = 65535U;
    for (unsigned i = 0; i < 9; i++) { Samples(&s, 1, 5); Samples(&s, 0, 5); }
    assert(s.dropped == 65535U);
    puts("Remote 1ms keys/FIFO: guard bursts, repeats, release, STOP priority, capacity, debounce, startup and token wrap PASS (host)");
}
