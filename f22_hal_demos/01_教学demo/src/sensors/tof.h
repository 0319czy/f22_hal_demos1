#ifndef F22_TOF_H
#define F22_TOF_H
#include "sensor_io.h"
#define TOF_ADDR_7BIT 0x29U
#define TOF_MAX_AGE_MS 150U
typedef struct {
    SensorIo io;
    uint32_t last_sample_ms, started_ms;
    uint16_t distance_mm, id;
    uint8_t raw_status, present, valid, seen;
    unsigned errors;
} TofSensor;
/* VL53L1X ONLY: long distance mode, 33ms budget, 40ms interval. */
int Tof_Init(TofSensor *sensor, SensorIo io);
/* 1 new sample, 0 waiting, -1 communication/timeout error; invalid range stays invalid. */
int Tof_Poll(TofSensor *sensor);
unsigned Tof_AgeMs(const TofSensor *sensor);
#endif
