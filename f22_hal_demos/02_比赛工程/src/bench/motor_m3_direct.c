#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#ifdef BENCH_DIRECT_DIAGNOSTICS
#include "usart.h"
#include <stdio.h>
#endif

#ifndef BENCH_DIRECT_QUAD
#define BENCH_DIRECT_QUAD 0
#endif
#ifndef BENCH_DIRECT_START_DELAY_MS
#define BENCH_DIRECT_START_DELAY_MS 0U
#endif
#if BENCH_DIRECT_QUAD
#define DIRECT_PINS (BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin)
#else
#define DIRECT_PINS BOARD_M3_Pin
#endif

#ifdef BENCH_DIRECT_DIAGNOSTICS
static void Direct_Log(const char *phase)
{
    char text[220];
    const uint32_t odr = GPIOC->ODR, idr = GPIOC->IDR, crh = GPIOC->CRH;
    const uint32_t cfg = (crh >> 4U) & 15U; /* PC9 configuration nibble. */
    int n = snprintf(text, sizeof(text),
        "[M4] ms=%lu phase=%s key2=%u ODR=0x%04lX IDR=0x%04lX "
        "PC9_cmd=%u PC9_read=%u PC9_mode=%lu PC9_cnf=%lu\r\n",
        (unsigned long)HAL_GetTick(), phase,
        (unsigned)(HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET),
        (unsigned long)odr, (unsigned long)idr,
        (unsigned)((odr & BOARD_M4_Pin) != 0U), (unsigned)((idr & BOARD_M4_Pin) != 0U),
        (unsigned long)(cfg & 3U), (unsigned long)(cfg >> 2U));
    if (n > 0) {
        uint16_t len = (uint16_t)((unsigned)n < sizeof(text) ? (unsigned)n : sizeof(text) - 1U);
        /* USB CH340 and JP13 carry the same text. External RX needs matching logic levels. */
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, len, 25U);
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, len, 25U);
    }
}
#else
#define Direct_Log(phase) ((void)0)
#endif

/* Explicitly requested, propeller-free circuit test: full DC on selected motors.
 * This is an isolated test entry, never the default competition entry. */
static void MotorPinsOff(void)
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

void Error_Handler(void)
{
    MotorPinsOff();
    __disable_irq();
    for (;;) { }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}

void HardFault_Handler(void)
{
    /* These clocks were enabled before M3 could turn on. No UART dependency. */
    GPIOC->BRR = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
    GPIOB->BRR = BOARD_M6_Pin | BOARD_M7_Pin;
    __disable_irq();
    for (;;) { }
}

int main(void)
{
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    MotorPinsOff();
    SystemClock_Config();
    MX_GPIO_Init();
#ifdef BENCH_DIRECT_DIAGNOSTICS
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
#endif
    HAL_Delay(10U);

    /* F22 PC6..PC9 -> gates of low-side N-MOSFETs. HIGH = full conduction.
     * Both LED supply outputs stay off; no wireless or PWM is initialized. */
#if BENCH_DIRECT_START_DELAY_MS > 0U
    uint32_t started = HAL_GetTick();
    uint8_t cancelled = 0U;
    Direct_Log("WAIT");
    while ((uint32_t)(HAL_GetTick() - started) < BENCH_DIRECT_START_DELAY_MS) {
        if (HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET) {
            cancelled = 1U;
        }
        HAL_Delay(1U);
    }
    if (!cancelled && HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) != GPIO_PIN_RESET) {
#else
    if (HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) != GPIO_PIN_RESET) {
#endif
        /* One BSRR write asserts all four outputs together in the quad build. */
        HAL_GPIO_WritePin(GPIOC, DIRECT_PINS, GPIO_PIN_SET);
    }
    Direct_Log((GPIOC->ODR & DIRECT_PINS) != 0U ? "ON" : "STOP");
#ifdef BENCH_DIRECT_DIAGNOSTICS
    uint32_t logged = HAL_GetTick();
#endif
    for (;;) {
        if (HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET) {
            /* Latched off: releasing KEY2 does not restart the motor. */
            HAL_GPIO_WritePin(GPIOC, DIRECT_PINS, GPIO_PIN_RESET);
        }
#ifdef BENCH_DIRECT_DIAGNOSTICS
        if ((uint32_t)(HAL_GetTick() - logged) >= 500U) {
            Direct_Log((GPIOC->ODR & DIRECT_PINS) != 0U ? "ON" : "STOP");
            logged = HAL_GetTick();
        }
#endif
        HAL_Delay(1U);
    }
}
