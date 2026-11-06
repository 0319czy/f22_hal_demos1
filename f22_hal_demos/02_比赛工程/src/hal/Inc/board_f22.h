#ifndef F22_HAL_DEMOS_BOARD_F22_H
#define F22_HAL_DEMOS_BOARD_F22_H

#include "stm32f1xx_hal.h"

/*
 * 本文件是 UAV-F22 的唯一引脚真值表。
 * 所有条目都来自《UAV-F22 无人机控制器用户手册 v2.0》第三章接口定义和
 * 《UAV-F22 v2.0》原理图；如果其他 hal/ 头文件里的宏与这里冲突，以这里为准。
 *
 * 端口命名沿用板卡丝印，而不是按功能重新命名，便于和手册、原理图对照：
 *   JP3/JP4  LED1/LED2 控制口      JP5  SPI2
 *   JP6      IIC1                   JP7  IIC2
 *   JP8      AI/AO                  JP9  5V 输出 / 备用电池
 *   JP10     3.3V 输出              JP11 SWD
 *   JP12     USART1                 JP13 USART2        JP14 USART4
 *   J1       T3_CH1~4 / T4_CH1~2    J2   PC12~PC15     J3   PB3~PB5
 */

/* ------------------------------------------------------------------------ */
/* 时钟                                                                       */
/* ------------------------------------------------------------------------ */
/* 原理图 X1 为 8 MHz 无源晶振，经 PLL×9 得到 72 MHz。 */
#define BOARD_HSE_FREQ_HZ 8000000U
#define BOARD_SYSCLK_HZ 72000000U

/* ------------------------------------------------------------------------ */
/* LED：JP3 -> PA0，JP4 -> PA1                                               */
/* ------------------------------------------------------------------------ */
/*
 * 板上有 Q1/Q2（S8550 NPN）反相级：MCU 引脚输出高电平时三极管导通，
 * JP3/JP4 的引脚 1 被拉到 GND；输出低电平时三极管截止，引脚 1 被上拉到 3.3 V。
 * 手册要求把外接 LED 正极接引脚 1、负极接引脚 2（GND），所以
 * "LED 点亮" 对应 MCU 引脚输出低电平 —— 与厂家例程 led.c 中
 * "GPIO_SetBits 表示关闭 LED" 的注释一致。
 *
 * 若手上这块板实测相反，只需把下面两个宏改成 GPIO_PIN_SET，
 * 不需要改动任何 demo 源码。
 */
#define BOARD_LED1_GPIO_Port GPIOA
#define BOARD_LED1_Pin GPIO_PIN_0
#define BOARD_LED2_GPIO_Port GPIOA
#define BOARD_LED2_Pin GPIO_PIN_1

#define BOARD_LED1_ON_LEVEL GPIO_PIN_RESET
#define BOARD_LED1_OFF_LEVEL GPIO_PIN_SET
#define BOARD_LED2_ON_LEVEL GPIO_PIN_RESET
#define BOARD_LED2_OFF_LEVEL GPIO_PIN_SET

/* ------------------------------------------------------------------------ */
/* 按键：PC12/PC13（J2 引脚 1/2）                                            */
/* ------------------------------------------------------------------------ */
/*
 * 原理图 S2/S3 的一端直接接 GND，另一端接 PC12/PC13，且没有外部上拉电阻，
 * 因此必须打开 MCU 内部上拉，按下为低电平。
 * PC13 同时是 RTC/后备域引脚，本板不接 32.768 kHz 晶振，直接当普通 GPIO 用。
 */
#define BOARD_KEY1_GPIO_Port GPIOC
#define BOARD_KEY1_Pin GPIO_PIN_12
#define BOARD_KEY2_GPIO_Port GPIOC
#define BOARD_KEY2_Pin GPIO_PIN_13

/* ------------------------------------------------------------------------ */
/* 串口                                                                       */
/* ------------------------------------------------------------------------ */
/* JP12 USART1：PA9=TXD1，PA10=RXD1。与板载 CH340 和 E49 433M 模组共用。 */
#define BOARD_USART1_TX_PORT GPIOA
#define BOARD_USART1_TX_PIN GPIO_PIN_9
#define BOARD_USART1_RX_PORT GPIOA
#define BOARD_USART1_RX_PIN GPIO_PIN_10

