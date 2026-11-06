#ifndef F22_FLOW_H
#define F22_FLOW_H
#include "sensor_io.h"
typedef enum {
    FLOW_STAGE_NONE, FLOW_STAGE_POWER_WAIT, FLOW_STAGE_RESET_WRITE,
    FLOW_STAGE_RESET_WAIT, FLOW_STAGE_ID_READ, FLOW_STAGE_INVERSE_READ,
    FLOW_STAGE_ID_CHECK, FLOW_STAGE_CLEAR_READ, FLOW_STAGE_CLEAR_WAIT,
    FLOW_STAGE_CONFIG_WRITE, FLOW_STAGE_READY, FLOW_STAGE_BURST_READ,
    FLOW_STAGE_RECHECK_ID, FLOW_STAGE_RECHECK_INVERSE,
    FLOW_STAGE_BANK_SELECT, FLOW_STAGE_PROBE_REVISION,
    FLOW_STAGE_PROBE_CONFIG_X, FLOW_STAGE_PROBE_CONFIG_Y
} FlowStage;
typedef struct {
    SensorIo io;
    int16_t dx, dy;
    uint8_t id, inverse_id, motion, observation, quality, raw_sum;
    uint8_t raw_max, raw_min, present, valid;
    uint16_t shutter;
    uint32_t last_sample_ms;
    unsigned errors, attempts, initializing, config_index;
    FlowStage stage, fault;
    uint8_t last_reg, id_read_mask;
    uint8_t probe_only, revision, config_4e, config_5b;
    int io_status;
    uint32_t next_ms;
} FlowSensor;
int Flow_Init(FlowSensor *sensor, SensorIo io);
/* Retry uses the already initialized sensor, retains cumulative errors and
 * attempts, and never sleeps. Call Step only while motor outputs are stopped. */
void Flow_BeginRetry(FlowSensor *sensor);
int Flow_InitStep(FlowSensor *sensor); /* 0=pending, 1=ready, -1=failed */
int Flow_CheckIdentity(FlowSensor *sensor);
int Flow_Poll(FlowSensor *sensor);
#endif
