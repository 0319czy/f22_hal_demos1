#include "common.h"
#include "gpio.h"
#include "key.h"
#include "led.h"
#include "system_clock.h"

/*
 * 验板用：KEY1 点亮/熄灭 LED1，KEY2 点亮/熄灭 LED2。
 * 上电后 LED 全灭，静止不动说明按键和 LED 都正常；
 * 按下没反应说明对应那一路有问题。
 */
int main(void)
{
    uint8_t key1_last = 1U; /* 初值取"未按下"，上电即可点亮 LED1 */
    uint8_t key2_last = 1U;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    Leds_Off();

    while (1) {
        uint8_t key1 = Key1_IsPressed();
        uint8_t key2 = Key2_IsPressed();

        /* 只在"新的按下沿"动作一次，持续按住不会反复切换。 */
        if (key1 && !key1_last) {
            Led1_Toggle();
        }
        if (key2 && !key2_last) {
            Led2_Toggle();
        }

        key1_last = key1;
        key2_last = key2;

        /* 20 ms 采样一次，足够快也不会因为抖动而连发。 */
        HAL_Delay(20U);
    }
}
