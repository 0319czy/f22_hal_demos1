#include "common.h"
#include "gpio.h"
#include "led.h"
#include "system_clock.h"
#include "usart.h"

/*
 * 验板最小例程：确认时钟、LED 和调试串口三个基本资源是活的。
 * 两个 LED 同闪，同时每秒从 USART1 输出一行。
 * USART1 就是板载 CH340 背后的串口，用 USB 线就能直接看到这两行输出；
 * 既看灯也能确认 72 MHz 时钟和串口都正常。
 */
int main(void)
{
    uint8_t second = 0U;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    /* 初始化失败会进入 Error_Handler()，此时既没有串口输出也没有 LED 变化。 */
    MX_USART1_UART_Init();

    Board_Log("F22 demo01 start, HSE 8MHz -> SYSCLK 72MHz\r\n");

    while (1) {
        /* 每秒切换一次亮灭状态。 */
        HAL_GPIO_TogglePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin);
        HAL_GPIO_TogglePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin);

        Board_Log("tick ");
        Board_LogHexByte(second);
        Board_Log("\r\n");
        second = (uint8_t)(second + 1U);

        HAL_Delay(1000U);
    }
}
