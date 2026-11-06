#ifndef F22_HAL_DEMOS_USART_H
#define F22_HAL_DEMOS_USART_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

/*
 * 三个串口都可以在 PlatformIO build_flags 里覆盖默认波特率，
 * 例如 -DUSART1_DEMO_BAUDRATE=9600U 去对接 AT89S52 遥控器。
 */
#ifndef USART1_DEMO_BAUDRATE
#define USART1_DEMO_BAUDRATE 115200U
#endif

#ifndef USART2_DEMO_BAUDRATE
#define USART2_DEMO_BAUDRATE 115200U
#endif

#ifndef USART4_DEMO_BAUDRATE
#define USART4_DEMO_BAUDRATE 115200U
#endif

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;

/* 初始化普通 8N1 UART；具体例程显式决定是否调用。 */
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART4_UART_Init(void);

#endif /* F22_HAL_DEMOS_USART_H */
