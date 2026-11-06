#include "board_f22.h"
#include "common.h"
#include "system_clock.h"

/* UAV-F22 使用 8 MHz 外部晶振（原理图 X1），统一配置为 72 MHz 系统时钟。 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clock = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    /* 8 MHz × 9 = 72 MHz；APB1 再分频到 36 MHz，符合 F1 的 PCLK1 上限。 */
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clock.ClockType = RCC_CLOCKTYPE_HCLK |
                      RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_PCLK1 |
                      RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV2;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }

    /*
     * 释放 PB3(JTDO) 和 PB4(NJTRST) 的默认复用，同时保留 SWD 调试能力。
     * J3 提供的 PB3/PB4 通用 IO 依赖这一步；TIM3 的完全重映射也在本函数之后配置。
     */
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}
