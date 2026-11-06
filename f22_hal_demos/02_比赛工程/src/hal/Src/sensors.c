#include "sensors.h"

#include "i2c.h"

/*
 * ============================================================================
 * 板载传感器驱动：MPU6050 + SPL06-001
 * ============================================================================
 *
 * 两只都在 I2C2（PB10=SCL / PB11=SDA）上，见手册 2.11 与 3.7。
 * 地址在 board_f22.h 里与其他板载器件放在一起。
 *
 * 保留原始码接口供自检复用，SPL06 另提供出厂系数补偿后的 Pa/°C。
 */

/* I2C 超时：100 kHz 下 20 字节连读约 2 ms，20 ms 已经很宽松。 */
#define SENSORS_I2C_TIMEOUT_MS 20U

static HAL_StatusTypeDef Reg_Read(uint8_t address7, uint8_t reg, uint8_t *buf,
                                  uint16_t len) {
  /* HAL 的 DevAddress 是 8 位形式，7 位地址要左移一位。 */
  return HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(address7 << 1U), reg,
                          I2C_MEMADD_SIZE_8BIT, buf, len,
                          SENSORS_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef Reg_Write(uint8_t address7, uint8_t reg,
                                   const uint8_t *buf, uint16_t len) {
  return HAL_I2C_Mem_Write(&hi2c2, (uint16_t)(address7 << 1U), reg,
                           I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len,
                           SENSORS_I2C_TIMEOUT_MS);
}

/* ------------------------------------------------------------------------- */
/* MPU6050                                                                     */
/* ------------------------------------------------------------------------- */

#define MPU_REG_SMPLRT_DIV 0x19U
#define MPU_REG_CONFIG 0x1AU
#define MPU_REG_GYRO_CONFIG 0x1BU
#define MPU_REG_ACCEL_CONFIG 0x1CU
#define MPU_REG_ACCEL_XOUT_H 0x3BU
#define MPU_REG_PWR_MGMT_1 0x6BU
#define MPU_REG_WHO_AM_I 0x75U

static int16_t Be16(const uint8_t *p) {
  return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

HAL_StatusTypeDef Sensors_ImuReadId(uint8_t *id) {
  if (id == NULL) {
    return HAL_ERROR;
  }
  return Reg_Read(BOARD_I2C_ADDR_MPU6050, MPU_REG_WHO_AM_I, id, 1U);
}

HAL_StatusTypeDef Sensors_ImuInit(void) {
  HAL_StatusTypeDef rc;

  /*
   * PWR_MGMT_1=0x01：唤醒，选用陀螺仪 X 轴 PLL 作为时钟源。
   *  不唤醒的话后面所有寄存器读出来都是 0。
   */
  if (Reg_Write(BOARD_I2C_ADDR_MPU6050, MPU_REG_PWR_MGMT_1,
                (const uint8_t[]){0x01U}, 1U) != HAL_OK) {
    return HAL_ERROR;
  }
  HAL_Delay(50U); /*  datasheet 要求唤醒后等 50 ms */

  /* DLPF 开启时基础速率 1 kHz，SMPLRT_DIV=4 → 输出 1000/(4+1)=200 Hz。 */
  rc = Reg_Write(BOARD_I2C_ADDR_MPU6050, MPU_REG_SMPLRT_DIV,
                 (const uint8_t[]){0x04U}, 1U);
  /* CONFIG=0x03 → DLPF 44 Hz，滤掉电机和高频噪声。 */
  if (rc == HAL_OK) {
    rc = Reg_Write(BOARD_I2C_ADDR_MPU6050, MPU_REG_CONFIG,
                   (const uint8_t[]){0x03U}, 1U);
  }
  /* GYRO_CONFIG=0x00 → ±250 °/s，131 LSB/(°/s)。 */
  if (rc == HAL_OK) {
    rc = Reg_Write(BOARD_I2C_ADDR_MPU6050, MPU_REG_GYRO_CONFIG,
                   (const uint8_t[]){0x00U}, 1U);
  }
  /* ACCEL_CONFIG=0x00 → ±2 g，16384 LSB/g。 */
  if (rc == HAL_OK) {
    rc = Reg_Write(BOARD_I2C_ADDR_MPU6050, MPU_REG_ACCEL_CONFIG,
                   (const uint8_t[]){0x00U}, 1U);
  }

  /* 丢一次，丢的是四组配置寄存器之一，不能继续。 */
  if (rc != HAL_OK) {
    return HAL_ERROR;
  }
  HAL_Delay(10U);
  return HAL_OK;
}

HAL_StatusTypeDef Sensors_ImuRead(ImuSample *out) {
  uint8_t raw[14];
  HAL_StatusTypeDef rc;

  if (out == NULL) {
    return HAL_ERROR;
  }

  /* ACCEL_XOUT_H 起 14 字节是连续的：accel(6) + temp(2) + gyro(6)。 */
  rc = Reg_Read(BOARD_I2C_ADDR_MPU6050, MPU_REG_ACCEL_XOUT_H, raw, 14U);
  if (rc != HAL_OK) {
    return rc;
  }

  out->accel[0] = Be16(&raw[0]);
  out->accel[1] = Be16(&raw[2]);
  out->accel[2] = Be16(&raw[4]);
  out->temp_raw = Be16(&raw[6]);
  out->gyro[0] = Be16(&raw[8]);
  out->gyro[1] = Be16(&raw[10]);
  out->gyro[2] = Be16(&raw[12]);
  return HAL_OK;
}

/* ------------------------------------------------------------------------- */
/* SPL06-001                                                                   */
/* ------------------------------------------------------------------------- */

/*
 * 寄存器表照抄官方例程
 *   3_官方例程/UAV-F22之应用例程/UAV-F22之应用__SPL06-001/USER/src/SPL06_001.c
 * 之前按 BME280 的习惯去读 0x09 当 ID、0xF7 当数据，全部读成 0，
 * 一度误判成芯片坏了。SPL06-001 的表跟 BME280 只是地址撞了，实际排布不同。
 */
#define SPL_REG_PRESSURE 0x00U /* 压力原始值，3 字节，24 位有符号 */
#define SPL_REG_TEMP 0x03U     /* 温度原始值，3 字节，24 位有符号 */
#define SPL_REG_PRS_CFG 0x06U  /* 压力过采样率 / 后台输出速率 */
#define SPL_REG_TMP_CFG 0x07U  /* 温度过采样率 / 后台输出速率 / 温源选择 */
#define SPL_REG_MEAS_CFG 0x08U /* 测量模式 + 状态位 */
#define SPL_REG_CFG 0x09U      /* 中断 / FIFO / 数据覆盖 */
#define SPL_REG_RESET 0x0CU    /* 0x09=软复位，0x80=FIFO 冲洗 */
#define SPL_REG_ID 0x0DU       /* 芯片 ID，(id & 0xF0) 应为 0x10 */
#define SPL_REG_COEF 0x10U     /* 出厂标定系数，0x10~0x21 */

/* MEAS_CFG(0x08) 状态位。 */
#define SPL_STS_COEF_RDY 0x80U    /* 内部标定值可读 */
#define SPL_STS_SENSOR_RDY 0x40U  /* 内部初始化完成 */
#define SPL_STS_TMP_RDY 0x20U     /* 温度值就绪（读后自动清零） */
#define SPL_STS_PRS_RDY 0x10U     /* 气压值就绪 */

/* MEAS_CFG(0x08) 低 2 位模式。 */
#define SPL_MODE_STANDBY 0x00U
#define SPL_MODE_CMD_PRS 0x01U  /* 单次采压力 */
#define SPL_MODE_CMD_TMP 0x02U  /* 单次采温度 */
#define SPL_MODE_BG_PSR_TMP 0x07U /* 后台连续采压力+温度 */

/* CFG(0x09) 数据覆盖位：过采样 >8 次时必须置位，否则数据会被丢掉。 */
#define SPL_CFG_P_SHIFT 0x04U

/*
 * 官方初始化用的参数：压力 32 次过采样 / 128 Hz 输出，
 * 温度 8 次过采样 / 4 Hz 输出，测 MEMS 芯片自带的温度计。
 */
#define SPL_PRS_OSS 5U /* 32 次 */
#define SPL_PRS_RATE 7U /* 128 Hz */
#define SPL_TMP_OSS 3U  /* 8 次 */
#define SPL_TMP_RATE 2U /* 4 Hz */
#define SPL_TMP_EXT 1U  /* 用气压芯片上的温度计，而不是 F22 板载的 */

#define SPL_ID_MASK 0xF0U
#define SPL_ID_VALUE 0x10U

/* 官方例程的缩放常量，与上面的过采样配置配套。 */
#define SPL_PRESSURE_SCALE 516096.0f /* 32 次过采样 */
#define SPL_TEMPERATURE_SCALE 7864320.0f /* 8 次过采样 */

typedef struct {
  int32_t c0, c1, c00, c10, c01, c11, c20, c21, c30;
} SplCalibration;

static SplCalibration g_baro_calibration;
static uint8_t g_baro_calibration_valid;

/* 有符号 12/20/16 位扩展，避免对负值做移位或依赖无符号转有符号。 */
static int32_t SignExtend(uint32_t value, uint8_t bits) {
  uint32_t sign = 1UL << (bits - 1U);
  return (value & sign) != 0U
             ? (int32_t)value - (int32_t)(1UL << bits)
             : (int32_t)value;
}

static SplCalibration DecodeCalibration(const uint8_t *b) {
  SplCalibration c;
  c.c0 = SignExtend(((uint32_t)b[0] << 4U) | (b[1] >> 4U), 12U);
  c.c1 = SignExtend(((uint32_t)(b[1] & 0x0FU) << 8U) | b[2], 12U);
  c.c00 = SignExtend(((uint32_t)b[3] << 12U) |
                        ((uint32_t)b[4] << 4U) | (b[5] >> 4U), 20U);
  c.c10 = SignExtend(((uint32_t)(b[5] & 0x0FU) << 16U) |
                        ((uint32_t)b[6] << 8U) | b[7], 20U);
  c.c01 = SignExtend(((uint32_t)b[8] << 8U) | b[9], 16U);
  c.c11 = SignExtend(((uint32_t)b[10] << 8U) | b[11], 16U);
  c.c20 = SignExtend(((uint32_t)b[12] << 8U) | b[13], 16U);
  c.c21 = SignExtend(((uint32_t)b[14] << 8U) | b[15], 16U);
  c.c30 = SignExtend(((uint32_t)b[16] << 8U) | b[17], 16U);
  return c;
}

/* 24 位有符号，官方是按三字节拼 int32 后手工补符号位。 */
static int32_t Be24(const uint8_t *p) {
  int32_t v = ((int32_t)p[0] << 16) | ((int32_t)p[1] << 8) | (int32_t)p[2];
  return (v & 0x00800000) ? (v | (int32_t)0xFF000000) : v;
}

HAL_StatusTypeDef Sensors_BaroReadId(uint8_t *id) {
  uint8_t raw;

  if (id == NULL) {
    return HAL_ERROR;
  }
  if (Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_ID, &raw, 1U) != HAL_OK) {
    return HAL_ERROR;
  }
  *id = raw;
  return HAL_OK;
}

HAL_StatusTypeDef Sensors_BaroReadStatus(uint8_t *status) {
  if (status == NULL) {
    return HAL_ERROR;
  }
  return Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_MEAS_CFG, status, 1U);
}

