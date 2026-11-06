/* Propeller-free radio bench. USART1 RX9600, independent JP13 TX115200. */
#include "common.h"
#include "system_clock.h"
#include "gpio.h"
#include "usart.h"
#include "aircraft1_pwm.h"
#include "motor_radio.h"
#include <stdio.h>
#ifndef MOTOR_RADIO_LOG_USB
#define MOTOR_RADIO_LOG_USB 0
#endif
#if MOTOR_RADIO_LOG_USB
#define MOTOR_LOG_UART (&huart1)
#define MOTOR_LOG_PERIOD_MS 2000U
#else
#define MOTOR_LOG_UART (&huart2)
#define MOTOR_LOG_PERIOD_MS 200U
#endif

static volatile uint8_t rx_head, rx_tail, rx_error;
static volatile uint8_t rx_queue[256];
static volatile uint32_t rx_times[256];
static uint8_t rx_byte;
static volatile uint32_t rx_bytes;
static volatile uint8_t rx_raw[16], rx_raw_head;
static uint8_t e49_config[6];
static unsigned config_ok;
static unsigned uart_errors;
static char telemetry[512];
static volatile unsigned telemetry_busy;
static uint32_t telemetry_started;
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{ if (uart == MOTOR_LOG_UART) telemetry_busy = 0; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    rx_bytes++; rx_raw[rx_raw_head] = rx_byte;
    rx_raw_head = (uint8_t)((rx_raw_head + 1U) & 15U);
    uint8_t next = (uint8_t)(rx_head + 1U);
    if (next == rx_tail) { rx_error = 1; Aircraft1Pwm_Trip(); }
    else { rx_queue[rx_head] = rx_byte; rx_times[rx_head] = HAL_GetTick(); rx_head = next; }
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1) != HAL_OK) {
        rx_error = 1; Aircraft1Pwm_Trip();
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == &huart1) { rx_error = 1; Aircraft1Pwm_Trip(); }
}
static void ReceiveReset(void)
{
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    (void)HAL_UART_AbortReceive(&huart1);
    rx_head = rx_tail = rx_error = 0;
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1) != HAL_OK) rx_error = 1;
    __set_PRIMASK(mask);
}
static void Send(const MotorRadio *c, uint32_t now)
{
    if (telemetry_busy) {
        if ((uint32_t)(now - telemetry_started) < 100U) return;
        (void)HAL_UART_AbortTransmit(MOTOR_LOG_UART);
        telemetry_busy = 0;
    }
    char raw_hex[33]; uint8_t raw[16], head;
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    head = rx_raw_head;
    for (unsigned i = 0; i < 16; ++i) raw[i] = rx_raw[(head + i) & 15U];
    __set_PRIMASK(mask);
    for (unsigned i = 0; i < 16; ++i) (void)snprintf(raw_hex + 2*i, 3, "%02X", (unsigned)raw[i]);
#if MOTOR_RADIO_LOG_USB
    int n;
    if (!c->receiver.accepted) {
        n = snprintf(telemetry, sizeof(telemetry),
            "L0 N0 B%lu R%u E%u P0 F%u X%s\r\n",
            (unsigned long)rx_bytes, c->receiver.rejected, uart_errors,
            c->stop_reason, raw_hex);
    } else {
        n = snprintf(telemetry, sizeof(telemetry),
            "L%u C%u N%u B%lu E%u P%u F%u G%lu\r\n",
            Radio_LinkValid(&c->receiver, now), (unsigned)c->receiver.command,
            c->receiver.accepted, (unsigned long)rx_bytes, uart_errors,
            (unsigned)(TIM3->CCR1 * 5U), c->stop_reason, (unsigned long)c->max_gap_ms);
    }
#else
    int n = snprintf(telemetry, sizeof(telemetry),
        "[MOTOR50 RADIO] running=%u link=%u age_ms=%lu packets=%u reject=%u "
        "seq_missing=%u gap_ms=%lu max_gap_ms=%lu uart_err=%u cmd=%u "
        "p1_requests=%u starts=%u disconnects=%u stop_reason=%u trip=%u "
        "rx_bytes=%lu raw=%s mode=%u%u rxpin=%u cfg_ok=%u "
        "cfg=%02X%02X%02X%02X%02X%02X brr=%lu "
        "PWM_permille_M1_M2_M3_M6=%u,%u,%u,%u\r\n",
        c->motor.state == CL_HOLD, Radio_LinkValid(&c->receiver, now),
        (unsigned long)(c->receiver.seen ? now - c->receiver.last_packet_ms : UINT32_MAX),
        c->receiver.accepted, c->receiver.rejected, c->sequence_missing,
        (unsigned long)c->last_gap_ms, (unsigned long)c->max_gap_ms, uart_errors,
        (unsigned)c->receiver.command, c->start_requests, c->starts, c->disconnects,
        c->stop_reason, Aircraft1Pwm_Tripped(), (unsigned long)rx_bytes, raw_hex,
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET,
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET,
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET,
        config_ok, (unsigned)e49_config[0], (unsigned)e49_config[1],
        (unsigned)e49_config[2], (unsigned)e49_config[3],
        (unsigned)e49_config[4], (unsigned)e49_config[5], (unsigned long)USART1->BRR,
        (unsigned)(TIM3->CCR1 * 5U),
        (unsigned)(TIM3->CCR2 * 5U), (unsigned)(TIM3->CCR3 * 5U), (unsigned)(TIM4->CCR1 * 5U));
#endif
    if (n <= 0 || (unsigned)n >= sizeof(telemetry)) return;
    telemetry_busy = 1;
    telemetry_started = now;
    if (HAL_UART_Transmit_IT(MOTOR_LOG_UART, (uint8_t *)telemetry, (uint16_t)n) != HAL_OK)
        telemetry_busy = 0;
}
static void Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_USART1_UART_Init(); MX_USART2_UART_Init();
#if !MOTOR_RADIO_LOG_USB
    static const uint8_t early_jp13[] = "\r\nJP13:UART2_READY\r\n";
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)early_jp13, sizeof(early_jp13)-1U, 20);
#endif
#if MOTOR_RADIO_LOG_USB
    static const uint8_t early[] = "BOOT_USB_9600\r\n";
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)early, sizeof(early)-1U, 20);
#endif
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_8;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOA, &gpio);
    /* E49 mode0 transparent RX. Existing channel/address configuration is preserved. */
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOA, &gpio);
    Aircraft1Pwm_Init();
