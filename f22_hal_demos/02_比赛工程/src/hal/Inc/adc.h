#ifndef F22_HAL_DEMOS_ADC_H
#define F22_HAL_DEMOS_ADC_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

/* 初始化 ADC1 的规则组（8 个通道）和 DMA1 通道 1。 */
void MX_ADC1_Init(void);

/*
 * 读取 JP8 上第 port_index 路（0~7，对应 ADC1~ADC8）的原始码。
 * 每次调用单独配一个通道做单次转换，结果最直观、不受 DMA 时序影响，
 * 适合自检例程这种"读一两次"的用法。port_index 越界返回 0。
 */
uint16_t Board_AdcReadCode(uint8_t port_index);

/*
 * 读 ADC 内部基准电压通道（VREFINT）的原始码，16 次采样取平均。
 * 返回的是原始码而不是电压，因为 F103 没有 VREFINT 工厂校准值，
 * 换算出来的 VDDA 误差可达百分之几，只有原始码是可复现的判据。
 * 板上实测稳定在 1800~1900 区间（手册 2.2 节暗示 VDDA 约 3.6 V）。
 */
uint16_t Board_AdcReadVrefRaw(void);

/* 取上一次 Board_AdcReadVrefRaw() 采到的原始码，不重新采样。 */
uint16_t Board_AdcLastVrefRaw(void);

/*
 * 由 VREFINT 原始码反推 VDDA（单位 mV）。
 * 仅供观察量级，不作为判板依据：手册 2.2 节写 DAC 输出 0~3.6 V，
 * 而按 VREFINT 标称 1.204 V 反推只有约 2659 mV，两者对不上，
 * 怀疑该片 VREFINT 标称值与 STM32 不同。要真实电压请用万用表。
 * 返回 0 表示原始码异常。
 */
uint32_t Board_AdcReadVrefMv(void);

/*
 * 把 12 位原始码换算成 JP8 端口上的实际电压（单位 mV）。
 * 板上有二分压：手册 2.1 节写明每路可输入 0~7.2 V，
 * 即端口电压 = raw / 4095 * VDDA * 2。
 */
uint32_t Board_AdcCodeToPortMv(uint16_t code);

#endif /* F22_HAL_DEMOS_ADC_H */
