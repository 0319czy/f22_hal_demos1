/* JP13/DAP uplink diagnostic. Motors are never initialized or driven.
 * Bytes received on USART2 are echoed as readable text on USART1/CH340. */
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include <stdio.h>

static volatile uint8_t rx_queue[256];
static volatile uint8_t rx_head, rx_tail, rx_byte;

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return;
    uint8_t next = (uint8_t)(rx_head + 1U);
    if (next != rx_tail) {
        rx_queue[rx_head] = rx_byte;
        rx_head = next;
    }
    (void)HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_byte, 1U);
}

int main(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    HAL_NVIC_SetPriority(USART2_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    (void)HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_byte, 1U);
    static const uint8_t boot[] = "JP13_UPLINK_ECHO_READY\r\n";
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)boot, sizeof(boot)-1U, 100U);
    for (;;) {
        while (rx_tail != rx_head) {
            uint8_t ch = rx_queue[rx_tail++];
            char line[32];
            int n = snprintf(line, sizeof(line), "RX byte=0x%02X '%c'\r\n",
                             (unsigned)ch, (ch >= 32U && ch < 127U) ? ch : '.');
            if (n > 0 && (unsigned)n < sizeof(line))
                (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 100U);
        }
        HAL_Delay(1U);
    }
}