#if !MOTOR_RADIO_LOG_USB
    static const uint8_t pwm_ready[] = "JP13:PWM_ZERO\r\n";
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)pwm_ready, sizeof(pwm_ready)-1U, 20);
#endif
    /* E49 manual 6.4/7.2: mode2 M0=0/M1=1, C1 C1 C1 reads saved
     * settings. Boot-only, motors OFF; never issue C0/C2 configuration writes. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); HAL_Delay(100);
    static const uint8_t query[] = {0xc1, 0xc1, 0xc1};
    if (HAL_UART_Transmit(&huart1, (uint8_t *)query, sizeof(query), 20) == HAL_OK &&
        HAL_UART_Receive(&huart1, e49_config, sizeof(e49_config), 200) == HAL_OK &&
        e49_config[0] == 0xc0) config_ok = 1;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET); HAL_Delay(100);
    huart1.Init.Mode = MOTOR_RADIO_LOG_USB ? UART_MODE_TX_RX : UART_MODE_RX;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler(); /* No runtime TX into E49. */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0); HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0); HAL_NVIC_EnableIRQ(USART2_IRQn);
    ReceiveReset();
#if MOTOR_RADIO_LOG_USB
    static const uint8_t ready[] = "READY_USB_9600\r\n";
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)ready, sizeof(ready)-1U, 20);
#endif
#if !MOTOR_RADIO_LOG_USB
    static const uint8_t boot[] =
        "\r\n[BOOT] MOTOR50 RADIO F1/P1=start F3/F4=STOP KEY1/KEY2=STOP JP13=115200 motors=OFF\r\n";
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)boot, sizeof(boot)-1U, 20);
#endif
}
int main(void)
{
    MotorRadio control; MotorRadio_Init(&control); Init(); uint32_t logged = HAL_GetTick();
    while (1) {
        float motor[4]; unsigned error = rx_error;
        if (error) {
            uart_errors++; ReceiveReset();
            control.receiver.used = control.receiver.seen = 0; control.start_allowed = 0;
        }
        while (rx_tail != rx_head) {
            uint8_t byte = rx_queue[rx_tail]; uint32_t arrived = rx_times[rx_tail];
            rx_tail = (uint8_t)(rx_tail + 1U); MotorRadio_Feed(&control, byte, arrived);
        }
        uint32_t now = HAL_GetTick(); /* Sample after draining arrival timestamps. */
        unsigned stop = HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET ||
                        HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET;
        MotorRadio_Step(&control, stop, Aircraft1Pwm_Tripped(), error, now, motor);
        if (control.clear_output) Aircraft1Pwm_Clear();
        if (!Aircraft1Pwm_Apply(motor, HAL_GetTick()))
            MotorRadio_Step(&control, 0, 1, 0, HAL_GetTick(), motor);
        if ((uint32_t)(now - logged) >= MOTOR_LOG_PERIOD_MS) { Send(&control, now); logged = now; }
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
            Radio_LinkValid(&control.receiver, now) ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
        HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin,
            control.motor.state == CL_HOLD ? BOARD_LED2_ON_LEVEL : BOARD_LED2_OFF_LEVEL);
        HAL_Delay(5);
    }
}
