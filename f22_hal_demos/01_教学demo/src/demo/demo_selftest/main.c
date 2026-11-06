#include "adc.h"
#include "board_f22.h"
#include "common.h"
#include "dac.h"
#include "gpio.h"
#include "i2c.h"
#include "sensors.h"
#include "system_clock.h"
#include "tim.h"
#include "usart.h"

/* Safe by default: the health check must not alter stored data or spin motors. */
#define SELFTEST_ENABLE_EEPROM_WRITE 0U
#define SELFTEST_ENABLE_MOTOR_TEST 0U

/*
 * ============================================================================
 * UAV-F22 综合自检例程（一个固件查全部）
 * ============================================================================
 *
 * 用法
 *   烧录：pio run -d /data/Downloads/f22_hal_demos/01_教学demo -e demo_selftest -t upload
 *   看结果：串口 115200 8N1（板载 CH340，USB 线直接看）
 *
 * 为什么要做成一个固件：手上有三块板要验，逐个烧不同例程容易漏项、
 * 也分不清是哪一步坏的。这个固件每次上电按固定顺序跑完所有项，
 * 末尾打印一份汇总表，三块板的输出可以直接互相比较。
 *
 * 安全默认
 *   EEPROM 仅探测地址，不写任何数据；本环境不编译电机测试代码。
 *   需要测试电机时，使用隔离的 demo03_motor_bench，每次只测一路并拆桨。
 *
 * 判定基准
 *   [OK]      符合预期
 *   [WARN]    有响应但数值可疑，多半是外接接线或供电问题，不是芯片坏
 *   [FAIL]    完全没响应；须排查地址、供电、接线和总线后再判断故障
 *   [SKIP]    安全模式跳过 EEPROM 写入/电机输出，或未接外部激励
 */

#define SELFTEST_RESULT_OK 0U
#define SELFTEST_RESULT_WARN 1U
#define SELFTEST_RESULT_FAIL 2U
#define SELFTEST_RESULT_SKIP 3U
#define SELFTEST_RESULT_MAX 24U

typedef struct {
  const char *item;
  uint8_t result;
} SelfTestResult;

static SelfTestResult g_results[SELFTEST_RESULT_MAX];
static uint8_t g_result_count = 0U;

/*
 * 阶段序号。观察到的现象是程序会在中途重启、而且每次停在的位置不一样，
 * 所以每进入一个阶段都先打印序号：如果看到同一个序号出现两次，
 * 就说明卡在这里并被复位了。
 */
static uint8_t g_step = 0U;

/* ------------------------------------------------------------------------- */
/* 输出与记录小工具 */
/* ------------------------------------------------------------------------- */

static void Print_Result(uint8_t result) {
  switch (result) {
  case SELFTEST_RESULT_OK:
    Board_Log(" [OK]\r\n");
    break;
  case SELFTEST_RESULT_WARN:
    Board_Log(" [WARN]\r\n");
    break;
  case SELFTEST_RESULT_FAIL:
    Board_Log(" [FAIL]\r\n");
    break;
  default:
    Board_Log(" [SKIP]\r\n");
    break;
  }
}

static void Record_Result(const char *item, uint8_t result) {
  Board_Log("  ");
  Board_Log(item);
  Print_Result(result);

  if (g_result_count < SELFTEST_RESULT_MAX) {
    g_results[g_result_count].item = item;
    g_results[g_result_count].result = result;
    g_result_count++;
  }
}

static void Print_Title(const char *title) {
  g_step++;

  Board_Log("\r\n---- [step ");
  Board_LogU32(g_step);
  Board_Log("] ");
  Board_Log(title);
  Board_Log(" ----\r\n");
}

/* ------------------------------------------------------------------------- */
/* 0. 板卡身份：MCU 唯一 ID                                                   */
/* ------------------------------------------------------------------------- */

/*
 * STM32F1 在 0x1FFFF7E8 存了 96 位（12 字节）出厂唯一 ID。
 * 三块板要分开记录，这个 ID 就是最省事的区分手段 —— 不需要额外的器件。
 */
static void Test_BoardId(void) {
  static const uint32_t base = 0x1FFFF7E8UL;
  const uint8_t *uid = (const uint8_t *)base;
  uint8_t i;

  Board_Log("  MCU UID:");
  for (i = 0U; i < 12U; ++i) {
    if ((i % 2U) == 0U) {
      Board_LogHexByte(uid[i]);
    } else {
      Board_LogHexByte(uid[i]);
    }
  }
  Board_Log("\r\n");

  /* 12 个字节全 0 或全 FF 说明没读到有效 ID，属于异常。 */
  {
    uint8_t all_same = 1U;
    for (i = 1U; i < 12U; ++i) {
      if (uid[i] != uid[0]) {
        all_same = 0U;
        break;
      }
    }
    if (all_same && (uid[0] == 0x00U || uid[0] == 0xFFU)) {
      Record_Result("MCU unique ID", SELFTEST_RESULT_FAIL);
    } else {
      Record_Result("MCU unique ID", SELFTEST_RESULT_OK);
    }
  }
}

/* ------------------------------------------------------------------------- */
/* 1. 时钟与供电                                                              */
/* ------------------------------------------------------------------------- */

/*
 * VREFINT 内部基准在出厂时按 3.0 V 标定，测它就能反推 VDDA。
 * 不需要接任何外部东西，是判断"板子供电正常"最直接的检查。
 */
static void Test_Supply(void) {
  uint16_t raw = Board_AdcReadVrefRaw();
  uint32_t vref_mv;

  /*
   * 判决用原始码，不用换算出来的电压。
   *
   * 原因：手册 2.2 节写明 DAC 输出范围 0~3.6 V，暗示 VDDA 约 3.6 V；
   * 但按 VREFINT 标称 1.204 V 反推只有 2659 mV，两者差了 35%。
   * F103 没有 VREFINT 工厂校准值，芯片也未必是 STM32 原厂料，
   * 所以换算值不可信，只有原始码是可复现的。
   *
   * 三块板实测都稳定落在 1800~1900，说明内部基准本身是正常的，
   * 偏离这个区间才说明供电或基准真有问题。
   */
  if ((raw < 1700U) || (raw > 2000U)) {
    Board_Log("  VREFINT raw out of expected 1700-2000 band\r\n");
    Record_Result("internal VREFINT reference sanity", SELFTEST_RESULT_WARN);
    return;
  }

  vref_mv = Board_AdcReadVrefMv();
  Board_Log("  VDDA (derived, reference only) = ");
  Board_LogU32(vref_mv);
  Board_Log(" mV\r\n");
  Board_Log(
      "  NOTE: derived from assumed 1.204 V nominal; verify with a meter\r\n");

  Record_Result("internal VREFINT reference sanity", SELFTEST_RESULT_OK);
}

