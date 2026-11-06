#ifndef DEMO02_LED_H
#define DEMO02_LED_H

#include "stm32f1xx_hal.h"

/* LED1（JP3 / PA0）和 LED2（JP4 / PA1）的四个基本操作。 */
void Led1_On(void);
void Led1_Off(void);
void Led1_Toggle(void);
void Led2_On(void);
void Led2_Off(void);
void Led2_Toggle(void);
void Leds_Off(void);

#endif /* DEMO02_LED_H */
