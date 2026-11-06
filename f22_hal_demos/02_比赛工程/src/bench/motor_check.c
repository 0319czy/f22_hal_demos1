#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include "tim.h"
#include "motor_bench.h"
#include <string.h>
#ifndef BENCH_OUTPUT_ENABLE
#define BENCH_OUTPUT_ENABLE 0
#endif
#ifndef BENCH_MOTOR_INDEX
#define BENCH_MOTOR_INDEX 1U
#endif
_Static_assert(BENCH_MOTOR_INDEX == 1U || BENCH_MOTOR_INDEX == 2U ||
               BENCH_MOTOR_INDEX == 3U || BENCH_MOTOR_INDEX == 4U ||
               BENCH_MOTOR_INDEX == 6U || BENCH_MOTOR_INDEX == 7U, "M5 is not a rotor");
#if BENCH_OUTPUT_ENABLE
#ifndef BOARD_SINGLE_MOTOR_INDEX
#error "Physical bench requires single-pin timer routing"
#endif
_Static_assert(BOARD_SINGLE_MOTOR_INDEX == BENCH_MOTOR_INDEX, "Timer route must match bench motor");
#endif
static void MotorPinsStop(void)
{
    /* F22 v2.0: N-channel low-side MOSFET with gate pulldown.
     * GPIO low and active-high PWM CCR=0 both switch the motor off. */
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0}; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOB, &gpio);
}
static volatile uint8_t queue[256], head, tail, rx_error;
static volatile uint32_t arrived[256];
static uint8_t rx_byte;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *u)
{
    if (u != &huart1) return;
    uint8_t next = (uint8_t)(head + 1U);
    if (next == tail) rx_error = 1U;
    else { queue[head] = rx_byte; arrived[head] = HAL_GetTick(); head = next; }
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) rx_error = 1U;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *u) { if (u == &huart1) rx_error = 1U; }
#if BENCH_OUTPUT_ENABLE
static void PwmStart(void)
{
    uint32_t begin = HAL_GetTick(); RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) if ((uint32_t)(HAL_GetTick() - begin) > 100U) Error_Handler();
    IWDG->KR = 0x5555U; IWDG->PR = 3U; IWDG->RLR = 625U;
    while (IWDG->SR) if ((uint32_t)(HAL_GetTick() - begin) > 100U) Error_Handler();
    IWDG->KR = 0xAAAAU; IWDG->KR = 0xCCCCU;
    TIM_HandleTypeDef *timer; uint32_t channel;
    if (BENCH_MOTOR_INDEX <= 4U) {
        MX_TIM3_Init(); timer = &htim3; channel = (BENCH_MOTOR_INDEX - 1U) * 4U;
    } else { MX_TIM4_Init(); timer = &htim4; channel = (BENCH_MOTOR_INDEX - 6U) * 4U; }
    if (Board_MotorPwm_SetDuty(BENCH_MOTOR_INDEX, 0U) != HAL_OK ||
        HAL_TIM_PWM_Start(timer, channel) != HAL_OK) Error_Handler();
    /* Route only the selected pin after its timer already drives zero duty. */
    HAL_TIM_MspPostInit(timer);
}
#endif
int main(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init(); MotorPinsStop(); SystemClock_Config(); MX_GPIO_Init();
    MX_USART1_UART_Init(); MX_USART4_UART_Init();
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOA, &gpio);
    MotorBench bench; MotorBench_Init(&bench); unsigned errors = 0;
    HAL_NVIC_SetPriority(USART1_IRQn, 2U, 0U); HAL_NVIC_EnableIRQ(USART1_IRQn);
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) Error_Handler();
#if BENCH_OUTPUT_ENABLE
    PwmStart();
#endif
    uint32_t logged = HAL_GetTick();
    while (1) {
        if (rx_error) {
            uint32_t mask = __get_PRIMASK(); __disable_irq();
            (void)HAL_UART_AbortReceive(&huart1); head = tail = rx_error = 0U;
            MotorBench_Init(&bench); ++errors;
            if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) rx_error = 1U;
            __set_PRIMASK(mask);
        }
        while (tail != head) {
            uint8_t byte = queue[tail]; uint32_t time = arrived[tail];
            tail = (uint8_t)(tail + 1U); MotorBench_Feed(&bench, byte, time);
        }
        uint32_t now = HAL_GetTick();
        unsigned key1 = HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET;
        unsigned key2 = HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET;
        unsigned requested = MotorBench_Step(&bench, key1, key2 || rx_error, now), actual = 0;
#if BENCH_OUTPUT_ENABLE
        actual = requested;
        if (Board_MotorPwm_SetDuty(BENCH_MOTOR_INDEX, actual) != HAL_OK) {
            (void)Board_MotorPwm_SetDuty(BENCH_MOTOR_INDEX, 0U); Error_Handler();
        }
        IWDG->KR = 0xAAAAU;
#endif
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                          requested ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
        HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin,
                          Radio_LinkValid(&bench.radio, now) ? BOARD_LED2_ON_LEVEL : BOARD_LED2_OFF_LEVEL);
        if ((uint32_t)(now - logged) >= 100U) {
            float c[9] = {bench.state, requested, actual, BENCH_MOTOR_INDEX,
                BENCH_OUTPUT_ENABLE, Radio_LinkValid(&bench.radio, now),
                bench.radio.accepted, bench.radio.rejected, errors};
            uint8_t frame[40]; const uint8_t end[] = {0,0,0x80,0x7f};
            _Static_assert(sizeof(float) == 4, "JustFloat needs float32");
            memcpy(frame, c, 36); memcpy(frame + 36, end, 4);
            (void)HAL_UART_Transmit(&huart4, frame, sizeof(frame), 20U); logged = now;
        }
        HAL_Delay(1U);
    }
}
