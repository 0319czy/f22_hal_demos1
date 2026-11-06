/* Isolate USART2 setup, baud rate and polarity from the JP13 wiring.
 * All motor pins stay low. PA2 also emits software UART test frames. */
#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

static void MotorsOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOB, &gpio);
}

static void Usb(const char *line)
{ (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)strlen(line), 300U); }

static void SoftLevel(unsigned value, unsigned inverted)
{ HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, (value ^ inverted) ? GPIO_PIN_SET : GPIO_PIN_RESET); }

static void SoftByte(uint8_t byte, unsigned inverted)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    const uint32_t bit_cycles = SystemCoreClock / 9600U;
    uint32_t start = DWT->CYCCNT;
    for (unsigned bit = 0; bit < 10U; ++bit) {
        unsigned value = bit == 0U ? 0U : bit == 9U ? 1U : (byte >> (bit-1U)) & 1U;
        SoftLevel(value, inverted);
        while ((uint32_t)(DWT->CYCCNT - start) < bit_cycles) { }
        start += bit_cycles;
    }
    __set_PRIMASK(mask);
}

int main(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init(); MotorsOff(); SystemClock_Config(); MX_GPIO_Init(); MX_USART1_UART_Init();
    uint32_t initial_mapr = AFIO->MAPR;
    __HAL_AFIO_REMAP_USART2_DISABLE();
    MX_USART2_UART_Init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    char line[220];
    (void)snprintf(line, sizeof(line), "AUDIT_BOOT mapr_before=%08lX mapr_now=%08lX core=%lu pclk1=%lu\r\n",
                   (unsigned long)initial_mapr, (unsigned long)AFIO->MAPR,
                   (unsigned long)SystemCoreClock, (unsigned long)HAL_RCC_GetPCLK1Freq());
    Usb(line);

    static const char *mode[] = {"HW115200", "HW9600", "SW9600", "SW9600_INVERTED"};
    for (unsigned cycle = 0;; ++cycle) {
        for (unsigned phase = 0; phase < 4U; ++phase) {
            unsigned baud = phase == 0U ? 115200U : 9600U;
            unsigned inverted = phase == 3U;
            if (phase < 2U) {
                (void)HAL_UART_DeInit(&huart2);
                __HAL_AFIO_REMAP_USART2_DISABLE();
                huart2.Init.BaudRate = baud;
                if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
            } else {
                (void)HAL_UART_DeInit(&huart2);
                GPIO_InitTypeDef gpio = {0};
                gpio.Pin = GPIO_PIN_2; gpio.Mode = GPIO_MODE_OUTPUT_PP;
                gpio.Speed = GPIO_SPEED_FREQ_HIGH;
                SoftLevel(1U, inverted); HAL_GPIO_Init(GPIOA, &gpio);
            }
            (void)snprintf(line, sizeof(line),
                "MODE %s baud=%u cycle=%u mapr=%08lX pa2cfg=%lX brr=%lu cr1=%04lX\r\n",
                mode[phase], baud, cycle, (unsigned long)AFIO->MAPR,
                (unsigned long)((GPIOA->CRL >> 8U) & 15U),
                (unsigned long)USART2->BRR, (unsigned long)USART2->CR1);
            Usb(line); HAL_Delay(400U);
            for (unsigned n = 0; n < 6U; ++n) {
                char payload[64];
                int length = snprintf(payload, sizeof(payload), "JP13_TEST %s cycle=%u n=%u\r\n", mode[phase], cycle, n);
                unsigned status = 0U;
                if (phase < 2U) status = (unsigned)HAL_UART_Transmit(&huart2, (uint8_t *)payload, (uint16_t)length, 100U);
                else for (int i=0; i<length; ++i) SoftByte((uint8_t)payload[i], inverted);
                (void)snprintf(line, sizeof(line),
                    "SEND %s n=%u status=%u sr=%04lX pa2=%u\r\n", mode[phase], n, status,
                    (unsigned long)USART2->SR,
                    HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET);
                Usb(line); HAL_Delay(500U);
            }
        }
    }
}
