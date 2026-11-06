#include "common.h"
#include "system_clock.h"

int main(void)
{
    /* HAL_Init() 启用 HAL 时间基准（SysTick），并复位外设状态。 */
    HAL_Init();
    /* 芯片复位后使用 8 MHz HSI，这里切换到 72 MHz HSE+PLL 时钟。 */
    SystemClock_Config();

    /*
     * 这是一个刻意保持为空的应用。
     * 学习新外设时，可以每次只增加一个 MX_* 初始化函数和一个小型 C 辅助函数。
     */
    while (1) {
        /* HAL_Delay() 可以验证 SysTick_Handler 是否正确安装。 */
        HAL_Delay(1000U);
    }
}
