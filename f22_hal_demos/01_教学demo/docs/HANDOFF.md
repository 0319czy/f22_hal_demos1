# UAV-F22 综合自检项目 — 交接文档

> 本文件保留2026-10-02历史自检记录。当前七课工程、硬件状态和更谨慎的故障结论以[总入口](../README.md)及[硬件梳理](HARDWARE_OVERVIEW.md)为准；QMC无ACK证明板级通信异常，未单独确认芯片本体损坏。

> **当前安全模式（2026-10-07）**：`demo_selftest` 只读检查，不写 EEPROM，也不编译/触发电机 PWM。旧记录中 EEPROM 写入和全电机测试仅描述历史版本。单电机测试请使用 `demo03_motor_bench`，先拆桨。

> 历史交接内容最后更新：2026-10-02；当前安全自检构建更新：2026-10-07
> 代码位置：`/home/czy/f22_hal_demos1/f22_hal_demos/01_教学demo`
> 目标平台：UAV-F22 飞控板（STM32F103RCT6），HAL + PlatformIO 工程

新增 VOFA+ 实时传感器遥测环境 `demo_vofa`：115200 / JustFloat / 50 Hz，现有 20 个通道。
Cube 四元数绑定及全部通道定义见 [VOFA.md](VOFA.md)。综合自检仍使用 `demo_selftest`。

---

## 1. 一句话说明这个项目

给手上 3 块 UAV-F22 板写**一个固件**，上电跑一遍就把 MCU、供电、按键、LED、
I2C、EEPROM 地址探测、两个板载传感器、航向漂移、ADC、DAC 全查完，末尾打印汇总表，
FAIL 项 LED1 常亮。接手的板子只需要串口连上跑一次，不用挨个烧例程。

---

## 2. 快速开始

```bash
cd /home/czy/f22_hal_demos1/f22_hal_demos/01_教学demo

# 编译
pio run -e demo_selftest

# 烧录（自动找 CH340 端口，不用手填 ttyUSBx）
pio run -e demo_selftest -t upload

# 抓一份输出存下来看
python3 /tmp/opencode/runcap.py
```

当前安全自检构建产物约 24.5 KB / 256 KB。下文设备测试过程和旧的电机/EEPROM写入记录属于历史版本；现在的 `demo_selftest` 不写 EEPROM，也不输出 PWM。

**烧录注意**：CH340 会间歇掉线，内核会重新分配 `ttyUSB` 编号。
`platformio.ini` 里挂了 `extra_scripts/ch340_port.py`，按 USB VID `1a86` 定位，
不用管编号变不变。烧录失败直接重试即可，实测第 2 次通常就成功。

**烧录原理**（已实测有效，不要改回去）：板载 CH340 的 RTS# 经 R60(1K) 驱动
Q8(S8550) 反相，再经 R61(1K) 拉高 BOOT0；DTR 脉冲产生 NRST。
因为 Q8 反相，本板 RTS# 释放(高)才等于 BOOT0 高，顺序与 RCB6412 那套相反：

```
upload_flags = -R -i -rts,-dtr,dtr:rts,-dtr,dtr
```

`-R` 让 stm32flash 写完执行 GPIO 序列里 `:` 后的退出序列（释放 RTS 把 BOOT0
拉低并复位）。没有 `-R` 板子写完会停在 bootloader 里不跳转到新程序。

**没有 ST-Link**，这台机器上也不应该用 ST-Link，CH340 串口 ISP 是唯一路径。

---

## 3. 目录结构

```
f22_hal_demos/
├── platformio.ini            # 16 个有效构建环境；demo_selftest 是只读综合自检
├── extra_scripts/
│   └── ch340_port.py         # 按 VID 1a86 自动定位串口
├── docs/
│   └── HANDOFF.md            # 本文件
└── src/
    ├── hal/                  # 板级驱动，所有例程共用
    │   ├── Inc/
    │   │   ├── board_f22.h   # 引脚 / I2C 地址 / 端口电平，195 行
    │   │   ├── adc.h  common.h  dac.h  gpio.h  i2c.h
    │   │   ├── sensors.h     # MPU6050 + SPL06-001 驱动接口
    │   │   ├── system_clock.h  tim.h  usart.h
    │   │   └── stm32f1xx_hal_conf.h
    │   └── Src/
    │       ├── adc.c   common.c  dac.c  gpio.c  i2c.c
    │       ├── sensors.c        # 两个传感器的寄存器级驱动
    │       ├── system_clock.c  tim.c  usart.c
    │       └── stm32f1xx_it.c
    └── demo/
        ├── 00_template/  01_gpio_led/  02_key_led/
        ├── 05_i2c_scan/
        └── demo_selftest/main.c    # ★ 综合自检，1125 行
```

