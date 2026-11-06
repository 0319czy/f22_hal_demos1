#ifndef STANDALONE_KEYS_H
#define STANDALONE_KEYS_H
#include "closed_loop.h"
#include <stdint.h>
typedef struct { uint32_t changed_ms; unsigned initialized, down, ready; } StandaloneKeys;
void StandaloneKeys_Init(StandaloneKeys *k);
HoverCommand StandaloneKeys_Step(StandaloneKeys *k, unsigned key1, unsigned key2,
                                 ClosedLoopState state, uint32_t now);
#endif
