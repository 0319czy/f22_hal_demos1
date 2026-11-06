#include "board_f22.h"
#include "led.h"

/*
 * UAV-F22 的 LED1(PA0)/LED2(PA1) 前级是 S8550 NPN 反相，
 * 所以"点亮"对应 MCU 输出低电平，即 GPIO_PIN_RESET。
 * 有效电平统一放在 board_f22.h，这里不再重复电气细节。
 */
void Led1_On(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_ON_LEVEL);
}

void Led2_On(void)
{
    HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port,
                      BOARD_LED2_Pin,
                      BOARD_LED2_ON_LEVEL);
}

void Leds_Off(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_OFF_LEVEL);
    HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port,
                      BOARD_LED2_Pin,
                      BOARD_LED2_OFF_LEVEL);
}

/* 两灯同闪：只有一个灯在闪，说明另一路 LED 或引脚有问题。 */
void Leds_BlinkBoth(void)
{
    while (1) {
        Led1_On();
        Led2_On();
        HAL_Delay(500U);
        Leds_Off();
        HAL_Delay(500U);
    }
}

/* 两灯交替：两个灯都在闪但节奏相反，说明两个通道都正常。 */
void Leds_BlinkAlternate(void)
{
    while (1) {
        Led1_On();
        HAL_Delay(300U);
        Leds_Off();
        Led2_On();
        HAL_Delay(300U);
        Leds_Off();
        HAL_Delay(300U);
    }
}