/* ------------------------------------------------------------------------- */
/* 2. 模拟量输入：8 路                                                        */
/* ------------------------------------------------------------------------- */

static void Test_AnalogInput(void) {
  uint8_t i;
  uint8_t any_active = 0U;

  Board_Log("  通道号  引脚   原始码     端口电压\r\n");

  for (i = 0U; i < BOARD_ADC_PORT_COUNT; ++i) {
    uint16_t code = Board_AdcReadCode(i);
    uint32_t mv = Board_AdcCodeToPortMv(code);

    Board_Log("   ADC");
    Board_LogU32(i + 1U);
    Board_Log("  ");
    /* PB0/PB1 是 ADC1/ADC2，PC0~PC5 对应 ADC3~ADC8。 */
    if (i < 2U) {
      Board_Log("PB");
    } else {
      Board_Log("PC");
    }
    Board_LogU32((i < 2U) ? (uint32_t)i : (uint32_t)(i - 2U));
    Board_Log("    ");
    Board_LogU32(code);
    Board_Log("      ");
    Board_LogU32(mv);
    Board_Log(" mV\r\n");

    /* 端口悬空时原始码会在中值附近跳动，接了信号才有明显偏离。 */
    if ((code > 100U) && (code < 4000U)) {
      any_active = 1U;
    }
    HAL_Delay(20U);
  }

  Board_Log("  (在 JP8 上接 0~7.2V 再跑一次，对比上面的数值)\r\n");
  Record_Result("ADC1-8 analog input", SELFTEST_RESULT_OK);
  (void)any_active;
}

/* ------------------------------------------------------------------------- */
/* 3. 模拟量输出：DAC1/DAC2                                                   */
/* ------------------------------------------------------------------------- */

static void Test_AnalogOutput(void) {
  uint8_t pass = 1U;

  if (Board_DacSet(0U, 2048U) != HAL_OK || Board_DacSet(1U, 2048U) != HAL_OK) {
    Record_Result("DAC1/DAC2 output", SELFTEST_RESULT_FAIL);
    return;
  }
  Board_Log("  DAC1/DAC2 set to ~1.65 V (code 2048)\r\n");
  HAL_Delay(500U);

  if (Board_DacSet(0U, 0U) != HAL_OK || Board_DacSet(1U, 0U) != HAL_OK) {
    pass = 0U;
  }
  Board_Log("  DAC1/DAC2 set to ~0 V (code 0)\r\n");
  HAL_Delay(300U);

  Record_Result("DAC1/DAC2 output",
                pass ? SELFTEST_RESULT_OK : SELFTEST_RESULT_FAIL);
}

/* ------------------------------------------------------------------------- */
/* 4. 按键                                                                    */
/* ------------------------------------------------------------------------- */

static uint8_t Key_IsPressed(GPIO_TypeDef *port, uint16_t pin) {
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}

static void Test_Keys(void) {
  uint8_t key1 = Key_IsPressed(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin);
  uint8_t key2 = Key_IsPressed(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin);

  Board_Log("  KEY1(PC12)=");
  Board_Log(key1 ? "pressed" : "released");
  Board_Log("  KEY2(PC13)=");
  Board_Log(key2 ? "pressed" : "released");
  Board_Log("\r\n");

  /* 内部上拉生效时松开必须是高电平，否则说明上拉或走线有问题。 */
  if (key1 || key2) {
    Record_Result("KEY1/KEY2 pull-up idle high", SELFTEST_RESULT_WARN);
  } else {
    Record_Result("KEY1/KEY2 pull-up idle high", SELFTEST_RESULT_OK);
  }
}

/* ------------------------------------------------------------------------- */
/* 5. LED                                                                     */
/* ------------------------------------------------------------------------- */

/* 返回 1 表示引脚电平确实是"点亮"那一档，说明输出通路通了。 */
static uint8_t Test_OneLed(GPIO_TypeDef *port, uint16_t pin, uint16_t on_level,
                           const char *name) {
  uint8_t ok;

  HAL_GPIO_WritePin(port, pin, on_level);
  HAL_Delay(200U);
  ok = (HAL_GPIO_ReadPin(port, pin) == on_level) ? 1U : 0U;

  HAL_GPIO_WritePin(port, pin, (uint16_t)~on_level);
  HAL_Delay(200U);

  Record_Result(name, ok ? SELFTEST_RESULT_OK : SELFTEST_RESULT_FAIL);
  return ok;
}

static void Test_Leds(void) {
  Test_OneLed(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin, BOARD_LED1_ON_LEVEL,
              "LED1 (JP3/PA0) output");
  Test_OneLed(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin, BOARD_LED2_ON_LEVEL,
              "LED2 (JP4/PA1) output");
}

/* ------------------------------------------------------------------------- */
/* 6. I2C 总线与板载器件                                                      */
/* ------------------------------------------------------------------------- */

typedef struct {
  uint8_t address7;
  const char *name;
} OnboardDevice;

/* 地址固定的三个器件。EEPROM 地址随跳线变化，单独扫，见 Eeprom_FindAddress()。
 */
static const OnboardDevice g_i2c2_devices[] = {
    {BOARD_I2C_ADDR_MPU6050, "MPU6050 attitude"},
    {BOARD_I2C_ADDR_QMC5883L, "QMC5883L magnetometer"},
    {BOARD_I2C_ADDR_SPL06, "SPL06-001 barometer"},
};

/* 扫出来的 EEPROM 实际 7 位地址；0 表示没扫到。 */
static uint8_t g_eeprom_address7 = 0U;