`demo_selftest` 是板载只读诊断例程之一；教学课入口见项目根目录 README。

---

## 4. 自检固件跑什么

| # | 项目 | 判据 |
|---|------|------|
| 1 | 复位原因 | 上电只应有 POR，出现 PIN 说明运行中被复位 |
| 2 | MCU unique ID | 打印 96 位 ID |
| 3 | VREFINT | raw 1700~2000，实测 ~1898 |
| 4 | 按键 KEY1/KEY2 | 上拉空闲为高 |
| 5 | LED1/LED2 | PA0/PA1 可控 |
| 6 | I2C1 (JP6) | 总线能初始化，0x50 无 ACK 属正常（空板） |
| 7 | I2C2 (JP7) | 扫板载器件 + EEPROM 地址探测（当前只读） |
| 8 | IMU MPU6050 | WHO_AM_I=0x68、\|accel\| 0.8~1.2g、gyro 近 0 |
| 9 | 气压 SPL06-001 | CHIP_ID=0x1x、COEF_RDY/SENSOR_RDY、数据会变 |
| 10 | 航向漂移 60s | 起止 gyroZ 均值之差换算成 deg/s |
| 11 | ADC1-8 | 打印 8 通道 raw 与端口电压 |
| 12 | DAC1/DAC2 | 置码后启通道 |

当前安全构建跳过 EEPROM 写入，并完全关闭电机 PWM。历史版曾由 KEY1 触发全路电机；这段只作旧记录，不能按旧步骤在当前板上执行。需要检查电机时使用第 3 课单路台架固件并拆桨。

汇总行形如 `total=18  FAIL=1  WARN=0`。

---

## 5. 最近一次实测结果（板 3）

```
total=18  FAIL=1  WARN=0
唯一 FAIL = QMC5883L magnetometer（板 3 硬件故障）
```

关键读数：

```
VREFINT raw=1898    VDDA(反推) = 2597 mV
TEMPSENS raw=1799 -> -2.5 C

CHIP_ID(0x0D)=0x10 OK      init OK (COEF_RDY + SENSOR_RDY)
MEAS_CFG(0x08)=0xF7        PRS_RDY=1  TMP_RDY=1
pressure raw: -211.8 .. -211.8 (delta 60)
temp raw    : 2200.3 .. 2206.9

IMU: WHO_AM_I=0x68  |accel| avg 0.9g  accZ 0.9~1.0g  gyro worst 0.4 dps
航向: baseline gyrZ=27 LSB -> final 27 LSB, drift 0.0 deg/s (60s)

ADC1 PB0=3975(6989mV)  ADC2 PB1=3975(6989mV)  ADC3~8=0
EEPROM: 0x52 写 0xA5 回读一致
I2C2 总线重扫: 0x52 / 0x68 / 0x76 在，0x0D 不在
```

三块板 UID：

| 板 | UID |
|----|-----|
| 1 | `30FFD60530574B3030630143` |
| 2 | `30FFDA0530574B3023531443` |
| 3 | `30FFD90530574B3036751643` |

---

## 6. 三个传感器的结论（这部分花的时间最多，记清楚免得重走）

### MPU6050 @ 0x68（I2C2）— 正常
`WHO_AM_I=0x68`，静止 `|accel|≈0.9~1.0g`，gyro 静态最差 0.4 dps。

### SPL06-001 @ 0x76（I2C2）— 正常，**寄存器表踩过坑**

一开始读出来全是 0，差点判成芯片坏。根因是**我按 BME280 的习惯去猜寄存器
地址**，而 SPL06-001 虽然也在 0x76，排布完全不同。最后是翻官方例程源码
才拿到权威寄存器表：

```
/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/3_官方例程/UAV-F22之应用例程/UAV-F22之应用__SPL06-001/
    USER/src/SPL06_001.c        ← 权威，注意是 GBK 编码，grep 要加 -a
```

正确寄存器表：

