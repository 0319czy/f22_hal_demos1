#ifndef SENSORS_H
#define SENSORS_H

#include "board_f22.h"

/* ------------------------------------------------------------------------- */
/* MPU6050（板载，I2C2 @ 0x68）—— 手册 2.20 姿态传感功能                       */
/* ------------------------------------------------------------------------- */

/* WHO_AM_I 期望值。MPU6050 = 0x68，MPU6500/MPU9250 也会回 0x70/0x71/0x73。 */
#define MPU6050_ID_MPU6050 0x68U

typedef struct {
  int16_t accel[3];  /* 原始码，±2g 量程 = 16384 LSB/g */
  int16_t gyro[3];   /* 原始码，±250°/s 量程 = 131 LSB/(°/s) */
  int16_t temp_raw;  /* MPU6050 芯片温度：raw / 340.0f + 36.53f °C */
} ImuSample;

/* 返回 HAL_OK 表示能读到 14 字节连续数据。 */
HAL_StatusTypeDef Sensors_ImuRead(ImuSample *out);

/* 读 WHO_AM_I(0x75)。 */
HAL_StatusTypeDef Sensors_ImuReadId(uint8_t *id);

/* 唤醒并配置量程/DLPF。必须在 Sensors_ImuRead() 之前调用一次。 */
HAL_StatusTypeDef Sensors_ImuInit(void);

/* ------------------------------------------------------------------------- */
/* SPL06-001（板载，I2C2 @ 0x76）—— 手册 2.21 气压传感功能                    */
/* ------------------------------------------------------------------------- */

typedef struct {
  int32_t pressure_raw; /* 24 位有符号原始码（0x00~0x02） */
  int32_t temp_raw;     /* 24 位有符号原始码（0x03~0x05） */
} BaroSample;

typedef struct {
  float pressure_pa;
  float temperature_c;
} BaroCompensatedSample;

HAL_StatusTypeDef Sensors_BaroInit(void);
HAL_StatusTypeDef Sensors_BaroReadId(uint8_t *id);
HAL_StatusTypeDef Sensors_BaroReadStatus(uint8_t *status);
HAL_StatusTypeDef Sensors_BaroRead(BaroSample *out);

/* 使用初始化时读取的出厂系数补偿；失败时不改写 out。
 * 必须先成功调用 Sensors_BaroInit()，并等测量数据就绪。
 */
HAL_StatusTypeDef Sensors_BaroReadCompensated(BaroCompensatedSample *out);

/*
 * 调试用：从任意寄存器连续读 len 字节。
 * SPL06-001 与 BME280 的寄存器布局不同，请按 SPL06 手册或厂家例程查询。
 */
HAL_StatusTypeDef Sensors_BaroReadRegs(uint8_t reg, uint8_t *buf,
                                       uint16_t len);

#endif /* SENSORS_H */
