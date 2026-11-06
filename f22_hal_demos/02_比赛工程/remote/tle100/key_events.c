#include "key_events.h"
#ifdef __SDCC_mcs51
#define KEY_ISR_HELPER __reentrant
#else
#define KEY_ISR_HELPER
#endif

/* ISR-only helper: stack arguments prevent its parameters from overlaying
 * the main loop's CRC pointer or other non-reentrant compiler temporaries. */
static KeyByte Enqueue(KeyEvents KEY_RAM *s, KeyByte command, KeyByte keys, KeyByte token) KEY_ISR_HELPER
{
    if (s->count == 8) { if (s->dropped != 65535U) s->dropped++; return 0; }
    s->command[s->head] = command; s->keys[s->head] = keys; s->token[s->head] = token;
    s->head = (s->head + 1U) & 7U; s->count++; return 1;
}
void KeyEvents_Init(KeyEvents KEY_RAM *s, KeyByte initial_keys)
{
    KeyByte i;
    s->head = s->tail = s->count = s->serial = s->repeats = 0;
    s->active_command = s->active_keys = s->active_token = 0;
    s->stop_token = s->stop_repeats = s->stop_release = 0;
    s->stable = initial_keys & 15U; s->stop_keys = s->stable;
    s->release_latched = !s->stable;
    s->stop_blocked = (s->stable & 8U) != 0; s->dropped = 0;
    for (i = 0; i < 4; i++) s->debounce[i] = 0;
}
void KeyEvents_Sample(KeyEvents KEY_RAM *s, KeyByte raw)
{
    KeyByte i, mask, before = s->stable, pressed;
    raw &= 15U;
    for (i = 0, mask = 1; i < 4; i++, mask <<= 1) {
        if ((raw & mask) == (s->stable & mask)) s->debounce[i] = 0;
        else if (++s->debounce[i] == 5) {
            s->stable ^= mask; s->debounce[i] = 0;
        }
    }
    pressed = s->stable & ~before;
    if (pressed & 8U) {
        /* STOP bypasses a full FIFO and cancels old actions and retries. */
        s->head = s->tail = s->count = s->repeats = 0;
        s->stop_release = s->release_latched = 0; s->stop_blocked = 1;
        s->serial = (s->serial + 1U) & 15U;
        s->stop_token = s->serial; s->active_token = s->stop_token;
        s->stop_keys = s->stable; s->stop_repeats = 3;
        return;
    }
    if (s->stop_blocked) {
        /* Keys pressed during held STOP do not turn into a delayed START. */
        if (!s->stable) { s->stop_release = 1; s->stop_blocked = 0; }
        return;
    }
    if (before && !s->stable) {
        /* Latch the actual debounced release, never synthesize one from an
         * action's snapshot. It must precede the next same-key action. */
        s->release_latched = 1;
    }
    for (i = 0, mask = 1; i < 3; i++, mask <<= 1) {
        if (pressed & mask) {
            s->serial = (s->serial + 1U) & 15U;
            /* Internal bit7 means a genuine all-keys release preceded this
             * action. It does not occupy another FIFO slot or reach the wire. */
            if (Enqueue(s, i == 0 ? 4 : i,
                        s->stable | (s->release_latched ? 0x80U : 0), s->serial))
                s->release_latched = 0;
        }
    }
}
KeyByte KeyEvents_Urgent(const KeyEvents KEY_RAM *s)
{ return s->stop_repeats || s->stop_release || s->repeats || s->count || s->release_latched; }
void KeyEvents_Next(KeyEvents KEY_RAM *s, KeyEventPacket KEY_RAM *p)
{
    if (s->stop_repeats || (s->stable & 8U)) {
        p->command = 3; p->keys = s->stop_repeats ? s->stop_keys : s->stable;
        p->token = s->stop_token;
        if (s->stop_repeats) s->stop_repeats--;
        return;
    }
    if (s->stop_release) {
        p->command = p->keys = 0; p->token = s->stop_token;
        s->stop_release = 0; return;
    }
    if (!s->repeats && s->count) {
        if (s->keys[s->tail] & 0x80U) {
            p->command = p->keys = 0; p->token = s->active_token;
            s->keys[s->tail] &= 15U; return;
        }
        s->active_command = s->command[s->tail]; s->active_keys = s->keys[s->tail];
        s->active_token = s->token[s->tail];
        s->tail = (s->tail + 1U) & 7U; s->count--;
        s->repeats = s->active_command ? 3 : 1;
    }
    if (s->repeats) {
        p->command = s->active_command; p->keys = s->active_keys; p->token = s->active_token;
        s->repeats--; return;
    }
    if (s->release_latched) {
        p->command = p->keys = 0; p->token = s->active_token;
        s->release_latched = 0; return;
    }
    p->command = 0; p->keys = s->stable; p->token = s->active_token;
}
