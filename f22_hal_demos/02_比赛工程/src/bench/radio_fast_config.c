#include "radio_fast_config.h"
#include "usart.h"
#include <string.h>

static unsigned Parameters(uint8_t packet[6])
{
    const uint8_t query[3] = {0xc1, 0xc1, 0xc1};
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    if (HAL_UART_Transmit(&huart1, (uint8_t *)query, 3, 30) != HAL_OK) return 0;
    if (HAL_UART_Receive(&huart1, packet, 6, 400) != HAL_OK) return 0;
    return packet[0] == 0xc0 && packet[4] <= 0xc8 && (packet[5] & 0x7cU) == 0;
}
static void Mode(unsigned m1, unsigned m0)
{
    /* F22 manual 4.9: PA6=WL_M0, PA7=WL_M1. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, m0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, m1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(120);
}
void RadioFast_Configure(RadioFastConfig *result)
{
    memset(result, 0, sizeof(*result));
    huart1.Init.BaudRate = 9600; huart1.Init.Mode = UART_MODE_TX_RX;
    if (HAL_UART_Init(&huart1) != HAL_OK) return;
    Mode(1, 0);
    for (unsigned retry = 0; retry < 2 && !result->read_ok; ++retry)
        result->read_ok = Parameters(result->original);
    if (result->read_ok) {
        uint8_t candidate[6], echo[6];
        memcpy(candidate, result->original, 6); candidate[0] = 0xc2;
        /* Local UART9600 and RF50k. Address/channel/options remain unchanged. */
        candidate[3] = (uint8_t)((candidate[3] & 0xc0U) | 0x1dU);
#ifdef LATENCY_UART_FAST
        candidate[3] = 0x35; /* 8N1, UART57600, RF50k. */
#endif
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        if (HAL_UART_Transmit(&huart1, candidate, 6, 30) == HAL_OK) {
            (void)HAL_UART_Receive(&huart1, echo, 6, 400);
            HAL_Delay(30);
            if (Parameters(result->active) &&
                memcmp(result->active + 1, candidate + 1, 5) == 0) result->applied = 1;
        }
        if (!result->applied) {
            memcpy(candidate, result->original, 6); candidate[0] = 0xc2;
            __HAL_UART_CLEAR_OREFLAG(&huart1);
            if (HAL_UART_Transmit(&huart1, candidate, 6, 30) == HAL_OK) {
                (void)HAL_UART_Receive(&huart1, echo, 6, 400);
                HAL_Delay(30);
                result->rollback_ok = Parameters(result->active) &&
                    memcmp(result->active + 1, result->original + 1, 5) == 0;
            }
        }
    }
    Mode(0, 0);
    if (result->applied || result->rollback_ok) {
        static const uint32_t baud[8] = {1200,2400,4800,9600,19200,38400,57600,115200};
        huart1.Init.BaudRate = baud[(result->active[3] >> 3) & 7U];
    }
    huart1.Init.Mode = UART_MODE_RX;
    (void)HAL_UART_Init(&huart1);
}
