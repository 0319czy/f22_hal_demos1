#ifndef DEMO01_LED_H
#define DEMO01_LED_H

#include "stm32f1xx_hal.h"

/* 依次点亮 LED1、LED2，便于只用一个灯时也能判断是哪个通道失效。 */
void Led1_On(void);
void Led2_On(void);
void Leds_Off(void);

/* 两个灯同时闪，最直观的"板子还活着"标志。 */
void Leds_BlinkBoth(void);

/* 两个灯交替闪，用于区分两个通道是否接反或其中一个损坏。 */
void Leds_BlinkAlternate(void);

#endif /* DEMO01_LED_H */
