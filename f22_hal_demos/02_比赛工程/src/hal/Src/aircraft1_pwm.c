#include "aircraft1_pwm.h"
#include "common.h"
#include "board_f22.h"
#include "closed_loop_profile.h"
#include <math.h>
#ifndef AIRCRAFT1_PWM_MAX_DUTY
#define AIRCRAFT1_PWM_MAX_DUTY CL_MAX_DUTY
#endif
static volatile unsigned initialized, tripped, active;
static volatile uint32_t updated;
static void Zero(void)
{
    if (!initialized) return;
    TIM3->CCR1 = TIM3->CCR2 = TIM3->CCR3 = TIM3->CCR4 = 0;
    TIM4->CCR1 = TIM4->CCR2 = 0;
    TIM3->EGR = TIM_EGR_UG; TIM4->EGR = TIM_EGR_UG;
    active = 0U;
}
void Aircraft1Pwm_Trip(void) { Zero(); tripped = 1U; }
void Aircraft1Pwm_Clear(void)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    Zero(); tripped = 0U; __set_PRIMASK(mask);
}
unsigned Aircraft1Pwm_Tripped(void) { return tripped; }
void Aircraft1Pwm_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_TIM3_CLK_ENABLE(); __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_AFIO_REMAP_TIM3_ENABLE();
    TIM3->CR1 = TIM4->CR1 = 0;
    TIM3->CCER = TIM4->CCER = 0;
    /* 72MHz / (23+1) / (199+1) = 15kHz; all CCRs high-active. */
    TIM3->PSC = TIM4->PSC = 23;
    TIM3->ARR = TIM4->ARR = 199;
    TIM3->CCMR1 = (6U << 4) | TIM_CCMR1_OC1PE | (6U << 12) | TIM_CCMR1_OC2PE;
    TIM3->CCMR2 = (6U << 4) | TIM_CCMR2_OC3PE;
    TIM4->CCMR1 = (6U << 4) | TIM_CCMR1_OC1PE;
    TIM4->CCMR2 = 0;
    initialized = 1U; Zero(); tripped = 0U;
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
    TIM4->CCER = TIM_CCER_CC1E;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN; TIM4->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    /* Start at zero first, then route only PC6/7/8 and PB6. M4/M7 stay GPIO LOW. */
    gpio.Mode = GPIO_MODE_AF_PP; gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8; HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_6; HAL_GPIO_Init(GPIOB, &gpio);
}
unsigned Aircraft1Pwm_Apply(const float motor[4], uint32_t now)
{
    unsigned ticks[4], nonzero = 0;
    for (unsigned i = 0; i < 4; ++i) {
#if defined(FLOW_ID_PROBE) || CL_FLIGHT_OUTPUT_LOCKED
        /* Independent driver lock: a diagnostic must never energize a motor,
         * and a quarantined flight image cannot bypass the controller gate. */
        if (motor[i] != 0) { Aircraft1Pwm_Trip(); return 0; }
#endif
        if (!isfinite(motor[i]) || motor[i] < 0 || motor[i] > AIRCRAFT1_PWM_MAX_DUTY) {
            Aircraft1Pwm_Trip(); return 0;
        }
        ticks[i] = (unsigned)(motor[i] * 200.0f); nonzero |= ticks[i];
    }
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    if (!initialized || tripped) { Zero(); __set_PRIMASK(mask); return 0; }
    updated = now;
    TIM3->CCR1 = ticks[0]; TIM3->CCR2 = ticks[1]; TIM3->CCR3 = ticks[2];
    TIM4->CCR1 = ticks[3]; active = nonzero != 0;
    __set_PRIMASK(mask); return 1;
}
/* Stops PWM even when an I2C/SPI timeout blocks the main loop. */
void HAL_SYSTICK_Callback(void)
{
    if (!initialized || !active) return;
    /* 2026-10-28 实飞：6ms 节拍下偶发 I2C/SPI 读取超时会让主循环停顿超过
     * 20ms，22 次飞行里出现过 OUTPUT_FAULT 停机。放宽到 50ms 仍能覆盖真实
     * 卡死（正常周期 6ms 的 8 倍），减少误触发。 */
    if ((uint32_t)(HAL_GetTick() - updated) >= 50U ||
#ifndef AIRCRAFT1_STANDALONE
        HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET ||
#endif
        HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET)
        Aircraft1Pwm_Trip();
}