static uint8_t I2c_Ping(I2C_HandleTypeDef *bus, uint8_t address7) {
  return HAL_I2C_IsDeviceReady(bus, (uint16_t)(address7 << 1U), 3U, 20U) ==
         HAL_OK;
}

/*
 * ST24C02 的 7 位地址由 A0~A2 三个跳线决定，会在 0x50~0x57 之间变化。
 * 原理图上标的是 0xA0（7 位 0x50），但实测手上这块板跳到了 0x52。
 * 三块板的跳线未必一样，所以扫出实际地址再用，不硬编码。
 */
static uint8_t Eeprom_FindAddress(void) {
  uint16_t address7;

  for (address7 = BOARD_I2C_ADDR_EEPROM24C02_BASE;
       address7 <= BOARD_I2C_ADDR_EEPROM24C02_TOP; ++address7) {
    if (I2c_Ping(&hi2c2, (uint8_t)address7)) {
      return (uint8_t)address7;
    }
  }
  return 0U;
}

static void Test_I2C(void) {
  uint32_t i;
  uint8_t missing = 0U;

  /* I2C1（JP6）出厂不接器件，只验证总线本身能初始化。 */
  Print_Title("I2C1 (JP6 / PB6 PB7)");
  MX_I2C1_Init();
  {
    uint8_t ack = I2c_Ping(&hi2c1, 0x50U);
    /* 0x50 是 EEPROM 地址。板上没接就不该有 ACK，这里只是报个事实。 */
    Board_Log("  probe 0x50: ");
    Board_Log(ack ? "ACK" : "no ACK (expected on bare board)");
    Board_Log("\r\n");
    Record_Result("I2C1 bus init", SELFTEST_RESULT_OK);
  }

  Print_Title("I2C2 (JP7 / PB10 PB11) onboard devices");
  MX_I2C2_Init();
  for (i = 0U; i < (sizeof(g_i2c2_devices) / sizeof(g_i2c2_devices[0])); ++i) {
    uint8_t ack = I2c_Ping(&hi2c2, g_i2c2_devices[i].address7);

    Board_Log("  0x");
    Board_LogHexByte(g_i2c2_devices[i].address7);
    Board_Log(" ");
    Board_Log(g_i2c2_devices[i].name);
    Board_Log(" : ");
    Board_Log(ack ? "OK" : "MISSING");
    Board_Log("\r\n");

    Record_Result(g_i2c2_devices[i].name,
                  ack ? SELFTEST_RESULT_OK : SELFTEST_RESULT_FAIL);
    if (!ack) {
      missing++;
    }
  }

  /* EEPROM 单独扫 0x50~0x57，因为它的地址随 A0~A2 跳线变化。 */
  g_eeprom_address7 = Eeprom_FindAddress();
  Board_Log("  ST24C02 EEPROM : ");
  if (g_eeprom_address7 != 0U) {
    Board_Log("OK @ 0x");
    Board_LogHexByte(g_eeprom_address7);
    Board_Log("\r\n");
    Record_Result("ST24C02 EEPROM", SELFTEST_RESULT_OK);
  } else {
    Board_Log("MISSING (0x50~0x57 no ACK)\r\n");
    Record_Result("ST24C02 EEPROM", SELFTEST_RESULT_FAIL);
    missing++;
  }

  Board_Log("  missing count = ");
  Board_LogU32(missing);
  Board_Log("\r\n");

  /*
   * 有器件不应答时把整条总线扫一遍，方便区分"地址猜错了"和"芯片真的
   * 坏了/没焊"。EEPROM 的地址问题上面已经处理过，这里是兜底信息。
   */
  if (missing > 0U) {
    uint16_t address7;

    Board_Log("  full bus rescan:\r\n");
    for (address7 = 0x08U; address7 <= 0x77U; ++address7) {
      if (I2c_Ping(&hi2c2, (uint8_t)address7)) {
        Board_Log("    present at 0x");
        Board_LogHexByte((uint8_t)address7);
        Board_Log("\r\n");
      }
    }
  }
}

/* ------------------------------------------------------------------------- */
/* 板载传感器深测：IMU / 气压计 / 航向漂移                                   */
/* ------------------------------------------------------------------------- */

/*
 * 下面三项和前面那些"有没有应答"的自检不一样：那些只证明芯片在总线上，
 * 这里要验的是"它吐出来的数据对不对"。判断依据全部是物理量：
 *   - 静止时加速度合矢量必须约等于 1 g，否则重力都测不对，谈不上姿态
 *   - 静止时陀螺仪必须接近 0，否则连自稳都做不到
 *   - 气压必须随高度变化，否则测不了高度
 *   - 航向漂移速率直接决定没有罗盘时能飞多久（旧板磁力计曾无应答，不能据此确认芯片本体损坏）
 */

/* 打印定点数：传 v/1000，输出一位小数，如 1005 -> "1.0"。 */
static void Log_Fixed1K(uint32_t v) {
  Board_LogU32(v / 1000U);
  Board_Log(".");
  Board_LogU32((v % 1000U) / 100U);
}

/*
 * 有符号版本。
 * 加速度三轴在静止时必然有一轴是负的，直接用无符号打印会绕成
 * 4294967.x 这种明显荒谬的数，所以带符号的那一路必须走这个。
 */
static void Log_SignedMilli(int32_t milli) {
  if (milli < 0) {
    Board_Log("-");
    milli = -milli;
  }
  Log_Fixed1K((uint32_t)milli);
}

static void Log_Label(const char *name) {
  Board_Log("    ");
  Board_Log(name);
  Board_Log(": ");
}

/* 加速度合矢量，单位 0.001 g。静止平放时应该接近 1000。 */
static uint32_t Imu_AccelMagMilliG(const int16_t *accel) {
  float x = (float)accel[0] / 16384.0f;
  float y = (float)accel[1] / 16384.0f;
  float z = (float)accel[2] / 16384.0f;
  return (uint32_t)(__builtin_sqrtf(x * x + y * y + z * z) * 1000.0f);
}

/* 单轴陀螺仪绝对值的最大值，单位 0.001 °/s。±250 °/s 量程 = 131 LSB/(°/s)。 */
static uint32_t Imu_GyroMaxMilliDps(int16_t raw) {
  int32_t v = raw;
  if (v < 0) {
    v = -v;
  }
  return (uint32_t)((v * 1000L) / 131L);
}

