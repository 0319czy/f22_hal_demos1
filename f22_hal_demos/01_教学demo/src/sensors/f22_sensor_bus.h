#ifndef F22_SENSOR_BUS_H
#define F22_SENSOR_BUS_H
#include "sensor_io.h"
/* Initialize module enable PC14 and SPI2; I2C2 is initialized by caller. */
void F22_ModuleBusInit(void);
SensorIo F22_TofIo(void);
SensorIo F22_FlowIo(void);
#endif
