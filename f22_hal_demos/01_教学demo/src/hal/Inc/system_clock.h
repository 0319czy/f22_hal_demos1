#ifndef F22_HAL_DEMOS_SYSTEM_CLOCK_H
#define F22_HAL_DEMOS_SYSTEM_CLOCK_H

#include "stm32f1xx_hal.h"

/*
 * 使用板载 8 MHz 无源晶振和 PLL 配置 72 MHz 系统时钟。
 * 顺带释放 PB3/PB4 的 JTAG 复用，使 J3 上的两个通用 IO 可以正常使用。
 */
void SystemClock_Config(void);

#endif /* F22_HAL_DEMOS_SYSTEM_CLOCK_H */