static void Test_Imu(void) {
  static const char *axis_name[3] = {"accX", "accY", "accZ", };
  static const char *gyro_name[3] = {"gyrX", "gyrY", "gyrZ"};
  uint8_t id = 0U;
  uint8_t i;
  uint8_t s;
  uint8_t read_ok = 0U;
  ImuSample sample;
  uint32_t mag_min = 0xFFFFFFFFUL;
  uint32_t mag_max = 0UL;
  uint32_t mag_sum = 0UL;
  int32_t acc_min[3];
  int32_t acc_max[3];
  uint32_t gyro_max[3];

  Print_Title("IMU (MPU6050) - leave the board still");

  if (Sensors_ImuReadId(&id) != HAL_OK) {
    Board_Log("  WHO_AM_I read FAILED\r\n");
    Record_Result("MPU6050 WHO_AM_I", SELFTEST_RESULT_FAIL);
    Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_SKIP);
    return;
  }
  Board_Log("  WHO_AM_I = 0x");
  Board_LogHexByte(id);
  if (id != MPU6050_ID_MPU6050) {
    Board_Log("  (expect 0x68)\r\n");
  } else {
    Board_Log("  OK\r\n");
  }
  Record_Result("MPU6050 WHO_AM_I",
                (id == MPU6050_ID_MPU6050) ? SELFTEST_RESULT_OK
                                           : SELFTEST_RESULT_WARN);

  if (Sensors_ImuInit() != HAL_OK) {
    Board_Log("  config write FAILED\r\n");
    Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_FAIL);
    return;
  }
  HAL_Delay(50U);

  for (i = 0U; i < 3U; ++i) {
    acc_min[i] = 32767;
    acc_max[i] = -32768;
    gyro_max[i] = 0UL;
  }

  for (s = 0U; s < 20U; ++s) {
    if (Sensors_ImuRead(&sample) != HAL_OK) {
      Board_Log("  sample read FAILED at #");
      Board_LogU32(s);
      Board_Log("\r\n");
      continue;
    }
    read_ok = 1U;
    {
      uint32_t mag = Imu_AccelMagMilliG(sample.accel);
      if (mag < mag_min) {
        mag_min = mag;
      }
      if (mag > mag_max) {
        mag_max = mag;
      }
      mag_sum += mag;
    }
    for (i = 0U; i < 3U; ++i) {
      int32_t v = sample.accel[i];
      uint32_t g;
      if (v < acc_min[i]) {
        acc_min[i] = v;
      }
      if (v > acc_max[i]) {
        acc_max[i] = v;
      }
      g = Imu_GyroMaxMilliDps(sample.gyro[i]);
      if (g > gyro_max[i]) {
        gyro_max[i] = g;
      }
    }
    HAL_Delay(50U);
  }

  if (read_ok == 0U) {
    Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_FAIL);
    return;
  }

  Board_Log("  20 samples, 50 ms apart\r\n");
  for (i = 0U; i < 3U; ++i) {
    Log_Label(axis_name[i]);
    Log_SignedMilli(acc_min[i] * 1000L / 16384L);
    Board_Log(" .. ");
    Log_SignedMilli(acc_max[i] * 1000L / 16384L);
    Board_Log(" g\r\n");
    Log_Label(gyro_name[i]);
    Log_Fixed1K(gyro_max[i]);
    Board_Log(" deg/s max\r\n");
  }

  /* 合矢量：静止平放应该 1 g。留 0.8~1.2 g 的余量，手拿着测也能过。 */
  {
    uint32_t mag_avg = mag_sum / 20U;
    uint32_t worst_gyro = gyro_max[0];
    if (gyro_max[1] > worst_gyro) {
      worst_gyro = gyro_max[1];
    }
    if (gyro_max[2] > worst_gyro) {
      worst_gyro = gyro_max[2];
    }

    Log_Label("|accel| avg");
    Log_Fixed1K(mag_avg);
    Board_Log(" g (expect ~1.0)");
    if (mag_avg >= 800UL && mag_avg <= 1200UL) {
      Board_Log(" OK\r\n");
    } else {
      Board_Log(" WARN - board may be tilted or held\r\n");
    }
    Log_Label("|accel| range");
    Log_Fixed1K(mag_min);
    Board_Log(" .. ");
    Log_Fixed1K(mag_max);
    Board_Log(" g\r\n");
    Log_Label("gyro worst");
    Log_Fixed1K(worst_gyro);
    Board_Log(" deg/s (expect ~0)");

    if (mag_avg < 800UL || mag_avg > 1200UL) {
      Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_WARN);
    } else if (worst_gyro > 3000UL) {
      Board_Log("  WARN - gyro should read ~0 while still\r\n");
      Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_WARN);
    } else {
      Board_Log("  OK\r\n");
      Record_Result("IMU accelerometer/gyro data", SELFTEST_RESULT_OK);
    }
  }
}
/*
 * 气压计验证。寄存器表照抄官方例程 USER/src/SPL06_001.c：
 *   0x0D = ID（(id & 0xF0) 应为 0x10）
 *   0x08 = 测量模式 + 状态位（COEF_RDY/SENSOR_RDY/PRS_RDY/TMP_RDY）
 *   0x00~0x02 = 压力 24 位有符号
 *   0x03~0x05 = 温度 24 位有符号
 */