HAL_StatusTypeDef Sensors_BaroReadRegs(uint8_t reg, uint8_t *buf,
                                       uint16_t len) {
  if (buf == NULL) {
    return HAL_ERROR;
  }
  return Reg_Read(BOARD_I2C_ADDR_SPL06, reg, buf, len);
}

HAL_StatusTypeDef Sensors_BaroInit(void) {
  uint8_t cfg;
  uint8_t waits;
  uint8_t coefficients[18];
  SplCalibration calibration;
  HAL_StatusTypeDef rc;

  /* 重新初始化失败时，禁止继续使用上一次成功的标定系数。 */
  g_baro_calibration_valid = 0U;

  /*
   * 1) 等出厂标定值可读。官方是无条件死等，这里给 50 ms 上限，
   *    免得传感器坏掉时把自检卡死在这一步。
   */
  for (waits = 0U; waits < 50U; ++waits) {
    if (Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_MEAS_CFG, &cfg, 1U) != HAL_OK) {
      return HAL_ERROR;
    }
    if ((cfg & SPL_STS_COEF_RDY) != 0U) {
      break;
    }
    HAL_Delay(1U);
  }
  if (waits >= 50U) {
    return HAL_TIMEOUT;
  }

  if (Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_COEF, coefficients,
               sizeof(coefficients)) != HAL_OK) {
    return HAL_ERROR;
  }
  calibration = DecodeCalibration(coefficients);

  /*
   * 2) 等内部初始化完成。
   */
  for (waits = 0U; waits < 50U; ++waits) {
    if (Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_MEAS_CFG, &cfg, 1U) != HAL_OK) {
      return HAL_ERROR;
    }
    if ((cfg & SPL_STS_SENSOR_RDY) != 0U) {
      break;
    }
    HAL_Delay(1U);
  }
  if (waits >= 50U) {
    return HAL_TIMEOUT;
  }

  /* 3) 过采样 32 次 > 8 次，必须允许新数据覆盖旧数据。 */
  if (Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_CFG, &cfg, 1U) != HAL_OK) {
    return HAL_ERROR;
  }
  cfg = (uint8_t)(cfg | SPL_CFG_P_SHIFT);
  if (Reg_Write(BOARD_I2C_ADDR_SPL06, SPL_REG_CFG, &cfg, 1U) != HAL_OK) {
    return HAL_ERROR;
  }

  /* 4) 压力：后台速率 <<4 | 过采样。 */
  cfg = (uint8_t)((SPL_PRS_RATE << 4) | SPL_PRS_OSS);
  if (Reg_Write(BOARD_I2C_ADDR_SPL06, SPL_REG_PRS_CFG, &cfg, 1U) != HAL_OK) {
    return HAL_ERROR;
  }

  /* 5) 温度：温源 <<7 | 后台速率 <<4 | 过采样。 */
  cfg = (uint8_t)((SPL_TMP_EXT << 7) | (SPL_TMP_RATE << 4) | SPL_TMP_OSS);
  if (Reg_Write(BOARD_I2C_ADDR_SPL06, SPL_REG_TMP_CFG, &cfg, 1U) != HAL_OK) {
    return HAL_ERROR;
  }

  /* 6) 进后台模式，连续采压力+温度。 */
  cfg = SPL_MODE_BG_PSR_TMP;
  rc = Reg_Write(BOARD_I2C_ADDR_SPL06, SPL_REG_MEAS_CFG, &cfg, 1U);
  if (rc == HAL_OK) {
    g_baro_calibration = calibration;
    g_baro_calibration_valid = 1U;
  }
  return rc;
}

