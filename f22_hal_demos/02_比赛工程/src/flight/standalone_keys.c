#include "standalone_keys.h"
#include <string.h>
void StandaloneKeys_Init(StandaloneKeys *k) { memset(k, 0, sizeof(*k)); }
HoverCommand StandaloneKeys_Step(StandaloneKeys *k, unsigned key1, unsigned key2,
                                 ClosedLoopState state, uint32_t now)
{
    if (!k->initialized) {
        k->initialized = 1; k->down = key1; k->changed_ms = now; k->ready = 0;
    }
    if (key2) {
        k->ready = 0; k->down = key1; k->changed_ms = now;
        return HOVER_STOP; /* Immediate stop; no debounce on emergency button. */
    }
    if (key1 != k->down) { k->down = key1; k->changed_ms = now; }
    uint32_t age = now - k->changed_ms;
    if (!k->down && age >= 50U) k->ready = 1;
    if (k->down && k->ready && age >= 30U) {
        k->ready = 0; /* A held button is a single event, never an auto-rearm. */
        return state == CL_IDLE ? HOVER_START : state == CL_FAULT ? HOVER_STOP : HOVER_REQUEST_LAND;
    }
    return HOVER_NONE;
}
