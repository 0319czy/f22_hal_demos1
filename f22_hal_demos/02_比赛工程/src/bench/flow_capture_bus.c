#include "i2c.h"

/* Compatibility name for older flow-capture tooling. Production boot uses
 * I2C2_ClearBusAtBoot() directly from the HAL layer. */
unsigned FlowCapture_ClearI2cAtBoot(void)
{
    return I2C2_ClearBusAtBoot();
}