/* JP13 USART2：PA2=TXD2，PA3=RXD2。手册标注信号幅值为 5 V。 */
#define BOARD_USART2_TX_PORT GPIOA
#define BOARD_USART2_TX_PIN GPIO_PIN_2
#define BOARD_USART2_RX_PORT GPIOA
#define BOARD_USART2_RX_PIN GPIO_PIN_3

/* JP14 USART4：PC10=USART4_TX，PC11=USART4_RX。手册标注 3.3 V。 */
#define BOARD_USART4_TX_PORT GPIOC
#define BOARD_USART4_TX_PIN GPIO_PIN_10
#define BOARD_USART4_RX_PORT GPIOC
#define BOARD_USART4_RX_PIN GPIO_PIN_11

/* ------------------------------------------------------------------------ */
/* I2C                                                                        */
/* ------------------------------------------------------------------------ */
/* JP6 IIC1：PB6=SCL1，PB7=SDA1。注意 PB6/PB7 同时是 TIM4_CH1/CH2。 */
#define BOARD_I2C1_SCL_PORT GPIOB
#define BOARD_I2C1_SCL_PIN GPIO_PIN_6
#define BOARD_I2C1_SDA_PORT GPIOB
#define BOARD_I2C1_SDA_PIN GPIO_PIN_7

/* JP7 IIC2：PB10=SCL2，PB11=SDA2。板载传感器全部挂在这条总线上。 */
#define BOARD_I2C2_SCL_PORT GPIOB
#define BOARD_I2C2_SCL_PIN GPIO_PIN_10
#define BOARD_I2C2_SDA_PORT GPIOB
#define BOARD_I2C2_SDA_PIN GPIO_PIN_11

/* ------------------------------------------------------------------------ */
/* SPI2（JP5）                                                                */
/* ------------------------------------------------------------------------ */
/* PB12=NSS，PB13=SCK，PB14=MISO，PB15=MOSI。手册未标注复用冲突，默认不启用映射。 */
#define BOARD_SPI2_NSS_PORT GPIOB
#define BOARD_SPI2_NSS_PIN GPIO_PIN_12
#define BOARD_SPI2_SCK_PORT GPIOB
#define BOARD_SPI2_SCK_PIN GPIO_PIN_13
#define BOARD_SPI2_MISO_PORT GPIOB
#define BOARD_SPI2_MISO_PIN GPIO_PIN_14
#define BOARD_SPI2_MOSI_PORT GPIOB
#define BOARD_SPI2_MOSI_PIN GPIO_PIN_15

/* ------------------------------------------------------------------------ */
/* 电机 PWM（手册 4.1 / 4.2 节）                                              */
/* ------------------------------------------------------------------------ */
/* M1~M4 空心杯电机：T3_CH1~T3_CH4 = TIM3 完全重映射后的 PC6~PC9。 */
#define BOARD_M1_TIM_CHANNEL TIM_CHANNEL_1
#define BOARD_M2_TIM_CHANNEL TIM_CHANNEL_2
#define BOARD_M3_TIM_CHANNEL TIM_CHANNEL_3
#define BOARD_M4_TIM_CHANNEL TIM_CHANNEL_4
#define BOARD_M1_GPIO_Port GPIOC
#define BOARD_M1_Pin GPIO_PIN_6
#define BOARD_M2_GPIO_Port GPIOC
#define BOARD_M2_Pin GPIO_PIN_7
#define BOARD_M3_GPIO_Port GPIOC
#define BOARD_M3_Pin GPIO_PIN_8
#define BOARD_M4_GPIO_Port GPIOC
#define BOARD_M4_Pin GPIO_PIN_9

/* M6/M7 空心杯电机：T4_CH1/T4_CH2 = TIM4 的 PB6/PB7（手册只用前两路）。 */
#define BOARD_M6_TIM_CHANNEL TIM_CHANNEL_1
#define BOARD_M7_TIM_CHANNEL TIM_CHANNEL_2
#define BOARD_M6_GPIO_Port GPIOB
#define BOARD_M6_Pin GPIO_PIN_6
#define BOARD_M7_GPIO_Port GPIOB
#define BOARD_M7_Pin GPIO_PIN_7

