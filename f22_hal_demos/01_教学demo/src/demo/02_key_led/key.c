#include "board_f22.h"
#include "key.h"

/*
 * 原理图上 S2/S3 的一端直接接 GND，且没有外部上拉电阻，
 * 所以 MX_GPIO_Init() 打开了内部上拉，按下时读到低电平。
 * 这层电气细节封装在本文件里，main.c 只关心"按下"还是"没按下"。
 */
uint8_t Key1_IsPressed(void)
{
    return HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port,
                            BOARD_KEY1_Pin) == GPIO_PIN_RESET;
}

uint8_t Key2_IsPressed(void)
{
    return HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port,
                            BOARD_KEY2_Pin) == GPIO_PIN_RESET;
}
