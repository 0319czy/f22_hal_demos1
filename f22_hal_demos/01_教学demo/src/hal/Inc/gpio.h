#ifndef F22_HAL_DEMOS_GPIO_H
#define F22_HAL_DEMOS_GPIO_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

/* 初始化 LED1/LED2 输出（PA0/PA1）和两个板载按键输入（PC12/PC13）。 */
void MX_GPIO_Init(void);

/* 初始化 J2/J3 上的 5 个扩展通用 IO：PC14、PC15、PB3、PB4、PB5。 */
void MX_GPIO_InitGeneralIo(void);

/* 把 PC12/PC13 改成下降沿外部中断输入，供 exti_key 例程使用。 */
void MX_GPIO_InitExtiKey(void);

#endif /* F22_HAL_DEMOS_GPIO_H */
