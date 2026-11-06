/* Serial path probe: motor pins stay low, no PWM or radio parser. */
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
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    MotorsOff();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART4_UART_Init();

    for (unsigned sequence = 0;; ++sequence) {
        char line[192];
        int length = snprintf(line, sizeof(line), "DUAL_UART_PROBE seq=%u ms=%lu\r\n",
                              sequence, (unsigned long)HAL_GetTick());
        if (length > 0 && (unsigned)length < sizeof(line)) {
            HAL_StatusTypeDef tx2 = HAL_UART_Transmit(&huart2, (uint8_t *)line,
                                                       (uint16_t)length, 100);
            HAL_StatusTypeDef tx4 = HAL_UART_Transmit(&huart4, (uint8_t *)line,
                                                       (uint16_t)length, 100);
            int status_length = snprintf(line, sizeof(line),
                "UART_PROBE seq=%u tx2=%u tx4=%u err2=%lu err4=%lu sr2=%04lX sr4=%04lX brr2=%lu brr4=%lu pa2cfg=%lX pc10cfg=%lX\r\n",
                sequence, (unsigned)tx2, (unsigned)tx4,
                (unsigned long)huart2.ErrorCode, (unsigned long)huart4.ErrorCode,
                (unsigned long)USART2->SR, (unsigned long)UART4->SR,
                (unsigned long)USART2->BRR, (unsigned long)UART4->BRR,
                (unsigned long)((GPIOA->CRL >> 8U) & 15U),
                (unsigned long)((GPIOC->CRH >> 8U) & 15U));
            if (status_length > 0 && (unsigned)status_length < sizeof(line))
                (void)HAL_UART_Transmit(&huart1, (uint8_t *)line,
                                        (uint16_t)status_length, 100);
        }
        HAL_GPIO_TogglePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin);
        HAL_Delay(500);
    }
}
