/* JP13 pin 2 -> pin 3 loopback. All motor outputs remain low. */
#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

static volatile unsigned received, rx_error;

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart2) received = 1; }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart2) rx_error = uart->ErrorCode; }

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
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    for (unsigned sequence = 0;; ++sequence) {
        uint8_t sent[] = "JP13_LOOP_0123\r\n";
        uint8_t got[sizeof(sent)-1U] = {0};
        sent[10] = (uint8_t)('0' + sequence % 10U);
        (void)HAL_UART_AbortReceive(&huart2);
        __HAL_UART_CLEAR_OREFLAG(&huart2);
        received = rx_error = 0;
        HAL_StatusTypeDef arm = HAL_UART_Receive_IT(&huart2, got, sizeof(got));
        HAL_StatusTypeDef tx = HAL_UART_Transmit(&huart2, sent, sizeof(got), 100U);
        uint32_t started = HAL_GetTick();
        while (!received && !rx_error && (uint32_t)(HAL_GetTick()-started) < 200U) { }
        unsigned count = sizeof(got) - huart2.RxXferCount;
        unsigned match = received && memcmp(sent, got, sizeof(got)) == 0;
        (void)HAL_UART_AbortReceive(&huart2);
        char hex[2*sizeof(got)+1], line[160];
        for (unsigned i=0; i<sizeof(got); ++i)
            (void)snprintf(hex+2*i, 3, "%02X", (unsigned)got[i]);
        int length = snprintf(line, sizeof(line),
            "JP13_LOOP seq=%u tx=%u arm=%u rx=%u/%u match=%u err=%u raw=%s\r\n",
            sequence, (unsigned)tx, (unsigned)arm, count, (unsigned)sizeof(got),
            match, rx_error, hex);
        if (length > 0 && (unsigned)length < sizeof(line))
            (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 200U);
        HAL_Delay(1000U);
    }
}