| 地址 | 含义 |
|------|------|
| `0x00`~`0x02` | 压力，24 位有符号 |
| `0x03`~`0x05` | 温度，24 位有符号 |
| `0x06` | PRS_CFG，压力过采样率/输出速率 |
| `0x07` | TMP_CFG，温度过采样率/速率/温源 |
| `0x08` | MEAS_CFG，测量模式 + 状态位 |
| `0x09` | CFG，中断/FIFO/数据覆盖 |
| `0x0B` | FIFO_STS |
| `0x0C` | RESET，`0x09`=软复位，`0x80`=FIFO 冲洗 |
| `0x0D` | **芯片 ID**，`(id & 0xF0)` 应为 `0x10` |
| `0x10`~`0x21` | 出厂标定系数 |

MEAS_CFG(`0x08`) 状态位：`0x80` COEF_RDY、`0x40` SENSOR_RDY、
`0x20` TMP_RDY、`0x10` PRS_RDY。低 2 位是模式：
`0`=待机、`1`=单次压力、`2`=单次温度、`7`=后台压力+温度。

初始化顺序（照抄官方）：
1. 轮询 `0x08` 直到 `COEF_RDY`(0x80)
2. 读标定系数 `0x10~0x21`
3. 轮询 `0x08` 直到 `SENSOR_RDY`(0x40)
4. 读 ID `0x0D`，校验 `(id & 0xF0) == 0x10`
5. 压力 32 次过采样 + 128Hz → `PRS_CFG = (7<<4)|5 = 0x75`
6. 温度 8 次过采样 + 32Hz + 外部温源 → `TMP_CFG = (1<<7)|(2<<4)|3 = 0xA3`
7. 过采样 >8 次必须置 P-SHIFT：`CFG(0x09) |= 0x04`，否则数据被丢
8. `MEAS_CFG = 0x07` 进后台模式

改完后一次通过，`MEAS_CFG` 回读 `0xF7` = 四个 RDY 全置位 + 模式 7。

### QMC5883L @ 0x0D（I2C2）— 板 3 硬件故障

**不是代码问题**，已排除：
- `HAL_I2C_IsDeviceReady()` 只测 ACK，不碰寄存器语义，不可能是"误判"
- 地址 `0x0D`、总线 `hi2c2` 正确；已重试 3 次 × 20ms
- **同一份固件**在板 1、板 2 正常应答，只有板 3 不应答
- 全总线重扫只有 `0x52 / 0x68 / 0x76`，`0x0D` 确实不存在

结论：板 3 磁力计坏了。**注意它只影响航向（罗盘纠偏）**，
MPU6050 和气压计都独立工作。

---

## 7. 硬件要点（踩过的坑）

### 电机

- **不需要外置电调**。手册 2.9 明确板载集成驱动，可直驱 6 路空心杯 + 1 路双向减速电机。
- **PWM 极性是低有效**（`BOARD_PWM_ACTIVE_LEVEL = GPIO_PIN_RESET`）。
  用户实测：接电后只有按 KEY1 才转，且所有电机都转 —— 这验证了低有效行为。
- **空心杯 6 路全部单向**，要反向就交换对应 M 口的两根电机线。
- 多旋翼安装：相邻方向相反、对角相同，桨叶旋向必须匹配，否则打转或烧驱动。
- M5 是唯一的双向直流减速电机（TIM1_CH1 / PA8），别当空心杯旋翼口用。

### PWM 频率

官方 DM22 手册要求 **约 15 kHz**。当前值（沿用厂家例程）：

| 定时器 | 电机 | 当前值 | 实际频率 | 建议 |
|--------|------|--------|----------|------|
| TIM3 | M1~M4 空心杯 | `BOARD_TIM3_PERIOD_US 83` | ≈11.9 kHz | **改成 66 → ≈14.9 kHz** |
| TIM1 | M5 减速电机 | `1000` | 1 kHz | 保持（低频避免啸叫） |
| TIM4 | M6/M7 | `1000` | 1 kHz | **改成 66** |

改动位置 `src/hal/Inc/tim.h:18-19`。**这项还没做。**

### M5 反转已放弃（重要）

原设计用 `TIM1_CH4 / PA11` 做 M5 反转。**PA11 是 MCU 的 `USB_DM`**，
绝对不能占用；TIM1_CH4 也没有其他可替代的引脚（PB14 需开全重映射）。

已从 `board_f22.h` / `tim.c` 移除，**M5 目前只能正转**，
`Board_MotorPwm_SetM5(BOARD_M5_REVERSE, ...)` 返回 `HAL_ERROR`。

