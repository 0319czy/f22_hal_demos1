#include "common.h"
#include "board_f22.h"
#include "i2c.h"

/* A DAP reset can stop the MCU halfway through a transfer while a powered
 * sensor keeps SDA low. Recover only at boot, before enabling PWM or I2C2. */
unsigned I2C2_ClearBusAtBoot(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();
    __HAL_RCC_I2C2_FORCE_RESET();
    __HAL_RCC_I2C2_RELEASE_RESET();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_Delay(1U);

    unsigned clocks = 0U;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_SET &&
        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET) {
        for (; clocks < 9U; ++clocks) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
            HAL_Delay(1U);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_Delay(1U);
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) != GPIO_PIN_SET) break;
        }
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_SET) {
            /* STOP: SDA rises while SCL is released high. */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
            HAL_Delay(1U);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
            HAL_Delay(1U);
        }
    }

    unsigned failed = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) != GPIO_PIN_SET ||
                      HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) != GPIO_PIN_SET;
    /* HAL_I2C_MspInit() changes both pins to alternate-function open-drain. */
    return clocks | (failed ? 0x100U : 0U);
}
