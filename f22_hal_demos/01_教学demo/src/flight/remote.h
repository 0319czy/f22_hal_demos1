#ifndef F22_REMOTE_H
#define F22_REMOTE_H
#include <stdint.h>

/* Legacy TLE100 key stream: no release packets and no idle heartbeat. */
typedef struct {
    uint32_t last_key_ms, accepted, rejected;
    uint8_t seen, last_key;
    int8_t forward, right, vertical;
    uint8_t calibrate_requested, start_requested, stop_requested;
} RemoteKeys;

void RemoteKeys_Init(RemoteKeys *keys);
uint8_t RemoteKeys_Feed(RemoteKeys *keys, uint8_t byte, uint32_t now_ms);
/* Returns recent-key activity, NOT proof that an idle radio link is alive. */
uint8_t RemoteKeys_Update(RemoteKeys *keys, uint32_t now_ms);
#endif
