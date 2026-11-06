/* Slow PA2/JP13 signal test for a DC multimeter. No PWM or radio parser. */
#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include <stdio.h>

static void MotorsOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOB, &gpio);
}

int main(void)
{
    GPIO_InitTypeDef gpio = {0};
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    MotorsOff();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = BOARD_USART2_TX_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(BOARD_USART2_TX_PORT, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(BOARD_USART2_TX_PORT, &gpio);

    for (unsigned sequence = 0;; ++sequence) {
        GPIO_PinState pa2 = (sequence & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(BOARD_USART2_TX_PORT, BOARD_USART2_TX_PIN, pa2);
        char line[64];
        int length = snprintf(line, sizeof(line), "JP13_VOLTAGE seq=%u pa2=%u\r\n",
                              sequence, pa2 == GPIO_PIN_SET);
        if (length > 0 && (unsigned)length < sizeof(line))
            (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 100);
        HAL_Delay(2000);
    }
}
