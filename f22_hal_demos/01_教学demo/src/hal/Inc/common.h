#ifndef F22_HAL_DEMOS_COMMON_H
#define F22_HAL_DEMOS_COMMON_H

#include "stm32f1xx_hal.h"

/* HAL 初始化或外设操作失败时进入的统一错误停机函数。 */
void Error_Handler(void);

/* 打印一串以 '\0' 结尾的文本，阻塞发送到调试串口；未初始化串口时静默丢弃。 */
void Board_Log(const char *text);

/* 只把单个字节追加成两位十六进制文本，便于打印地址和寄存器值。 */
void Board_LogHexByte(uint8_t value);

/*
 * 以十进制打印一个 32 位无符号数，不带换行。
 * 自检例程要输出电压、原始码、计数这类数值，比反复转成字符串方便。
 */
void Board_LogU32(uint32_t value);

/*
 * 打印并清除 MCU 的复位原因（RCC->CSR）。
 * 上电时如果出现意外复位，用它能直接区分是上电复位、按键复位、
 * 掉电欠压复位、看门狗复位还是软件复位 —— 比在 main 里逐行加打印快得多。
 * 返回 1 表示至少有一个复位源被记录。
 */
uint8_t Board_PrintResetCause(void);

#endif /* F22_HAL_DEMOS_COMMON_H */
