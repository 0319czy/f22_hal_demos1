#include "board_f22.h"
#include "led.h"

/*
 * 板上 Q1/Q2 是 S8550 NPN 反相级：MCU 输出低电平时三极管截止，
 * JP3/JP4 的引脚 1 被抬到 3.3V，外接 LED（正极接引脚 1）点亮。
 * 有效电平宏定义在 board_f22.h，这里不重复电气细节。
 */
void Led1_On(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_ON_LEVEL);
}

void Led1_Off(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_OFF_LEVEL);
}

void Led1_Toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin);
}

void Led2_On(void)
{
    HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port,
                      BOARD_LED2_Pin,
                      BOARD_LED2_ON_LEVEL);
}

void Led2_Off(void)
{
    HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port,
                      BOARD_LED2_Pin,
                      BOARD_LED2_OFF_LEVEL);
}

void Led2_Toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin);
}

void Leds_Off(void)
{
    Led1_Off();
    Led2_Off();
}
