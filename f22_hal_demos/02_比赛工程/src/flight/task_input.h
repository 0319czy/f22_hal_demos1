#ifndef F22_TASK_INPUT_H
#define F22_TASK_INPUT_H
#include "hover.h"
#include "radio.h"
typedef struct {
    RadioReceiver radio;
    HoverCommand pending;
    unsigned calibrate;
    uint32_t pending_ms, calibrate_ms;
} TaskInput;
void TaskInput_Init(TaskInput *input);
/* now is the byte's ARRIVAL timestamp, not a delayed queue-consumption timestamp. */
void TaskInput_Feed(TaskInput *input, uint8_t byte, uint32_t now);
HoverCommand TaskInput_Take(TaskInput *input, uint32_t now);
/* Calibration events are consumed and rejected unless the controller is idle. */
unsigned TaskInput_TakeCalibration(TaskInput *input, HoverState state, uint32_t now);
#endif