### I2C 与 PWM 引脚冲突

`M6`(PB6) / `M7`(PB7) 与 `I2C1`(JP6) 共用引脚，**不能同时用**。
官方手册把 PB6/PB7 标了 M6/M7 复用，调试时已释放 PB6/PB7 供 I2C1 使用。
VL53LXX 测距模块应接 **JP7 / I2C2**，不要接 JP6。

### 供电

- 手册要求 3.7V 电池从 `CON+/-` 输入时，**`JP9` 也要接同一电池**。
- `JP10` 是 3.3V 双向口，**不应往里灌 3.78V**。
- VREFINT raw≈1898，反推 VDDA≈2597 mV，与预期 3.3V 差约 0.7V。
  VREFINT 标称按 1.204V 反解，**真实 VDDA 必须用万用表确认**。
  这个反推值在 `Board_AdcReadVrefMv()` 里有注释说明，仅供参考。

### ADC 电压换算

`BOARD_ADC_FULL_SCALE_MV = 7200`（JP8 端口按 0~7.2V 设计）。
所以 ADC1/ADC2 raw=3975 会显示 6989mV —— **这不是 bug**，
是按 7.2V 满量程换算的结果。要判断通道好坏应看 raw 值本身。
未接外部信号时 ADC3~8 读 0 属正常。

---

## 8. 已知问题 / 待办

按优先级：

1. **CH340 掉线**（影响所有长时间测试）
   实测前 8 秒不在线、第 10 秒自动恢复；端口号会在 `ttyUSB0/1` 之间跳。
   航向 60 秒测试因此经常被重置重跑，最终汇总仍能跑完但耗时翻倍。
   `/tmp/opencode/runcap.py` 会在掉线后**不复位重连续抓**，凑不齐就再烧一次。
   根本原因疑似 CH340 芯片或线材质量，**未定位**。

2. **TIM3/TIM4 调到 15kHz** — 一行改动，见第 7 节。

3. **EEPROM 写入**
   当前安全构建只探测地址，不写 page 0，避免覆盖板上已有数据。旧版历史记录中的 `0xA5` 写入已停用。

4. **`Test_HeadingDrift()` 方法有缺陷**
   它只比较起点和终点的 gyroZ **均值**，不是累计积分 yaw。
   零偏稳定时会显示 0 deg/s（本次就是这样），但**掩盖了缓慢的累积漂移**。
   正确做法应改成累加 `∫gz·dt`。本次实测 baseline 27 LSB → final 27 LSB，
   60 秒内确实稳定，但测不出长期漂移。

5. **`Test_Supply()` 重复采样 VREFINT**
   应复用 `Board_AdcLastVrefRaw()`，避免二次转换。

6. **板上曾报告元件短路/烧坏**，位置、色环、翘脚情况仍未知。
   **未定位前不要反复上电或做盲目通断判断。**

7. **PMW3901 光流 / VL53LXX 测距驱动未实现**
   等模块型号 + 正反面照片 + 引脚丝印。

---

## 9. 比赛资料索引

全部资料在 `/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/`，六个目录：

### 1_硬件手册/

| 文件 | 用途 |
|------|------|
| `UAV-F22 v2.0.pdf` | **原理图**，查引脚、电源树时看这个 |
| `UAV-F22 v1.1.pdf` | 旧版原理图，交叉参考 |
| `UAV-F22无人机控制器用户手册 v2.0.pdf` | **主手册**：2.9 板载驱动、2.21 气压传感、JP 口定义 |
| `UAV-DM22无人机电机组件产品手册.pdf` | 电机规格、**约 15kHz PWM 要求**、接线、厂家联系方式 |
| `UAV-DM22 v1.0.pdf` | 电机手册另一版本 |

### 2_遥控器/

TLE100 可编程遥控装置（TLE100.pdf + 用户手册 + 编程例程 zip），
以及两套已编译好的遥控器端工程：`TLE100控制（遥控器端）/` 和
`UAV-F22控制（遥控器端）/`（Keil C51 工程，含 ex3.hex）。
另有 `433M无线通信定点模式配置方法pdf`。

### 3_官方例程/ （367 MB）

`UAV-F22之应用例程/` 下 14 个例程，**排查外设行为时的权威依据**：