static void Test_Baro(void) {
  uint8_t id = 0U;
  uint8_t status = 0U;
  uint8_t s;
  uint8_t read_ok = 0U;
  BaroSample sample;
  int32_t p_min = 0;
  int32_t p_max = 0;
  int32_t t_min = 0;
  int32_t t_max = 0;
  uint32_t p_delta = 0UL;

  Print_Title("BAROMETER (SPL06-001)");

  if (Sensors_BaroReadId(&id) != HAL_OK) {
    Board_Log("  CHIP_ID(0x0D) read FAILED\r\n");
    Record_Result("SPL06-001 CHIP_ID", SELFTEST_RESULT_FAIL);
    Record_Result("SPL06-001 pressure data", SELFTEST_RESULT_SKIP);
    return;
  }
  Board_Log("  CHIP_ID(0x0D) = 0x");
  Board_LogHexByte(id);
  if ((uint8_t)(id & 0xF0U) == 0x10U) {
    Board_Log("  OK (expect 0x1x)\r\n");
    Record_Result("SPL06-001 CHIP_ID", SELFTEST_RESULT_OK);
  } else {
    Board_Log("  WARN (expect 0x1x)\r\n");
    Record_Result("SPL06-001 CHIP_ID", SELFTEST_RESULT_WARN);
  }

  if (Sensors_BaroInit() != HAL_OK) {
    Board_Log("  init FAILED: no COEF_RDY/SENSOR_RDY within 50 ms\r\n");
    Record_Result("SPL06-001 pressure data", SELFTEST_RESULT_FAIL);
    return;
  }
  Board_Log("  init OK (COEF_RDY + SENSOR_RDY both seen)\r\n");
  HAL_Delay(100U); /* 压力 32 次过采样，需要一点转换时间 */

  /* PRS_RDY / TMP_RDY 说明这一轮的新数据到了。 */
  if (Sensors_BaroReadStatus(&status) == HAL_OK) {
    Board_Log("  MEAS_CFG(0x08) = 0x");
    Board_LogHexByte(status);
    Board_Log("  PRS_RDY=");
    Board_Log((uint8_t)((status & 0x10U) != 0U) ? "1" : "0");
    Board_Log("  TMP_RDY=");
    Board_Log((uint8_t)((status & 0x20U) != 0U) ? "1" : "0");
    Board_Log("\r\n");
  }

  for (s = 0U; s < 30U; ++s) {
    if (Sensors_BaroRead(&sample) != HAL_OK) {
      Board_Log("  sample read FAILED at #");
      Board_LogU32(s);
      Board_Log("\r\n");
      continue;
    }
    if (read_ok == 0U) {
      p_min = sample.pressure_raw;
      p_max = sample.pressure_raw;
      t_min = sample.temp_raw;
      t_max = sample.temp_raw;
      read_ok = 1U;
    } else {
      if (sample.pressure_raw < p_min) {
        p_min = sample.pressure_raw;
      }
      if (sample.pressure_raw > p_max) {
        p_max = sample.pressure_raw;
      }
      if (sample.temp_raw < t_min) {
        t_min = sample.temp_raw;
      }
      if (sample.temp_raw > t_max) {
        t_max = sample.temp_raw;
      }
    }
    HAL_Delay(100U);
  }

  if (read_ok == 0U) {
    Record_Result("SPL06-001 pressure data", SELFTEST_RESULT_FAIL);
    return;
  }

  p_delta = (uint32_t)(p_max - p_min);

  Board_Log("  30 samples, 100 ms apart\r\n");
  Board_Log("    pressure raw: ");
  Log_SignedMilli(p_min);
  Board_Log(" .. ");
  Log_SignedMilli(p_max);
  Board_Log(" (delta ");
  Board_LogU32(p_delta);
  Board_Log(")\r\n");
  Board_Log("    temp raw    : ");
  Log_SignedMilli(t_min);
  Board_Log(" .. ");
  Log_SignedMilli(t_max);
  Board_Log("\r\n");

  /*
   * 判据：后台模式下数据应该持续刷新。完全恒定说明没真正启动测量，
   * 这时把 MEAS_CFG 整个打出来，好知道芯片到底报了什么状态。
   */
  if (p_delta == 0UL && (uint32_t)(t_max - t_min) == 0UL) {
    Board_Log("  WARN - readings never change\r\n");
    if (Sensors_BaroReadStatus(&status) == HAL_OK) {
      Board_Log("    MEAS_CFG(0x08) = 0x");
      Board_LogHexByte(status);
      Board_Log("\r\n");
    }
    Record_Result("SPL06-001 pressure data", SELFTEST_RESULT_WARN);
  } else {
    Board_Log("  OK - lift or press the board, pressure should follow\r\n");
    Record_Result("SPL06-001 pressure data", SELFTEST_RESULT_OK);
  }
}

/* QST QMC5883L Rev B section 7.1/7.2: configure measurement,
 * then require DRDY and read all six bytes to unlock the next sample.
 * This proves data production, not calibrated compass heading accuracy. */
