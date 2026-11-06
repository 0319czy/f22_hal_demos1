# F22 四轴飞行器 · 悬停控制工程

> 具身智能实验室 · 新成员课题
> 平台：STM32F103RCT6 ｜ HAL + PlatformIO ｜ 2026 飞行巡航定点赛 V5.0 悬停组

一架手掌大小的四轴飞行器：**起飞后自主停在离地 20～100 cm 的某个高度，保持 5 秒以上**。
仓库包含飞控固件、遥控器固件、上位机脚本、调试文档与汇报材料。

---

## 1. 课题目标（比赛规则）

| 项目 | 要求 |
|---|---|
| 起飞 | 红圈内起飞，无线一键起飞后自主控制 |
| 高度 | 20～100 cm 之间保持 **5 秒以上** |
| 上限 | 全程不超过 1 m |
| 降落 | 一键降落，尽量落在中心红圈 |
| 约束 | 约 1.5 m 系绳连接重物 |

得分：中心红圈 500 分，向外每环递减 20 分；能直接看清中心标记与落环关系另加 20 分。

> 规则原文见 `f22_hal_demos/02_比赛工程/docs/references/比赛实施方案_2026.7.pdf`，
> 逐项实施进度见 `f22_hal_demos/02_比赛工程/PROJECT_PLAN.md`。

## 2. 硬件组成

| 部件 | 型号 / 说明 |
|---|---|
| 飞控主控 | STM32F103RCT6（HAL + PlatformIO，`board = genericSTM32F103RC`） |
| 姿态传感器 | MPU6050（I2C2 @0x68，陀螺仪 + 加速度计） |
| 气压计 | SPL06-001（I2C2 @0x76，本工程仅作辅助） |
| 高度传感器 | VL53L1X 激光测距（长距离模式，33 ms 测量 / 40 ms 周期） |
| 光流传感器 | PMW3901（水平位移估计，配合高度做定点） |
| 无线 | E49 433M 模组（JP12 / USART1，与板载 CH340 共用） |
| 电机 | 四路电机 M1 / M2 / M3 / M6，PWM 驱动 |
| 遥控器 | 自制发射端（`02_比赛工程/remote/tle100`），只有起飞 / 停止两个键 |

## 3. 仓库结构

```
f22_hal_demos1/
├── f22_hal_demos/
│   ├── 01_教学demo/           # 七课教学 demo、教程与手册
│   └── 02_比赛工程/           # 比赛固件主工程（本文重点）
│       ├── src/
│       │   ├── main.c         # 入口：初始化、主循环、按键与状态机
│       │   ├── flight/        # 闭环控制、传感器融合、飞行日志、遥控协议
│       │   ├── sensors/       # ToF / 光流 / 传感器总线驱动
│       │   ├── hal/           # 板级支持（PWM、I2C、UART、ADC）
│       │   └── bench/         # 台架测试程序（推力斜坡等）
│       ├── scripts/           # 编译检查、数据解码、采集与审计脚本
│       ├── tests/             # 主机侧单元测试（不需要硬件）
│       ├── remote/tle100/     # 遥控器固件
│       ├── docs/              # 设计、验证记录与传感器手册
│       └── platformio.ini     # 各编译环境（env）定义
├── 小飞机课题汇报_7分钟版.pptx     # 汇报材料（7 分钟）
├── 小飞机课题汇报_实验室版.pptx     # 汇报材料（11 分钟）
└── 小飞机技术汇报.pptx             # 早期版本
```

> 本仓库为整理版：**不包含** `.pio` 编译产物、原始逐帧采集日志（`logs/`、`*.jsonl`）、
> 历史备份快照（`archive/`）与大体量参考资料库（`无人机资料/`、`竞赛相关技术资料/`，约 3.3 GB，留在本机）。

## 4. 快速开始

### 4.1 编译

```bash
cd f22_hal_demos/02_比赛工程
pio run -e contest_flow_hold_wide_jp13      # 位置保持版（比赛主线）
pio run -e contest_imu_tof_low_hover_wide_jp13   # 无光流版（只做高度）
pio list | grep env   # 查看全部环境
```

产物：`.pio/build/<env>/firmware.bin`

### 4.2 烧录（SWD / CMSIS-DAP）

```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
  -c "adapter speed 100" -c "init" -c "reset halt" \
  -c "flash write_image erase .pio/build/contest_flow_hold_wide_jp13/firmware.bin 0x08000000 bin" \
  -c "reset run" -c "shutdown"
```

SWD 偶发失败（`timeout waiting for algorithm` / `CMD_INFO failed`）时：换 `adapter speed 50`、
确认飞控已上电、或重新插拔调试器。写入后建议读回比对：

