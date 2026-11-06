#include "../lesson_support.h"
#include "motor_guard.h"
#include "tim.h"
#ifndef MOTOR_BENCH_ENABLE
#define MOTOR_BENCH_ENABLE 0
#endif
#ifndef MOTOR_TEST_INDEX
#define MOTOR_TEST_INDEX 1U
#endif
_Static_assert(MOTOR_TEST_INDEX == 1U || MOTOR_TEST_INDEX == 2U ||
               MOTOR_TEST_INDEX == 3U || MOTOR_TEST_INDEX == 4U ||
               MOTOR_TEST_INDEX == 6U || MOTOR_TEST_INDEX == 7U,
               "Select one coreless motor; M5 is not a rotor output");
#if MOTOR_BENCH_ENABLE
static void Watchdog_Start(void)
{
    uint32_t start = HAL_GetTick();
    RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) {
        if ((uint32_t)(HAL_GetTick() - start) > 100U) Error_Handler();
    }
    IWDG->KR = 0x5555U; IWDG->PR = 3U; IWDG->RLR = 625U;
    while (IWDG->SR != 0U) {
        if ((uint32_t)(HAL_GetTick() - start) > 100U) Error_Handler();
    }
    IWDG->KR = 0xAAAAU; IWDG->KR = 0xCCCCU;
}
static void Bench_Start(void)
{
    TIM_HandleTypeDef *timer;
    uint32_t channel;
    Watchdog_Start();
    if (MOTOR_TEST_INDEX <= 4U) {
        MX_TIM3_Init(); timer = &htim3;
        channel = (MOTOR_TEST_INDEX - 1U) * 4U;
    } else {
        MX_TIM4_Init(); timer = &htim4;
        channel = (MOTOR_TEST_INDEX - 6U) * 4U;
    }
    if (Board_MotorPwm_SetDuty(MOTOR_TEST_INDEX, 0U) != HAL_OK ||
        HAL_TIM_PWM_Start(timer, channel) != HAL_OK) Error_Handler();
}
#endif
int main(void)
{
    MotorGuard guard;
    uint32_t reported_at = 0U;
    Lesson_Init(); MotorGuard_Init(&guard);
#if MOTOR_BENCH_ENABLE
    Bench_Start();
#endif
    while (1) {
        uint32_t now = HAL_GetTick();
        uint8_t requested = MotorGuard_Update(&guard, Lesson_Key1(), Lesson_Key2(), now);
        uint8_t actual = 0U;
#if MOTOR_BENCH_ENABLE
        actual = requested;
        if (Board_MotorPwm_SetDuty(MOTOR_TEST_INDEX, actual) != HAL_OK) {
            (void)Board_MotorPwm_SetDuty(MOTOR_TEST_INDEX, 0U); Error_Handler();
        }
        IWDG->KR = 0xAAAAU;
#endif
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                          requested ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
        if ((uint32_t)(now - reported_at) >= 100U) {
            float ch[] = {(float)guard.state, (float)requested, (float)actual,
                          (float)MOTOR_TEST_INDEX, (float)MOTOR_BENCH_ENABLE};
            (void)Lesson_SendFloats(ch, 5U); reported_at = now;
        }
        HAL_Delay(10U);
    }
}