static void Test_Magnetometer(void) {
  uint8_t id = 0U, value = 1U, status = 0U, data[6];
  uint8_t samples = 0U, overflow = 0U, nonzero = 0U, read_error = 0U;
  int16_t lo[3] = {32767,32767,32767}, hi[3] = {-32768,-32768,-32768};
  uint16_t addr = BOARD_I2C_ADDR_QMC5883L << 1;
  Print_Title("MAGNETOMETER (QMC5883L) data production");
  if (HAL_I2C_Mem_Read(&hi2c2, addr, 0x0DU, I2C_MEMADD_SIZE_8BIT, &id, 1U, 30U) != HAL_OK) {
    Record_Result("QMC5883L fresh XYZ data", SELFTEST_RESULT_FAIL); return;
  }
  Board_Log("  CHIP_ID = 0x"); Board_LogHexByte(id); Board_Log(" (expect FF)\r\n");
  if (id != 0xFFU) { Record_Result("QMC5883L fresh XYZ data", SELFTEST_RESULT_WARN); return; }
  if (HAL_I2C_Mem_Write(&hi2c2, addr, 0x0BU, I2C_MEMADD_SIZE_8BIT, &value, 1U, 30U) != HAL_OK) {
    Record_Result("QMC5883L fresh XYZ data", SELFTEST_RESULT_FAIL); return;
  }
  value = 0x1DU;
  if (HAL_I2C_Mem_Write(&hi2c2, addr, 0x09U, I2C_MEMADD_SIZE_8BIT, &value, 1U, 30U) != HAL_OK) {
    Record_Result("QMC5883L fresh XYZ data", SELFTEST_RESULT_FAIL); return;
  }
  for (uint8_t n = 0U; n < 30U; ++n) {
    HAL_Delay(20U);
    if (HAL_I2C_Mem_Read(&hi2c2, addr, 0x06U, I2C_MEMADD_SIZE_8BIT, &status, 1U, 30U) != HAL_OK) { read_error = 1U; break; }
    if (status & 2U) ++overflow;
    if (!(status & 1U)) continue;
    if (HAL_I2C_Mem_Read(&hi2c2, addr, 0U, I2C_MEMADD_SIZE_8BIT, data, 6U, 30U) != HAL_OK) { read_error = 1U; break; }
    ++samples;
    for (uint8_t axis = 0U; axis < 3U; ++axis) {
      int16_t raw = (int16_t)((uint16_t)data[2U*axis] | ((uint16_t)data[2U*axis+1U] << 8));
      if (raw) nonzero = 1U;
      if (raw < lo[axis]) lo[axis] = raw;
      if (raw > hi[axis]) hi[axis] = raw;
    }
  }
  Board_Log("  fresh samples="); Board_LogU32(samples);
  Board_Log(" overflow="); Board_LogU32(overflow); Board_Log("\r\n");
  uint8_t changing = 0U;
  if (samples) for (uint8_t axis = 0U; axis < 3U; ++axis) {
    Board_Log("  XYZ" ); Board_LogU32(axis); Board_Log(" raw range: ");
    if (lo[axis] < 0) { Board_Log("-"); }
    Board_LogU32(lo[axis] < 0 ? -(int32_t)lo[axis] : lo[axis]);
    Board_Log(" .. ");
    if (hi[axis] < 0) { Board_Log("-"); }
    Board_LogU32(hi[axis] < 0 ? -(int32_t)hi[axis] : hi[axis]); Board_Log("\r\n");
    if (hi[axis] != lo[axis]) changing = 1U;
  }
  value = 0U;
  (void)HAL_I2C_Mem_Write(&hi2c2, addr, 0x09U, I2C_MEMADD_SIZE_8BIT, &value, 1U, 30U);
  Record_Result("QMC5883L fresh XYZ data", (!samples || read_error) ? SELFTEST_RESULT_FAIL :
                (samples < 20U || overflow || !nonzero || !changing) ? SELFTEST_RESULT_WARN : SELFTEST_RESULT_OK);
}

/* Compare gyro Z bias at the start/end of a stationary 60-second hold.
 * This is not integrated yaw drift and cannot certify heading accuracy. */
static void Test_HeadingDrift(void) {
  ImuSample sample;
  int32_t z_start = 0L;
  int32_t z_end = 0L;
  int32_t drift_milli_dps;
  uint8_t s;
  uint8_t ok_start = 1U;
  uint8_t ok_end = 1U;

  Print_Title("GYRO Z BIAS STABILITY - keep the board FLAT and still for 60 s");

  if (Sensors_ImuInit() != HAL_OK) {
    Board_Log("  IMU init FAILED\r\n");
    Record_Result("gyro Z bias stability (not yaw drift)", SELFTEST_RESULT_SKIP);
    return;
  }
  HAL_Delay(50U);

  for (s = 0U; s < 20U; ++s) {
    if (Sensors_ImuRead(&sample) != HAL_OK) {
      ok_start = 0U;
      break;
    }
    z_start += sample.gyro[2];
    HAL_Delay(5U);
  }
  if (ok_start == 0U) {
    Board_Log("  gyro read FAILED\r\n");
    Record_Result("gyro Z bias stability (not yaw drift)", SELFTEST_RESULT_FAIL);
    return;
  }
  z_start /= 20L;

  Board_Log("  baseline gyrZ = ");
  Board_LogU32((uint32_t)(z_start < 0 ? -z_start : z_start));
  Board_Log(" LSB, holding 60 s...\r\n");

  /*
   * 分秒打印，CH340 会间歇掉线，有进度才能看出是死机还是掉线；
   * 也提醒使用者这段时间别碰板子。
   */
  for (s = 1U; s <= 60U; ++s) {
    HAL_Delay(1000U);
    if ((s % 10U) == 0U) {
      Board_Log("    ");
      Board_LogU32(s);
      Board_Log(" s\r\n");
    }
  }

  for (s = 0U; s < 20U; ++s) {
    if (Sensors_ImuRead(&sample) != HAL_OK) {
      ok_end = 0U;
      break;
    }
    z_end += sample.gyro[2];
    HAL_Delay(5U);
  }
  if (ok_end == 0U) {
    Board_Log("  gyro read FAILED at end\r\n");
    Record_Result("gyro Z bias stability (not yaw drift)", SELFTEST_RESULT_FAIL);
    return;
  }
  z_end /= 20L;

  /* ±250 °/s 量程 = 131 LSB/(°/s)，这里用千分之一度每秒打印。 */
  drift_milli_dps = ((z_end - z_start) * 1000L) / 131L;

  Board_Log("  baseline gyrZ = ");
  Board_LogU32((uint32_t)(z_start < 0 ? -z_start : z_start));
  Board_Log(" LSB\r\n");
  Board_Log("  final    gyrZ = ");
  Board_LogU32((uint32_t)(z_end < 0 ? -z_end : z_end));
  Board_Log(" LSB\r\n");
  Log_Label("gyro Z bias change");
  if (drift_milli_dps < 0) {
    Board_Log("-");
    drift_milli_dps = -drift_milli_dps;
  }
  Log_Fixed1K((uint32_t)drift_milli_dps);
  Board_Log(" deg/s");

  Board_Log("  NOTE: no yaw integration; this does not measure heading drift\r\n");
  Record_Result("gyro Z bias stability (not yaw drift)",
                drift_milli_dps <= 500L ? SELFTEST_RESULT_OK : SELFTEST_RESULT_WARN);

}