| 例程 | 对应本项目 |
|------|-----------|
| `UAV-F22之应用__SPL06-001` | **sensors.c 的寄存器表来源**（GBK 编码，grep 要 `-a`） |
| `UAV-F22之应用__mpu-6050` | IMU 参考 |
| `UAV-F22之应用__qmc5883l` | 磁力计参考 |
| `UAV-F22之应用__空心杯电机的控制与应用` | M1~M4 PWM |
| `UAV-F22之应用__直流减速电机控制应用` | M5（注意官方例程也用 PA8） |
| `UAV-F22之应用_PWM波的产生与应用` | PWM 频率/极性 |
| `UAV-F22之应用_IIC通信控制与EEPROM应用` | EEPROM |
| `UAV-F22之应用__超声波测量与应用` | 测距 |
| `UAV-F22之应用_模拟与数字转换的实现与应用` | ADC/DAC |
| `UAV-F22之应用_LED控制输出应用` / `串口通信应用` / `无线收发` 等 | 其余外设 |

源码在 `<例程名>/USER/{main.c,src/*.c,inc/*.h}`，编译产物在 `build/`（可忽略）。
另有 `F22演示视频.mp4` 和 `UAV-F22之应用__接收遥控器控制/`。

### 4_算法理论/

四旋翼姿态解算与控制，**做飞控算法时按这个顺序读**：

- 入门：`姿态解算说明（Mini AHRS）.pdf`、`惯性导航基本原理-(入门两天半).pdf`
- 核心：`四元数与欧拉角之间的转换.pdf`、`四元数解算姿态完全解析及资料汇总（1~3）.docx`
- 滤波：`基于互补滤波器的四旋翼飞行器姿态解算.pdf`、
  `叉积法融合陀螺和加速度核心程序详解（圆点博士四轴）.pdf`
- 控制：`基于PI_PD控制器的四旋翼姿态控制_唐健杰.pdf`
- 权威教材：`惯性导航_秦永元.pdf`（9.8 MB）

### 5_地图影像/

`飞行定点.mp4`、`飞行定点图片.jpg`、`飞行巡航-模型.pdf` — 定点/巡航任务的
实际演示与地图底图，**比赛任务要求可以从这里反推**。

### 6_开发工具/

`Progisp1.72下载操作使用说明书.pdf`、`mcuisp下载操作使用说明书.pdf`。
注意本项目走 PlatformIO + stm32flash（CH340 自动 ISP），
与厂家这两套工具是不同路径，但对理解 BOOT0/NRST 时序有帮助。

---

## 10. 关键操作备忘

| 事项 | 命令 / 做法 |
|------|-------------|
| 编译 | `pio run -e demo_selftest` |
| 烧录 | `pio run -e demo_selftest -t upload`（自动找端口） |
| 手动指定端口 | `pio device list` |
| 抓输出存文件 | `python3 /tmp/opencode/runcap.py` |
| 看输出 | `cat /tmp/opencode/sens.txt \| tr -d '\r'` |
| 电机测试 | 使用 `demo03_motor_bench`；拆桨后一次只测一路 |
| 查官方例程 | `grep -ra <keyword> /home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/3_官方例程/`（**必须带 `-a`，例程是 GBK 编码**） |

**官方资料路径**：

```
/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/1_硬件手册/UAV-F22 v2.0.pdf                    # 原理图
/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/1_硬件手册/UAV-F22无人机控制器用户手册 v2.0.pdf  # 用户手册
/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/1_硬件手册/UAV-DM22无人机电机组件产品手册.pdf     # 电机规格
/home/czy/f22_hal_demos1/f22_hal_demos/无人机资料/3_官方例程/UAV-F22之应用例程/                   # 各外设例程
```

厂家联系方式（DM22 手册内）：`sales_infeeon@126.com` /
`support_infeeon@126.com` / `13823123830` / `18588193385` / QQ `3462348702`。

---

## 11. 接手后建议先做的事

1. 万用表量一下 3.3V 和 VDDA，确认供电 —— 这是 `VDDA≈2597mV` 疑点的答案。
2. 改 TIM3/TIM4 到 66（15kHz），示波器确认 DM22 实际驱动频率。
3. 把 `Test_HeadingDrift()` 改成积分法，测真实 60 秒累计漂移。
4. 拆桨后跑电机测试，7 路逐个确认，M5 反转预期返回 `HAL_ERROR`（已知限制）。
5. 板 3 的 QMC5883L 单独修板：查 VCC/GND、SDA/SCL 连续性、补焊。
6. 解决 CH340 掉线（或接受重试），再做长时间测试。
