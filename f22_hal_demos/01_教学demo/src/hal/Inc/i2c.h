#ifndef F22_HAL_DEMOS_I2C_H
#define F22_HAL_DEMOS_I2C_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

/* 初始化 JP6 的 I2C1（PB6=SCL / PB7=SDA），100 kHz。 */
void MX_I2C1_Init(void);

/* 初始化 JP7 的 I2C2（PB10=SCL / PB11=SDA），100 kHz。 */
void MX_I2C2_Init(void);

#endif /* F22_HAL_DEMOS_I2C_H */