HAL_StatusTypeDef Sensors_BaroRead(BaroSample *out) {
  uint8_t raw[6];
  HAL_StatusTypeDef rc;

  if (out == NULL) {
    return HAL_ERROR;
  }

  /* 0x00~0x02 压力，0x03~0x05 温度，连续 6 字节一次读完。 */
  rc = Reg_Read(BOARD_I2C_ADDR_SPL06, SPL_REG_PRESSURE, raw, 6U);
  if (rc != HAL_OK) {
    return rc;
  }

  out->pressure_raw = (uint32_t)Be24(&raw[0]);
  out->temp_raw = (uint32_t)Be24(&raw[3]);
  return HAL_OK;
}

HAL_StatusTypeDef Sensors_BaroReadCompensated(BaroCompensatedSample *out) {
  BaroSample raw;
  BaroCompensatedSample sample;
  float p, t;
  HAL_StatusTypeDef rc;
  const SplCalibration *c = &g_baro_calibration;

  if (out == NULL || g_baro_calibration_valid == 0U) {
    return HAL_ERROR;
  }
  rc = Sensors_BaroRead(&raw);
  if (rc != HAL_OK) {
    return rc;
  }

  p = (float)raw.pressure_raw / SPL_PRESSURE_SCALE;
  t = (float)raw.temp_raw / SPL_TEMPERATURE_SCALE;
  sample.temperature_c = (float)c->c0 * 0.5f + (float)c->c1 * t;
  sample.pressure_pa = (float)c->c00 +
      p * ((float)c->c10 + p * ((float)c->c20 + p * (float)c->c30)) +
      t * (float)c->c01 +
      t * p * ((float)c->c11 + p * (float)c->c21);
  *out = sample;
  return HAL_OK;
}
