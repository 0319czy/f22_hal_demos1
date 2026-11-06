#ifndef F22_LESSON_SUPPORT_H
#define F22_LESSON_SUPPORT_H
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include <string.h>
static inline void Lesson_Init(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_USART1_UART_Init();
}
static inline uint8_t Lesson_Key1(void)
{
    return HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET;
}
static inline uint8_t Lesson_Key2(void)
{
    return HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET;
}
static inline HAL_StatusTypeDef Lesson_SendFloats(const float *values, unsigned count)
{
    uint8_t frame[32U * sizeof(float) + 4U];
    static const uint8_t tail[] = {0x00U, 0x00U, 0x80U, 0x7fU};
    _Static_assert(sizeof(float) == 4U, "JustFloat requires float32");
    if (count > 32U) return HAL_ERROR;
    memcpy(frame, values, count * sizeof(float));
    memcpy(frame + count * sizeof(float), tail, sizeof(tail));
    return HAL_UART_Transmit(&huart1, frame, count * sizeof(float) + sizeof(tail), 20U);
}
#endif
