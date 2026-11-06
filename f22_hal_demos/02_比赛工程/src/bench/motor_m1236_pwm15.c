/* Dedicated propeller-free constant-duty bench: no sensor/flight gates. */
#include "common.h"
#include "system_clock.h"
#include "gpio.h"
#include "usart.h"
#include "aircraft1_pwm.h"
#include "motor_continuous.h"
#include "standalone_keys.h"
#include <stdio.h>

static char telemetry[256];
static volatile unsigned telemetry_busy;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart1) telemetry_busy = 0; }

static void Send(unsigned running, unsigned tripped)
{
    if (telemetry_busy) return;
    int n = snprintf(telemetry, sizeof(telemetry),
        "[MOTOR15 LOCAL] mode=FIXED running=%u trip=%u "
        "PWM_permille_M1_M2_M3_M6=%u,%u,%u,%u\r\n",
        running, tripped, (unsigned)(TIM3->CCR1 * 5U),
        (unsigned)(TIM3->CCR2 * 5U), (unsigned)(TIM3->CCR3 * 5U),
        (unsigned)(TIM4->CCR1 * 5U));
    if (n <= 0 || (unsigned)n >= sizeof(telemetry)) return;
    telemetry_busy = 1;
    if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)telemetry, (uint16_t)n) != HAL_OK)
        telemetry_busy = 0;
}

static void Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_USART1_UART_Init();
    /* Radio mode 3, no remote receive; USB remains the diagnostic UART. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOA, &gpio);
    /* Hold M4/M5/M7 LOW and start selected timers at zero before routing AF. */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7; HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_8; HAL_GPIO_Init(GPIOA, &gpio);
    Aircraft1Pwm_Init();
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0); HAL_NVIC_EnableIRQ(USART1_IRQn);
    static const uint8_t boot[] =
        "\r\n[BOOT] MOTOR15 FIXED M1/M2/M3/M6 KEY1=toggle KEY2=STOP USB115200 motors=OFF\r\n";
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)boot, sizeof(boot)-1U, 20);
}

int main(void)
{
    MotorContinuous control; StandaloneKeys keys;
    Init(); MotorContinuous_Init(&control); StandaloneKeys_Init(&keys);
    uint32_t logged = HAL_GetTick();
    while (1) {
        uint32_t now = HAL_GetTick(); float motor[4];
        HoverCommand command = StandaloneKeys_Step(&keys,
            HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET,
            HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET,
            control.state, now);
        if (command == HOVER_STOP || command == HOVER_REQUEST_LAND)
            Aircraft1Pwm_Clear();
        MotorContinuous_Step(&control, command, Aircraft1Pwm_Tripped(), motor);
        if (!Aircraft1Pwm_Apply(motor, HAL_GetTick()))
            MotorContinuous_Step(&control, HOVER_NONE, 1, motor);
        if ((uint32_t)(now - logged) >= 200U) {
            Send(control.state == CL_HOLD, Aircraft1Pwm_Tripped()); logged = now;
        }
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                         control.state == CL_HOLD ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
        HAL_Delay(5);
    }
}
