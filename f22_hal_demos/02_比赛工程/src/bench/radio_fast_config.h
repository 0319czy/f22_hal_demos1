#ifndef RADIO_FAST_CONFIG_H
#define RADIO_FAST_CONFIG_H
#include <stdint.h>

typedef struct {
    uint8_t original[6], active[6];
    unsigned read_ok, applied, rollback_ok;
} RadioFastConfig;
/* Query and use C2 (volatile settings), retaining saved address/channel/options. */
void RadioFast_Configure(RadioFastConfig *result);
#endif
