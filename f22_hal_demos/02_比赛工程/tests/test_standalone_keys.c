#include "standalone_keys.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    StandaloneKeys k; StandaloneKeys_Init(&k);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 0) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 5000) == HOVER_NONE); /* held on boot */
    assert(StandaloneKeys_Step(&k, 0, 0, CL_IDLE, 5001) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 0, 0, CL_IDLE, 5051) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 5060) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 5089) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 5090) == HOVER_START);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_ASCEND, 6000) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 0, 0, CL_HOLD, 6001) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 0, 0, CL_HOLD, 6051) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_HOLD, 6060) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_HOLD, 6090) == HOVER_REQUEST_LAND);
    assert(StandaloneKeys_Step(&k, 1, 1, CL_HOLD, 6091) == HOVER_STOP);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_IDLE, 8000) == HOVER_NONE); /* no restart after KEY2 */
    StandaloneKeys_Init(&k);
    assert(StandaloneKeys_Step(&k, 0, 0, CL_FAULT, UINT32_MAX-100U) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 0, 0, CL_FAULT, UINT32_MAX-50U) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_FAULT, UINT32_MAX-10U) == HOVER_NONE);
    assert(StandaloneKeys_Step(&k, 1, 0, CL_FAULT, 19U) == HOVER_STOP);
    puts("Standalone keys passed: boot-held inhibit, debounce, single event, land, immediate stop, fault clear and tick wrap");
    return 0;
}
