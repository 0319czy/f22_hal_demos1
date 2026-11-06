#include "remote.h"
#include <string.h>

void RemoteKeys_Init(RemoteKeys *keys)
{
    memset(keys, 0, sizeof(*keys));
}

uint8_t RemoteKeys_Feed(RemoteKeys *keys, uint8_t byte, uint32_t now_ms)
{
    if (byte != 'F' && byte != 'B' && byte != 'L' && byte != 'R' &&
        byte != 'U' && byte != 'D' && byte != 'W' && byte != 'X' &&
        byte != 'Y' && byte != 'Z') {
        if (byte != '\r' && byte != '\n') ++keys->rejected;
        return 0U;
    }
    /* One key is one diagnostic event; this demo never arms an actuator. */
    keys->forward = keys->right = keys->vertical = 0;
    keys->calibrate_requested = keys->start_requested = keys->stop_requested = 0U;
    switch (byte) {
    case 'F': keys->forward = 1; break;
    case 'B': keys->forward = -1; break;
    case 'R': keys->right = 1; break;
    case 'L': keys->right = -1; break;
    case 'U': keys->vertical = 1; break;
    case 'D': keys->vertical = -1; break;
    case 'W': keys->calibrate_requested = 1U; break;
    case 'X': keys->start_requested = 1U; break;
    default: keys->stop_requested = 1U; break;
    }
    keys->last_key = byte;
    keys->last_key_ms = now_ms;
    keys->seen = 1U;
    ++keys->accepted;
    return 1U;
}

uint8_t RemoteKeys_Update(RemoteKeys *keys, uint32_t now_ms)
{
    if (!keys->seen || (uint32_t)(now_ms - keys->last_key_ms) >= 250U) {
        keys->forward = keys->right = keys->vertical = 0;
        keys->calibrate_requested = keys->start_requested = keys->stop_requested = 0U;
        return 0U;
    }
    return 1U;
}
