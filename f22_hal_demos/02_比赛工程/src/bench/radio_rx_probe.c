/* Receive-only radio probe. JP13 logs; optional USB mirror. Motors stay off. */
#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include "radio.h"
#include <stdio.h>
#ifndef RADIO_RX_LOG_USB
#define RADIO_RX_LOG_USB 1
#endif

static volatile uint8_t queue[256], head, tail, rx_byte;
static volatile uint8_t recent[16], recent_head;
static volatile uint32_t rx_bytes, uart_errors, overflows;

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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uint8_t byte = rx_byte, next = (uint8_t)(head + 1U);
    rx_bytes++;
    recent[recent_head] = byte;
    recent_head = (uint8_t)((recent_head + 1U) & 15U);
    if (next == tail) overflows++;
    else { queue[head] = byte; head = next; }
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U) != HAL_OK)
        uart_errors++;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uart_errors++;
}

int main(void)
{
    RadioReceiver radio;
    Radio_Init(&radio);
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    MotorsOff();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
#if !RADIO_RX_LOG_USB
    /* USART1 TX also feeds E49: keep telemetry exclusively on JP13. */
    huart1.Init.Mode = UART_MODE_RX;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
#endif

    /* E49 transparent mode: M0=0 and M1=0. Never write its saved settings. */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U);

    uint32_t logged = HAL_GetTick();
    uint32_t recovered_errors = uart_errors;
    unsigned tx2_errors = 0;
    for (;;) {
        if (recovered_errors != uart_errors) {
            recovered_errors = uart_errors;
            (void)HAL_UART_AbortReceive(&huart1);
            (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U);
            radio.used = radio.seen = 0;
        }
        while (tail != head) {
            uint8_t byte = queue[tail];
            tail = (uint8_t)(tail + 1U);
            (void)Radio_Feed(&radio, byte, HAL_GetTick());
        }
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - logged) >= 1000U) {
            uint8_t raw[16], index;
            __disable_irq();
            index = recent_head;
            for (unsigned i = 0; i < 16U; ++i) raw[i] = recent[(index + i) & 15U];
            __enable_irq();
            char hex[33], line[192];
            for (unsigned i = 0; i < 16U; ++i)
                (void)snprintf(hex + 2U*i, 3U, "%02X", (unsigned)raw[i]);
            int length = snprintf(line, sizeof(line),
                "RADIO_RX bytes=%lu frames=%u reject=%u err=%lu overflow=%lu link=%u cmd=%u tx2_err=%u raw=%s\r\n",
                (unsigned long)rx_bytes, radio.accepted, radio.rejected,
                (unsigned long)uart_errors, (unsigned long)overflows,
                Radio_LinkValid(&radio, now), (unsigned)radio.command, tx2_errors, hex);
            if (length > 0 && (unsigned)length < sizeof(line))
                if (HAL_UART_Transmit(&huart2, (uint8_t *)line, (uint16_t)length, 100U) != HAL_OK)
                    tx2_errors++;
#if RADIO_RX_LOG_USB
            if (length > 0 && (unsigned)length < sizeof(line))
                (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 200U);
#endif
            logged = now;
        }
    }
}