/* M5 双向直流减速电机：只用 OUTA1=T1_CH1=PA8。
 * 反转脚不可用 PA11（USB_DM）；TIM1_CH4 无其他可用引脚，故 M5 仅支持单向。 */
#define BOARD_M5_TIM_CHANNEL TIM_CHANNEL_1
#define BOARD_M5_GPIO_Port GPIOA
#define BOARD_M5_Pin GPIO_PIN_8

/* ------------------------------------------------------------------------ */
/* 通用 IO：J2 的 PC14/PC15，J3 的 PB3/PB4/PB5                                  */
/* ------------------------------------------------------------------------ */
/* PB3/PB4 复位后是 JTAG 的 JTDO/NJTRST，必须先关 JTAG 才能当普通 GPIO 用。 */
#define BOARD_IO_PC14_PORT GPIOC
#define BOARD_IO_PC14_Pin GPIO_PIN_14
#define BOARD_IO_PC15_PORT GPIOC
#define BOARD_IO_PC15_Pin GPIO_PIN_15
#define BOARD_IO_PB3_PORT GPIOB
#define BOARD_IO_PB3_Pin GPIO_PIN_3
#define BOARD_IO_PB4_PORT GPIOB
#define BOARD_IO_PB4_Pin GPIO_PIN_4
#define BOARD_IO_PB5_PORT GPIOB
#define BOARD_IO_PB5_Pin GPIO_PIN_5

/* ------------------------------------------------------------------------ */
/* 模拟量：JP8                                                                */
/* ------------------------------------------------------------------------ */
/*
 * AI：PB0=ADC1_IN0，PB1=ADC1_IN1，PC0~PC5=ADC1_IN10~IN15。
 * 手册说明每路可输入 0~7.2 V，且"采集到的信号值应乘以 2 才是端口上的实际值"，
 * 即板上有二分压：V_port ≈ raw / 4095 * 3.3 V * 2。
 */
#define BOARD_ADC_PORT_COUNT 8U
#define BOARD_ADC_FULL_SCALE_MV 7200U

extern const uint32_t board_adc_channel[BOARD_ADC_PORT_COUNT];
extern const uint8_t board_adc_pin_index[BOARD_ADC_PORT_COUNT];

/* AO：DAC_OUT1=PA4，DAC_OUT2=PA5，手册标称输出 0~3.6 V。 */
#define BOARD_DAC1_PORT GPIOA
#define BOARD_DAC1_Pin GPIO_PIN_4
#define BOARD_DAC2_PORT GPIOA
#define BOARD_DAC2_Pin GPIO_PIN_5

/* ------------------------------------------------------------------------ */
/* 板载 I2C 器件（原理图标注的是 8 位写地址，括号内为 7 位地址）                */
/* ------------------------------------------------------------------------ */
/* MPU6050   0xD0 -> 0x68      QMC5883L 见各自模块             SPL06-001 0x76 */
/*
 * ST24C02 的 7 位地址随 A0~A2 跳线在 0x50~0x57 之间变化。
 * 实测手上这块 F22 跳到了 0x52 —— 原理图上标的 0xA0 是按 0x50 画的，
 * 所以这里给的是一个地址区间，由例程扫出实际应答的那个，而不是写死 0x50。
 * 三块板之间的跳线可能不同，所以务必扫而不要硬编码。
 */
#define BOARD_I2C_ADDR_MPU6050 0x68U
#define BOARD_I2C_ADDR_QMC5883L 0x0DU
#define BOARD_I2C_ADDR_SPL06 0x76U
#define BOARD_I2C_ADDR_EEPROM24C02_BASE 0x50U
#define BOARD_I2C_ADDR_EEPROM24C02_TOP 0x57U
#define BOARD_I2C_ADDR_US22310S 0x39U

#endif /* F22_HAL_DEMOS_BOARD_F22_H */