```bash
openocd ... -c "init" -c "reset halt" -c "dump_image /tmp/rb.bin 0x08000000 <size>" -c "reset run" -c "shutdown"
sha256sum .pio/build/<env>/firmware.bin /tmp/rb.bin
```

### 4.3 操作按键

| 按键 | 作用 |
|---|---|
| **F2** | 起飞（进入起飞斜坡，之后转自主高度控制） |
| **F3** | 降落 |
| **F4** | 急停（同时触发飞行日志落盘） |

> 起飞前先按一次 F4 清除上电残留的 PWM 锁存，否则可能直接进入 `OUTPUT_FAULT`。

### 4.4 读取飞行数据（黑匣子）

```bash
# 1) 落地后把 8 KB 保留区 dump 出来（槽0 0x0803E000 + 槽1 0x0803F000）
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
  -c "init" -c "reset halt" -c "dump_image /tmp/f22-log.bin 0x0803E000 0x2000" \
  -c "reset run" -c "shutdown"

# 2) 解码
python3 scripts/decode_flightdata.py /tmp/f22-log.bin --csv /tmp/flight.csv
```

## 5. 控制架构

三层串级，一次只解决一个问题：

```
第 3 层  位置保持   光流位移 → 期望速度 → 倾角指令（加分项）
第 2 层  高度控制   激光测距 → 期望爬升速度 → PI → 总距（比赛核心）
第 1 层  姿态控制   陀螺仪角速度环 + 角度环 → 四路差动（安全底线）
```

要点：

- **前馈**：悬停油门固定/学习值（实测本机悬停带 800～830‰），PI 只做小修正；
- **抗饱和**：下砍有下限（不低于前馈 − 12%），避免"砍过头 → 下沉 → 越掉越快"；
- **偏航**：无旋转补偿的光流在机体自转时会把旋转误判成平移，因此偏航角速度超门限时暂停采信光流；
- **参数集中**：全部可调参数在 `src/flight/closed_loop_tuning.h`（均有 `#ifndef` 保护，可由 env 覆盖）。

## 6. 飞行黑匣子

- 飞行中以 10 Hz 采样，先写 RAM；回到待机状态时一次性写入内部 Flash 两个 4 KB 槽（轮转，掉电不丢）；
- 记：油门、融合高度、原始 ToF、横滚/俯仰角、最大占空比、光流增益与标志、水平速度、偏航角速度、三轴差动；
- 单条样本 **30 字节**，槽容量 135 条（`(4096-32)/30`，改样本结构时必须同步改 `FL_CAPACITY`，否则写穿槽位）；
- 头部 32 字节含 `magic 'F22L'`、版本、序号、时长、CRC32，便于判断记录是否完整。

## 7. 测试

```bash
cd f22_hal_demos/02_比赛工程
bash scripts/check_closed_loop.sh     # 主机侧回归：编译 + 多 profile 单元测试（不需要硬件）
```

覆盖：油门斜坡、起飞/降落状态机、光流位置环、PWM 看门狗、日志结构等。

## 8. 已知限制与后续

| 项目 | 状态 |
|---|---|
| 高度保持（20～100 cm） | 已达标，稳定工作点约 40～80 cm |
| 悬停时长 | 已达成 5 秒以上；受电池与整机重量影响较大 |
| 横向漂移 | 大幅改善；光流轴序尚未用地面标定验证（`CL_FLOW_AXES_VERIFIED = 0`） |
| 动力余量 | 同油门满电可飞 60 cm，电量不足只能 15 cm，需换高倍率电池或减重 |
| 下沉涡环 | 快速下降会掉进自身尾流，已改为禁止主动下降 |
| 惯性辅助定位 | 曾尝试加速度互补滤波，实测发散，已关闭并保留开关 |

## 9. 安全

1. 调试电机一律**拆桨**；
2. 首次上电先看串口 banner 的环境名与参数，确认烧的是对的固件；
3. 系绳按要求连接重物，场地留足半径；
4. 出现异常（异响、突然偏转、落地不稳）先断电，再读黑匣子定位原因。

## 10. 文档索引

| 文档 | 内容 |
|---|---|
| `02_比赛工程/docs/COMPETITION.md` | 比赛规则与实施进度 |
| `02_比赛工程/docs/CONTROL_DESIGN_2026-10-16.md` | 控制设计 |
| `02_比赛工程/docs/FLOW_ASSIST.md` | 光流辅助与位置保持 |
| `02_比赛工程/docs/AIRCRAFT1_CLOSED_LOOP.md` | 闭环实现说明 |
| `01_教学demo/教学总讲义.md` | 教学讲义（从通信到控制） |
| `01_教学demo/README.md` | 教学 demo 目录说明 |

---

本项目在实验室已有的套件、场地与工程规范上完成，个人工作集中在调试参数、修改控制代码与试飞数据分析。
