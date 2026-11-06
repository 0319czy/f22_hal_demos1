/* Separate diagnostic firmware, incompatible with flight/motor-control v1.
 * UART binary packets are observed directly over the remote USB serial port. */
#include <8052.h>
#ifndef LATENCY_PROTOCOL_V2
#include "key_events.h"
#endif
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;
__sbit __at(0x91) RADIO_MODE1;
__sbit __at(0x92) RADIO_MODE0;
static volatile u32 ticks;
#ifndef LATENCY_PROTOCOL_V2
static __idata KeyEvents key_events;
static __idata KeyEventPacket next_event;
static volatile u8 scan_enabled;
#ifdef LATENCY_AUTO_KEYS
static u16 auto_phase;
static u8 auto_slot;
#endif
static u8 ReadKeys(void)
{
    u8 result = 0;
    if (!(P0 & 0x40)) result |= 1;
    if (!(P0 & 0x80)) result |= 2;
    if (!(P1 & 0x08)) result |= 4;
    if (!(P1 & 0x10)) result |= 8;
    return result;
}
#endif
void Timer2Tick(void) __interrupt(5)
{
    TF2 = 0; ticks++;
#ifndef LATENCY_PROTOCOL_V2
    if (scan_enabled) {
#ifdef LATENCY_AUTO_KEYS
        /* Avoid 32-bit division in the 1ms interrupt. */
        if (++auto_phase == 757) { auto_phase = 0; auto_slot = (auto_slot + 1U) & 3U; }
        KeyEvents_Sample(&key_events, auto_phase < 80 ? (u8)(1U << auto_slot) : 0);
#else
        KeyEvents_Sample(&key_events, ReadKeys());
#endif
    }
#endif
}
static u32 Now(void)
{
    u32 value; EA = 0; value = ticks; EA = 1; return value;
}
static void Send(u8 byte) { TI = 0; SBUF = byte; while (!TI) {} TI = 0; }
#ifdef LATENCY_RADIO_FAST
static u8 config_original[6], config_active[6], config_ok, config_read, rollback_ok;
static void ConfigLog(void);
static void Wait(u16 ms)
{ u32 start = Now(); while ((u32)(Now() - start) < ms) {} }
static u8 Receive(u8 *p, u8 size)
{
    u8 used = 0; u32 start = Now();
    while (used < size && (u32)(Now() - start) < 400UL) {
        if (RI) { p[used++] = SBUF; RI = 0; }
    }
    return used == size;
}
static u8 Parameters(u8 *p)
{
    RI = 0; Send(0xc1); Send(0xc1); Send(0xc1);
    return Receive(p, 6) && p[0] == 0xc0 && p[4] <= 0xc8 && !(p[5] & 0x7c);
}
static void RadioConfigure(void)
{
    u8 i, candidate[6], echo[6], ok = 0;
    RADIO_MODE0 = 0; RADIO_MODE1 = 1; Wait(120);
    config_read = Parameters(config_original);
    if (config_read) {
        for (i = 0; i < 6; i++) candidate[i] = config_original[i];
        candidate[0] = 0xc2; candidate[3] = (candidate[3] & 0xc0) | 0x1d; /* UART9600, RF50k; volatile. */
#ifdef LATENCY_UART_FAST
        candidate[3] = 0x35; /* UART57600, RF50k. */
#endif
        RI = 0; for (i = 0; i < 6; i++) Send(candidate[i]);
        Receive(echo, 6); Wait(30);
        if (Parameters(config_active)) {
            ok = 1;
            for (i = 1; i < 6; i++) if (candidate[i] != config_active[i]) ok = 0;
        }
        if (!ok) {
            for (i = 0; i < 6; i++) candidate[i] = config_original[i];
            candidate[0] = 0xc2; RI = 0;
            for (i = 0; i < 6; i++) Send(candidate[i]);
            Receive(echo, 6); Wait(30);
            if (Parameters(config_active)) {
                rollback_ok = 1;
                for (i = 1; i < 6; i++)
                    if (config_original[i] != config_active[i]) rollback_ok = 0;
            }
        }
    }
    config_ok = ok;
    /* Configuration mode keeps this USB diagnostic text off the RF channel;
     * the E49 command parser ignores ASCII (no C0/C1/C2/C3 byte is present). */
    ConfigLog(); Wait(2);
    RADIO_MODE0 = RADIO_MODE1 = 0; Wait(120);
    if ((config_ok || rollback_ok) && ((config_active[3] >> 3) & 7) == 6) {
        TR1 = 0; PCON |= 0x80; TH1 = TL1 = 0xff; TR1 = 1;
    }
}
static void Text(const char *p) { while (*p) Send((u8)*p++); }
static void Hex(u8 value)
{
    const char *digits = "0123456789ABCDEF";
    Send(digits[value >> 4]); Send(digits[value & 15]);
}
static void ConfigLog(void)
{
    u8 i;
    Text("CFG TX read="); Send('0' + config_read);
    Text(" applied="); Send('0' + config_ok); Text(" rollback="); Send('0' + rollback_ok);
    Text(" original=");
    for (i = 0; i < 6; i++) Hex(config_original[i]);
    Text(" active="); for (i = 0; i < 6; i++) Hex(config_active[i]);
    Text("\r\n");
}
#endif
#ifdef LATENCY_PROTOCOL_V2
static u8 Keys(void)
{
#ifdef LATENCY_AUTO_KEYS
    /* Exercise the same debounce/event scheduler without a human operator.
     * 757ms is deliberately not a multiple of the 50ms heartbeat period,
     * sweeping key edges through the heartbeat phase. No motor receiver. */
    u32 now = Now();
    return now % 757UL < 80UL ? (u8)(1U << ((now / 757UL) & 3UL)) : 0;
#else
    u8 result = 0;
    if (!(P0 & 0x40)) result |= 1;
    if (!(P0 & 0x80)) result |= 2;
    if (!(P1 & 0x08)) result |= 4;
    if (!(P1 & 0x10)) result |= 8;
    return result;
#endif
}
#endif
static u8 Crc(const u8 *p)
{
    u8 crc = 0, i, bit;
    for (i = 0; i < 11; i++) {
        crc ^= p[i];
        for (bit = 0; bit < 8; bit++) crc = (crc & 0x80) ? (crc << 1) ^ 7 : crc << 1;
    }
    return crc;
}
void main(void)
{
#ifdef LATENCY_PROTOCOL_V2
    u8 p[12] = {0xa5, 0x5a, 2}, i, previous, stable, candidate, last_raw;
    u8 pending = 0, repeats = 0, urgent = 0;
#else
    u8 p[12] = {0xa5, 0x5a, 3}, i, urgent;
#endif
    u16 sequence = 0;
    u32 sent, now;
#ifdef LATENCY_PROTOCOL_V2
    u32 scanned;
#endif
    EA = 0; P0 = P1 = 0xff; RADIO_MODE0 = RADIO_MODE1 = 0;
    PCON &= 0x7f; TMOD = 0x20; TH1 = TL1 = 0xfd; SCON = 0x50; TR1 = 1;
    /* Timer2 autoreload: 922 counts at 921600 Hz = 1.000434ms per tick.
     * Board uptime is descriptive; PC monotonic clock measures observed delay. */
    T2CON = 0; RCAP2H = TH2 = 0xfc; RCAP2L = TL2 = 0x66;
    TF2 = 0; ET2 = 1; TR2 = 1; EA = 1;
#ifdef LATENCY_RADIO_FAST
    RadioConfigure();
#endif
#ifdef LATENCY_PROTOCOL_V2
    previous = stable = candidate = last_raw = Keys();
    scanned = sent = Now(); sent -= 50UL;
    for (;;) {
        u8 edges = 0, force = 0, cmd = 0;
        now = Now();
        if ((u32)(now - scanned) >= 5UL) {
            u8 raw = Keys(); scanned = now;
            candidate = raw == last_raw ? raw : stable;
            last_raw = raw; stable = candidate;
            edges = stable & ~previous; previous = stable;
            /* Preserve a brief F4 press even if released before the guard. */
            if (edges & 8) { pending = 3; repeats = 3; force = 1; }
            else if (edges & 4) { pending = 2; repeats = 3; force = 1; }
            else if (edges & 2) { pending = 1; repeats = 3; force = 1; }
            else if (edges & 1) { pending = 4; repeats = 3; force = 1; }
        }
        /* E49 normal mode flushes a short packet after ~20ms UART idle.
         * A 12-byte 9600/8N1 frame occupies 12.5ms. Keep at least 35ms
         * between frame starts, including immediate key events, so a key
         * cannot merge into a heartbeat still buffered by the module.
         * Latch the event until this guard expires instead of losing it. */
        if (force) urgent = 1;
        if ((u32)(now - sent) < (urgent ? 35UL : 50UL)) continue;
        urgent = 0;
        if (stable & 8) { cmd = 3; pending = repeats = 0; }
        else if (repeats) { cmd = pending; repeats--; }
        sent = Now(); sequence++;
        p[3] = (u8)sequence; p[4] = (u8)(sequence >> 8);
        p[5] = cmd; p[6] = stable;
        p[7] = (u8)sent; p[8] = (u8)(sent >> 8);
        p[9] = (u8)(sent >> 16); p[10] = (u8)(sent >> 24);
        p[11] = Crc(p);
        for (i = 0; i < 12; i++) Send(p[i]);
    }
#else
    /* Startup-held physical keys cannot become queued press events.
     * Radio configuration runs before scanning and never consumes key RAM. */
    EA = 0;
#ifdef LATENCY_AUTO_KEYS
    KeyEvents_Init(&key_events, 0);
#else
    KeyEvents_Init(&key_events, ReadKeys());
#endif
    scan_enabled = 1; EA = 1;
    sent = Now() - 50UL;
    for (;;) {
        now = Now();
        urgent = KeyEvents_Urgent(&key_events);
        if ((u32)(now - sent) < (urgent ? 35UL : 50UL)) continue;
        /* Only queue consumption needs to mask the ISR, not CRC or UART TX. */
        EA = 0; KeyEvents_Next(&key_events, &next_event); EA = 1;
        sent = Now(); sequence++;
        p[3] = (u8)sequence; p[4] = (u8)(sequence >> 8);
        p[5] = next_event.command;
        p[6] = (u8)((next_event.token << 4) | next_event.keys);
        p[7] = (u8)sent; p[8] = (u8)(sent >> 8);
        p[9] = (u8)(sent >> 16); p[10] = (u8)(sent >> 24);
        p[11] = Crc(p);
        for (i = 0; i < 12; i++) Send(p[i]);
    }
#endif
}
