#include "common.h"
#include "gpio.h"
#include "system_clock.h"

/* Dedicated propeller-free bench: PC6..PC9, TIM3 CH1..4, 15 kHz, 15%. */
#define MOTOR_PINS (BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin)

static void MotorsOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM3->CCER = 0U;
    TIM3->CR1 = 0U;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = MOTOR_PINS;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOB, &gpio);
}

void Error_Handler(void)
{
    MotorsOff();
    __disable_irq();
    for (;;) { }
}

void HardFault_Handler(void)
{
    MotorsOff();
    __disable_irq();
    for (;;) { }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}

static void MotorsPwm15(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM3_FORCE_RESET();
    __HAL_RCC_TIM3_RELEASE_RESET();
    /* SystemClock_Config provides a 72 MHz TIM3 clock.
     * 72MHz / (23+1) / (199+1) = 15kHz; 30 / 200 = 15%. */
    TIM3->PSC = 23U;
    TIM3->ARR = 199U;
    TIM3->CCR1 = TIM3->CCR2 = TIM3->CCR3 = TIM3->CCR4 = 30U;
    TIM3->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE |
                   TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
    TIM3->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE |
                   TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    __HAL_AFIO_REMAP_TIM3_ENABLE();
    gpio.Pin = MOTOR_PINS;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio);
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM3->CR1 |= TIM_CR1_CEN;
}

int main(void)
{
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    MotorsOff();
    SystemClock_Config();
    MX_GPIO_Init();
    uint32_t started = HAL_GetTick();
    unsigned cancelled = 0U;
    while ((uint32_t)(HAL_GetTick() - started) < 2000U) {
        if (HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET)
            cancelled = 1U;
        HAL_Delay(1U);
    }
    if (!cancelled && HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) != GPIO_PIN_RESET)
        MotorsPwm15();
    for (;;) {
        if (HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET)
            MotorsOff(); /* Latched stop: release does not restart. */
        HAL_Delay(1U);
    }
}