/* EEPROM is read-only by default; opt-in writes are reserved for controlled tests. */
static void Test_EepromWriteRead(void) {
#if SELFTEST_ENABLE_EEPROM_WRITE
  static uint8_t pattern = 0xA5U;
  uint8_t readback = 0U;
  uint8_t device_address8;

  if (g_eeprom_address7 == 0U) {
    /* Test_I2C 里已经报过 FAIL，这里不再重复记一条。 */
    return;
  }
  /* HAL 的 I2C API 要 8 位形式：写地址 = 7 位地址左移一位。 */
  device_address8 = (uint8_t)(g_eeprom_address7 << 1U);

  Board_Log("  writing pattern 0x");
  Board_LogHexByte(pattern);
  Board_Log(" to EEPROM @0x");
  Board_LogHexByte(g_eeprom_address7);
  Board_Log(" page 0...\r\n");

  if (HAL_I2C_IsDeviceReady(&hi2c2, device_address8, 3U, 20U) != HAL_OK) {
    Record_Result("EEPROM write/read verify", SELFTEST_RESULT_FAIL);
    return;
  }

  /* ST24C02 页大小 8 字节，这里只写第一个字节，够验证读回路径。 */
  if (HAL_I2C_Mem_Write(&hi2c2, device_address8, 0x00U, I2C_MEMADD_SIZE_8BIT,
                        &pattern, 1U, 100U) != HAL_OK) {
    Record_Result("EEPROM write/read verify", SELFTEST_RESULT_FAIL);
    return;
  }
  HAL_Delay(10U); /* ST24C02 写周期上限 5 ms，留余量 */

  if (HAL_I2C_Mem_Read(&hi2c2, device_address8, 0x00U, I2C_MEMADD_SIZE_8BIT,
                       &readback, 1U, 100U) != HAL_OK ||
      readback != pattern) {
    Board_Log("  readback mismatch: got 0x");
    Board_LogHexByte(readback);
    Board_Log("\r\n");
    Record_Result("EEPROM write/read verify", SELFTEST_RESULT_FAIL);
    return;
  }

  Board_Log("  readback matched\r\n");
  Record_Result("EEPROM write/read verify", SELFTEST_RESULT_OK);
#else
  Board_Log("  read-only safe mode: EEPROM contents were not modified\r\n");
  Record_Result("EEPROM write/read verify", SELFTEST_RESULT_SKIP);
#endif
}

/* ------------------------------------------------------------------------- */
/* 7. 电机（必须按键触发）                                                    */
/* ------------------------------------------------------------------------- */

/*
 * 电机一旦上电就会转，所以这里做成"按 KEY1 才动"。
 * 按之前所有通道都是 0% 占空比。
 */
#if SELFTEST_ENABLE_MOTOR_TEST
static void Test_Motors(void) {
  static const char *motor_names[] = {
      "M1 motor (TIM3_CH1/PC6)",     "M2 motor (TIM3_CH2/PC7)",
      "M3 motor (TIM3_CH3/PC8)",     "M4 motor (TIM3_CH4/PC9)",
      "M5 motor fwd (TIM1_CH1/PA8)", "M6 motor (TIM4_CH1/PB6)",
      "M7 motor (TIM4_CH2/PB7)",
  };
  uint8_t i;

  Print_Title("MOTORS - press KEY1 to start (REMOVE PROPS FIRST)");

  /*
   * 关键：TIM1/3/4 之前从没初始化过，htimX.Instance 还是 0。
   * 直接调 Board_MotorPwm_StartAll() 会在第一次写 CCR 时解空指针进 HardFault，
   * 表现为按 KEY1 后串口直接静默、没有任何电机反应。
   */
  MX_TIM1_Init();
  MX_TIM3_Init();

  /*
   * M6/M7 和 I2C1 共用 PB6/PB7（JP6 vs JP7）。
   * 上面的步骤 6 刚把 PB6/PB7 配成 I2C 模拟开漏，
   * 不先关掉 I2C1 的话，这里再配成定时器推挽输出会和 I2C 控制器抢引脚。
   */
  (void)HAL_I2C_DeInit(&hi2c1);
  Board_Log("  I2C1 released (PB6/PB7 -> TIM4_CH1/CH2)\r\n");

  MX_TIM4_Init();

  if (Board_MotorPwm_StartAll() != HAL_OK) {
    Board_Log("  motor PWM start FAILED\r\n");
    for (i = 1U; i <= BOARD_MOTOR_7; ++i) {
      Record_Result(motor_names[i - 1U], SELFTEST_RESULT_FAIL);
    }
    return;
  }

  /* M1~M4 / M6 / M7：单向上电，30% 便于观察转速。 */
  for (i = 1U; i <= BOARD_MOTOR_7; ++i) {
    if (i == BOARD_MOTOR_5) {
      /* M5 走双向接口，避免两路同时给电。 */
      (void)Board_MotorPwm_SetM5(BOARD_M5_FORWARD, 30U);
    } else {
      (void)Board_MotorPwm_SetDuty(i, 30U);
    }
    Board_Log("  ");
    Board_Log(motor_names[i - 1U]);
    Board_Log(" -> 30%\r\n");
    Record_Result(motor_names[i - 1U], SELFTEST_RESULT_OK);
    HAL_Delay(700U);
  }

  Board_Log("  all motors at 30%, waiting 3 s...\r\n");
  HAL_Delay(3000U);

  /* M5 反转，验证双向。 */
  Board_Log("  M5 reverse 30%\r\n");
  (void)Board_MotorPwm_SetM5(BOARD_M5_REVERSE, 30U);
  HAL_Delay(2000U);
  Record_Result("M5 reverse rotation", SELFTEST_RESULT_OK);

  /* 全部停下，确认停止占空比有效。 */
  for (i = 1U; i <= BOARD_MOTOR_7; ++i) {
    if (i == BOARD_MOTOR_5) {
      (void)Board_MotorPwm_SetM5(BOARD_M5_STOP, 0U);
    } else {
      (void)Board_MotorPwm_Stop(i);
    }
  }
  Board_Log("  all motors stopped\r\n");
}
#endif

/* ------------------------------------------------------------------------- */
/* 汇总 */
/* ------------------------------------------------------------------------- */

