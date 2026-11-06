#ifndef F22_SENSOR_IO_H
#define F22_SENSOR_IO_H
#include <stdint.h>
#include <stddef.h>
/* Return 0 on success. HAL adapter bounds each transfer; host tests inject faults. */
typedef struct {
    void *context;
    int (*read)(void *, uint16_t reg, uint8_t *, size_t);
    int (*write)(void *, uint16_t reg, const uint8_t *, size_t);
    uint32_t (*now_ms)(void *);
    void (*delay_ms)(void *, uint32_t);
} SensorIo;
#endif
