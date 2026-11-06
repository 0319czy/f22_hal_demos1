#ifndef F22_HAL_DEMOS_DAC_H
#define F22_HAL_DEMOS_DAC_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

extern DAC_HandleTypeDef hdac1;

/* 初始化 DAC1 的 PA4(DAC_OUT1) / PA5(DAC_OUT2) 两个通道。 */
void MX_DAC1_Init(void);

/*
 * 写 DAC 的输出码。channel_index 取 0(DAC_OUT1/PA4) 或 1(DAC_OUT2/PA5)，
 * code 取 0~4095，对应约 0~3.3 V（手册标称 0~3.6 V）。
 * 越界或 HAL 失败返回 HAL_ERROR。
 */
HAL_StatusTypeDef Board_DacSet(uint8_t channel_index, uint16_t code);

#endif /* F22_HAL_DEMOS_DAC_H */