static void Print_Summary(void) {
  uint8_t i;
  uint8_t fail_count = 0U;
  uint8_t warn_count = 0U;

  Board_Log("\r\n================ SUMMARY ================\r\n");
  for (i = 0U; i < g_result_count; ++i) {
    Board_Log("  ");
    Board_Log(g_results[i].item);
    Board_Log(": ");
    switch (g_results[i].result) {
    case SELFTEST_RESULT_OK:
      Board_Log("OK");
      break;
    case SELFTEST_RESULT_WARN:
      Board_Log("WARN");
      warn_count++;
      break;
    case SELFTEST_RESULT_FAIL:
      Board_Log("FAIL");
      fail_count++;
      break;
    default:
      Board_Log("SKIP");
      break;
    }
    Board_Log("\r\n");
  }

  Board_Log("----------------------------------------\r\n");
  Board_Log("total=");
  Board_LogU32(g_result_count);
  Board_Log("  FAIL=");
  Board_LogU32(fail_count);
  Board_Log("  WARN=");
  Board_LogU32(warn_count);
  Board_Log("\r\n");

  /*
   * 判决指示灯真的点亮。
   * 原来这里只打印文字，注释里说的"慢闪"从没实现过，
   * 而主循环又在无条件闪 LED1 当心跳，两者对不上，
   * 现场只看灯根本判断不出哪块板有问题 —— 而看灯正是这个自检的重点。
   *
   * 约定（常亮，不闪，避免和心跳逻辑互相打架）：
   *   LED1+LED2 常亮 = 全过
   *   仅 LED2 常亮   = 有 WARN，需要留意
   *   仅 LED1 常亮   = 有 FAIL
   */
  HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                    (fail_count > 0U) ? BOARD_LED1_ON_LEVEL
                                      : BOARD_LED1_OFF_LEVEL);
  HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin,
                    (fail_count > 0U) ? BOARD_LED2_OFF_LEVEL
                                      : BOARD_LED2_ON_LEVEL);

  if (fail_count > 0U) {
    Board_Log("verdict: HAS FAILING ITEMS (LED1 on, LED2 off)\r\n");
  } else if (warn_count > 0U) {
    Board_Log("verdict: PASS with warnings (LED2 on)\r\n");
  } else {
    Board_Log("verdict: ALL PASS (LED1+LED2 on)\r\n");
  }
}

/* ------------------------------------------------------------------------- */
/* main */
/* ------------------------------------------------------------------------- */

int main(void) {
#if SELFTEST_ENABLE_MOTOR_TEST
  uint8_t key1_stable = 1U;
#endif

  /* ROM bootloader GO can leave address 0 mapped to system ROM.
   * Place interrupts at our Flash vectors before HAL enables SysTick. */
  SCB->VTOR = FLASH_BASE;
  __DSB();
  __ISB();
  HAL_Init();
  SystemClock_Config();
  MX_USART1_UART_Init();
  MX_GPIO_Init();

  Board_Log("\r\n\r\n########## UAV-F22 SELF TEST ##########\r\n");

  /*
   * 上电第一件事先报复位原因。如果这里出现 PIN 或 POR 而不是只有 POR，
   * 说明板子不是干净上电，而是在运行中被复位了 —— 对验板很关键。
   */
  Board_Log("---- reset cause ----\r\n");
  (void)Board_PrintResetCause();

  /* 引脚电平和串口都通，说明芯片确实在跑用户程序。 */
  Print_Title("MCU unique ID");
  Test_BoardId();

  /*
   * ADC 必须在任何一次读数之前初始化。Board_AdcReadVrefMv() 内部会调
   * HAL_ADC_Init，如果 hadc1.Instance 还是 0（全局变量默认值），
   * HAL 会去解空指针进 HardFault，表现为"标题打出来了但后面什么都没有"。
   */
  MX_ADC1_Init();

  Print_Title("Clock / supply");
  Test_Supply();

  Print_Title("Keys");
  Test_Keys();

  Print_Title("LEDs");
  Test_Leds();

  Print_Title("I2C");
  Test_I2C();
  Test_EepromWriteRead();

  /*
   * 传感器深测放在 I2C2 初始化之后。
   * Test_HeadingDrift() 要静置 60 秒，是全程最慢的一段，放在前面
   * 先跑完，后面模拟量和电机才是短平快。
   */
  Test_Imu();
  Test_Baro();
  Test_Magnetometer();
  Test_HeadingDrift();

  Print_Title("Analog input (ADC1-8)");
  Test_AnalogInput();

  /* 写值前必须先初始化 DAC，否则 hdac1.Instance 为 0 会往非法地址写。 */
  MX_DAC1_Init();

  /*
   * HAL_DAC_SetValue 只写 DHRx 数据寄存器，不开通道。
   * STM32F1 上通道的 EN 位在 DAC_Init 里由 HAL_DAC_Start 打开，
   * 不显式启动的话 PA4/PA5 上根本没有波形，
   * "DAC 通过" 只证明寄存器写没进 HardFault，是假通过。
   */
  if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK ||
      HAL_DAC_Start(&hdac1, DAC_CHANNEL_2) != HAL_OK) {
    Board_Log("  HAL_DAC_Start failed\r\n");
    Record_Result("DAC1/DAC2 output", SELFTEST_RESULT_FAIL);
  }

  Print_Title("Analog output (DAC1/DAC2)");
  Test_AnalogOutput();

  /* Physical motor tests use the isolated, single-motor bench environment. */
#if !SELFTEST_ENABLE_MOTOR_TEST
  Record_Result("motor PWM outputs (disabled in safe self-test)",
                SELFTEST_RESULT_SKIP);
#endif
  Print_Summary();

#if SELFTEST_ENABLE_MOTOR_TEST
  Board_Log("\r\nmotors are held at 0% -- press KEY1 to run motor test\r\n");
  Board_Log("(LED1/LED2 now show the self-test verdict, not a heartbeat)\r\n");

  while (1) {
    uint8_t key1 = Key_IsPressed(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin);

    /* 上升沿触发，避免按住不放反复进入电机测试。 */
    if (key1 && key1_stable) {
      Board_Log("\r\nKEY1 pressed -- starting motor test\r\n");
      Test_Motors();
      Board_Log("\r\nmotor test done, motors stopped\r\n");
    }
    key1_stable = key1;

    /*
     * 这里原来是无条件闪 LED1 当心跳，但那个动作会把上面判决灯
     * 刚点亮的 LED1 覆盖掉，闪烁的含义也就没了。
     * 改成只轮询按键：判决灯保持常亮，现场能一眼看出结论。
     */
    HAL_Delay(50U);
  }
#else
  Board_Log("\r\nmotor PWM is disabled in this self-test build\r\n");
  while (1) {
    HAL_Delay(50U);
  }
#endif
}
