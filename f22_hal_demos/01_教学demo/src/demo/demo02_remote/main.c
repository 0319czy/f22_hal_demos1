#include "../lesson_support.h"
#include "remote.h"
#include "radio.h"
#ifndef F22_REMOTE_FRAMED
#define F22_REMOTE_FRAMED 0
#endif
#ifndef F22_REMOTE_LOG_USB
#define F22_REMOTE_LOG_USB 0
#endif
#include <stdio.h>

/* ISR produces bytes; the main loop consumes them and owns the key state. */
static volatile uint8_t queue[64], head, tail, rx_error;
static uint8_t rx_byte;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uint8_t next = (uint8_t)((head + 1U) & 63U);
    if (next == tail) rx_error = 1U;
    else { queue[head] = rx_byte; head = next; }
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) rx_error = 1U;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == &huart1) rx_error = 1U;
}
#if !F22_REMOTE_FRAMED
static void LogKeys(const RemoteKeys *keys, uint8_t active, uint32_t errors)
{
    char text[128];
    int length = snprintf(text, sizeof(text),
        "key=%c recent=%u forward=%d right=%d up=%d W=%u X=%u stop=%u count=%lu err=%lu\r\n",
        keys->seen ? keys->last_key : '-', active, keys->forward, keys->right,
        keys->vertical, keys->calibrate_requested, keys->start_requested,
        keys->stop_requested, (unsigned long)keys->accepted, (unsigned long)errors);
    if (length > 0 && (unsigned)length < sizeof(text))
        (void)HAL_UART_Transmit(&huart4, (uint8_t *)text, (uint16_t)length, 20U);
}
#else
static void LogPacket(const RadioReceiver *r,uint8_t active,uint32_t errors)
{
    char text[100];
#if F22_REMOTE_LOG_USB
    /* Keep RF-bound USB diagnostics short; the host expands command names. */
    int n=snprintf(text,sizeof(text),"L%u C%u N%lu E%lu\n",
        active,r->command,(unsigned long)r->accepted,(unsigned long)errors);
    if(n>0 && (unsigned)n<sizeof(text))(void)HAL_UART_Transmit(&huart1,(uint8_t *)text,(uint16_t)n,100U);
#else
    int n=snprintf(text,sizeof(text),"packet_cmd=%u link=%u seq=%u frames=%lu rejected=%lu uart_err=%lu\r\n",
        r->command,active,r->sequence,(unsigned long)r->accepted,(unsigned long)r->rejected,(unsigned long)errors);
    if(n>0 && (unsigned)n<sizeof(text))(void)HAL_UART_Transmit(&huart4,(uint8_t *)text,(uint16_t)n,20U);
#endif
}
#endif
int main(void)
{
    GPIO_InitTypeDef gpio = {0};
    RemoteKeys keys;
#if F22_REMOTE_FRAMED
    RadioReceiver radio; Radio_Init(&radio);
#if F22_REMOTE_LOG_USB
    uint8_t logged_command = RADIO_HEARTBEAT;
#endif
#endif
    uint32_t logged_at = 0U, heartbeat_at = 0U, errors = 0U;
    Lesson_Init(); MX_USART4_UART_Init();
    /* E49 transparent mode, verified board pins PA6=M0, PA7=M1. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    RemoteKeys_Init(&keys);
    HAL_NVIC_SetPriority(USART1_IRQn, 2U, 0U); HAL_NVIC_EnableIRQ(USART1_IRQn);
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) Error_Handler();
    while (1) {
        uint32_t now = HAL_GetTick();
        if (rx_error) {
            uint32_t mask = __get_PRIMASK();
            __disable_irq();
            (void)HAL_UART_AbortReceive(&huart1);
            head = tail = rx_error = 0U;
            RemoteKeys_Init(&keys);
#if F22_REMOTE_FRAMED
            Radio_Init(&radio);
#endif
            if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) rx_error = 1U;
            __set_PRIMASK(mask);
            ++errors;
        }
        while (tail != head) {
            uint8_t byte = queue[tail];
            tail = (uint8_t)((tail + 1U) & 63U);
#if F22_REMOTE_FRAMED
            unsigned accepted = Radio_Feed(&radio,byte,now);
#if F22_REMOTE_LOG_USB
            /* Record command transitions before the next heartbeat overwrites them. */
            if (accepted && radio.command != RADIO_HEARTBEAT && radio.command != logged_command) {
                LogPacket(&radio, 1U, errors);
                logged_at = HAL_GetTick();
            }
            if (accepted) logged_command = radio.command;
#else
            (void)accepted;
#endif
#else
            (void)RemoteKeys_Feed(&keys, byte, now);
#endif
        }
#if F22_REMOTE_FRAMED
        uint8_t active = Radio_LinkValid(&radio,now);
#else
        uint8_t active = RemoteKeys_Update(&keys, now);
#endif
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                          active ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
#if F22_REMOTE_FRAMED && F22_REMOTE_LOG_USB
        now = HAL_GetTick();
        active = Radio_LinkValid(&radio,now);
        if ((uint32_t)(now - logged_at) >= 2000U) {
#else
        if ((uint32_t)(now - logged_at) >= 100U) {
#endif
#if F22_REMOTE_FRAMED
            LogPacket(&radio, active, errors);
#else
            LogKeys(&keys, active, errors);
#endif
            logged_at = now;
        }
        if ((uint32_t)(now - heartbeat_at) >= 500U) {
            HAL_GPIO_TogglePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin); heartbeat_at = now;
        }
        HAL_Delay(1U);
    }
}
