#include "task_input.h"
#include <string.h>
void TaskInput_Init(TaskInput *in) { memset(in, 0, sizeof(*in)); Radio_Init(&in->radio); }
void TaskInput_Feed(TaskInput *in, uint8_t byte, uint32_t now)
{
    if (!Radio_Feed(&in->radio, byte, now)) return;
    switch ((RadioCommand)in->radio.command) {
    case RADIO_STOP:
        in->pending = HOVER_STOP; in->pending_ms = now; in->calibrate = 0U; break;
    case RADIO_LAND:
        if (in->pending != HOVER_STOP) { in->pending = HOVER_REQUEST_LAND; in->pending_ms = now; }
        in->calibrate = 0U; break;
    case RADIO_START:
        if (in->pending == HOVER_NONE) { in->pending = HOVER_START; in->pending_ms = now; }
        in->calibrate = 0U; break;
    case RADIO_CALIBRATE:
        if (in->pending == HOVER_NONE) { in->calibrate = 1U; in->calibrate_ms = now; }
        break;
    case RADIO_HEARTBEAT: break; /* A later heartbeat must not erase an event. */
    }
}
HoverCommand TaskInput_Take(TaskInput *in, uint32_t now)
{
    HoverCommand command = in->pending; in->pending = HOVER_NONE;
    if (command != HOVER_STOP && (!Radio_LinkValid(&in->radio, now) ||
        (uint32_t)(now - in->pending_ms) >= RADIO_LINK_TIMEOUT_MS)) return HOVER_NONE;
    return command;
}
unsigned TaskInput_TakeCalibration(TaskInput *in, HoverState state, uint32_t now)
{
    unsigned requested = in->calibrate; in->calibrate = 0U;
    return requested && state == HOVER_IDLE && in->pending == HOVER_NONE &&
           Radio_LinkValid(&in->radio, now) &&
           (uint32_t)(now - in->calibrate_ms) < RADIO_LINK_TIMEOUT_MS;
}
