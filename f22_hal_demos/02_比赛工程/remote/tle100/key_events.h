#ifndef TLE100_KEY_EVENTS_H
#define TLE100_KEY_EVENTS_H

typedef unsigned char KeyByte;
#ifdef __SDCC_mcs51
#define KEY_RAM __idata
#else
#define KEY_RAM
#endif
/* Accessed by the 1ms ISR; Next() is called with interrupts masked. */
typedef struct {
    volatile KeyByte command[8], keys[8], token[8];
    volatile KeyByte head, tail, count, stable, debounce[4], serial, release_latched;
    volatile KeyByte active_command, active_keys, active_token, repeats;
    volatile KeyByte stop_keys, stop_token, stop_repeats, stop_release, stop_blocked;
    volatile unsigned int dropped;
} KeyEvents;
typedef struct { KeyByte command, keys, token; } KeyEventPacket;

void KeyEvents_Init(KeyEvents KEY_RAM *state, KeyByte initial_keys);
/* Exactly one call per millisecond. Changes require five equal raw samples. */
void KeyEvents_Sample(KeyEvents KEY_RAM *state, KeyByte raw_keys);
KeyByte KeyEvents_Urgent(const KeyEvents KEY_RAM *state);
/* Caller masks the ISR while consuming; no dynamic allocation or clocks. */
void KeyEvents_Next(KeyEvents KEY_RAM *state, KeyEventPacket KEY_RAM *packet);

#endif
