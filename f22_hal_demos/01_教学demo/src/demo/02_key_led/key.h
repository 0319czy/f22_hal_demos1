#ifndef DEMO02_KEY_H
#define DEMO02_KEY_H

#include "stm32f1xx_hal.h"

/* 对应实体按键保持按下时返回 1。 */
uint8_t Key1_IsPressed(void);
uint8_t Key2_IsPressed(void);

#endif /* DEMO02_KEY_H */
