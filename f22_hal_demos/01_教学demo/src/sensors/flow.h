#ifndef F22_FLOW_H
#define F22_FLOW_H
#include "sensor_io.h"
typedef struct {
    SensorIo io;
    int16_t dx, dy;
    uint8_t id, inverse_id, motion, quality, present, valid;
    uint32_t last_sample_ms;
    unsigned errors;
} FlowSensor;
int Flow_Init(FlowSensor *sensor, SensorIo io);
int Flow_Poll(FlowSensor *sensor);
#endif
