#include "board_f22.h"
#include "common.h"
#include "dac.h"

DAC_HandleTypeDef hdac1;

/* 配置 DAC1 两个通道，使用软件写值而不是定时器触发。 */
void MX_DAC1_Init(void)
{
    DAC_ChannelConfTypeDef channel = {0};

    hdac1.Instance = DAC;
    if (HAL_DAC_Init(&hdac1) != HAL_OK) {
        Error_Handler();
    }

    /* 无触发模式下，每次 HAL_DAC_SetValue 都会直接更新输出寄存器。 */
    channel.DAC_Trigger = DAC_TRIGGER_NONE;
    /*
     * 开启输出缓冲后能带一定负载；如果后面要驱动低阻负载出现压降，
     * 可以改成 DAC_OUTPUTBUFFER_DISABLE 换成更直接的输出。
     */
    channel.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

    if (HAL_DAC_ConfigChannel(&hdac1, &channel, DAC_CHANNEL_1) != HAL_OK ||
        HAL_DAC_ConfigChannel(&hdac1, &channel, DAC_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_DAC_MspInit(DAC_HandleTypeDef *dac)
{
    GPIO_InitTypeDef gpio = {0};

    if (dac->Instance != DAC) {
        return;
    }

    /* DAC 输出脚必须配置成模拟模式，否则数字输入缓冲会影响输出。 */
    __HAL_RCC_DAC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = BOARD_DAC1_Pin | BOARD_DAC2_Pin;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef *dac)
{
    if (dac->Instance == DAC) {
        __HAL_RCC_DAC_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, BOARD_DAC1_Pin | BOARD_DAC2_Pin);
    }
}

HAL_StatusTypeDef Board_DacSet(uint8_t channel_index, uint16_t code)
{
    uint32_t channel;

    /*
     * 没调 MX_DAC1_Init() 时 hdac1.Instance 还是 0。HAL_DAC_SetValue() 会把
     * Instance 当基址再加上对齐偏移，然后往那个地址直接写：
     * 基址为 0 时算出的地址是 0x00000008，往低地址写会触发非精确总线错误，
     * 现象是"HARD FAULT，PC/LR 是 0x800 这种垃圾值"。必须挡住。
     */
    if (hdac1.Instance == (DAC_TypeDef *)0) {
        return HAL_ERROR;
    }

    if (code > 4095U) {
        return HAL_ERROR;
    }

    if (channel_index == 0U) {
        channel = DAC_CHANNEL_1;
    } else if (channel_index == 1U) {
        channel = DAC_CHANNEL_2;
    } else {
        return HAL_ERROR;
    }

    /* F1 的 DAC 右对齐输出，DHR12R 寄存器低 12 位有效。 */
    return HAL_DAC_SetValue(&hdac1, channel, DAC_ALIGN_12B_R, code);
}
