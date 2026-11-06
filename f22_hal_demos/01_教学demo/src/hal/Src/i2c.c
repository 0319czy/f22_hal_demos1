#include "board_f22.h"
#include "common.h"
#include "i2c.h"

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

static void I2c_Config100k(I2C_HandleTypeDef *i2c)
{
    /*
     * 板上多条从机共用同一条总线，初始化函数不绑定任何器件地址。
     * 100 kHz 是保守选择：SPL06 和 MPU6050 都能稳定工作，也便于用逻辑分析仪观察。
     */
    i2c->Init.ClockSpeed = 100000U;
    i2c->Init.DutyCycle = I2C_DUTYCYCLE_2;
    i2c->Init.OwnAddress1 = 0U;
    i2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c->Init.OwnAddress2 = 0U;
    i2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
}

/* 初始化 JP6 的 I2C1（PB6=SCL / PB7=SDA）。 */
void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    I2c_Config100k(&hi2c1);
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

/* 初始化 JP7 的 I2C2（PB10=SCL / PB11=SDA）。 */
void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    I2c_Config100k(&hi2c2);
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c)
{
    GPIO_InitTypeDef gpio = {0};

    if (i2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /*
         * 重要冲突提示：I2C1 的 PB6/PB7 就是 M6/M7 的 T4_CH1/T4_CH2。
         * 同一个例程里不能同时初始化 I2C1 和 TIM4，否则两者会争抢这两个引脚。
         */
        gpio.Pin = BOARD_I2C1_SCL_PIN;
        gpio.Mode = GPIO_MODE_AF_OD;
        /* I2C 是开漏结构，高电平完全依赖板上的外部上拉电阻。 */
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BOARD_I2C1_SCL_PORT, &gpio);

        gpio.Pin = BOARD_I2C1_SDA_PIN;
        HAL_GPIO_Init(BOARD_I2C1_SDA_PORT, &gpio);
    } else if (i2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio.Pin = BOARD_I2C2_SCL_PIN;
        gpio.Mode = GPIO_MODE_AF_OD;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BOARD_I2C2_SCL_PORT, &gpio);

        gpio.Pin = BOARD_I2C2_SDA_PIN;
        HAL_GPIO_Init(BOARD_I2C2_SDA_PORT, &gpio);
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2c)
{
    if (i2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(BOARD_I2C1_SCL_PORT, BOARD_I2C1_SCL_PIN);
        HAL_GPIO_DeInit(BOARD_I2C1_SDA_PORT, BOARD_I2C1_SDA_PIN);
    } else if (i2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_DISABLE();
        HAL_GPIO_DeInit(BOARD_I2C2_SCL_PORT, BOARD_I2C2_SCL_PIN);
        HAL_GPIO_DeInit(BOARD_I2C2_SDA_PORT, BOARD_I2C2_SDA_PIN);
    }
}
