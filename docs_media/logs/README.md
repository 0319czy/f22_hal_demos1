# 日志与数据样例

本目录放**脱敏后的真实日志节选**，用于说明本工程的日志体系与读法。
完整的原始采集日志（数百 MB）留在本地，不进仓库。

## 一、三类日志

| 类型 | 产生方式 | 存放位置 | 典型用途 |
|---|---|---|---|
| **串口遥测** | 飞控每 6 ms 打印一行 `[CL] ...`（约 50 个字段） | 上位机脚本录制为 `.raw.log` | 看实时状态机、传感器健康、闭环中间量 |
| **飞行黑匣子** | 飞行中 10 Hz 采样进 RAM，落地一次性写内部 Flash 两个 4 KB 槽 | 芯片 `0x0803E000` 起 8 KB，SWD dump 得到 `.bin` | 复盘一次飞行的高度/油门/姿态曲线，掉电不丢 |
| **脚本采集** | `scripts/capture_*.py` 抓串口或调试口 | `logs/`、`docs/validation/` 下的 `.jsonl` / `.csv` | 光流标定、遥控延迟、台架推力等专项测试 |

## 二、本目录的文件

| 文件 | 说明 |
|---|---|
| `sample_telemetry_raw.log` | 2026-10-21 地面检查会话的串口输出节选（约 30 行），可见 `[CAL]` 标定、`[FCAL]` 光流自检、`[CL]` 闭环状态行 |
| `sample_ground_check_telemetry.csv` | 从上述日志中解析出的一个**短时起跳段**（10 个样本，最高 64 mm），仅演示字段含义，**不代表达标飞行** |

> 说明：2026-10-21 那次会话以地面检查为主，未做长时悬停，因此节选高度很低。
> 达标飞行（20–100 cm 保持 5 秒以上）的数据在飞行黑匣子里，读取方法见下。

## 三、串口遥测字段（节选）

`[CL]` 行按固定顺序输出 `key=value`，常用字段：

| 字段 | 含义 |
|---|---|
| `state` / `fault` | 闭环状态机与故障码（0 待机、1 起飞、4 悬停、5 降落；3 失联、5 倾斜、6 ToF 丢失…） |
| `dt_ms` | 本周期耗时（用于监控循环抖动） |
| `height_mm` / `vz_mmps` | 融合高度与垂直速度 |
| `tof_mm` / `tof_status` / `tof_age_ms` | 激光测距原始值、状态与数据新鲜度 |
| `squal` / `dx` / `dy` | 光流质量与原始位移增量 |
| `u_permille` / `ref_permille` | 当前油门与悬停参考油门（‰） |
| `target_mm` | 目标高度 |
| `roll_cd` / `pitch_cd` | 姿态角（0.01°） |
| `link` / `packets` / `crc` | 无线链路健康度 |

## 四、读黑匣子的步骤

```bash
# 1) 落地后从芯片 dump 8 KB（槽0 0x0803E000 + 槽1 0x0803F000）
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
  -c "init" -c "reset halt" -c "dump_image /tmp/f22-log.bin 0x0803E000 0x2000" \
  -c "reset run" -c "shutdown"

# 2) 解码为可读文本/CSV（自动识别 v1~v5 样本格式）
python3 scripts/decode_flightdata.py /tmp/f22-log.bin --csv /tmp/flight.csv
```

样本字段（v5，30 字节一条）：

| 偏移 | 字段 | 含义 |
|---|---|---|
| 0 | `u_pm` | 油门需求 ‰ |
| 2 | `height_mm` | 融合高度 |
| 4 | `tof_mm` | 原始测距（无效为 0xFFFF） |
| 6/8 | `roll_cd` / `pitch_cd` | 姿态角 0.01° |
| 10 | `sf` | state << 8 \| fault |
| 12 | `pwm_pm` | 四路最大占空比 ‰ |
| 14 | `flow_gain_pm` | 光流增益 ×1000 |
| 16 | `flow_flags` | 有效/存在/请求/自动降落/模式位 |
| 18/20 | `vx_cms` / `vy_cms` | 光流估计水平速度 |
| 22 | `gyro_z_cdps` | 偏航角速度 0.01°/s |
| 24/26/28 | `corr_roll_pm` / `corr_pitch_pm` / `corr_yaw_pm` | 三轴差动指令 ‰ |

> 改这个结构体时必须同步改 `FL_CAPACITY`（`(4096-32)/30 = 135`），否则会写穿槽位。

## 五、判读经验（来自实际调试）

1. **先看 `fault` 再看现象**：`OUTPUT_FAULT` 往往是 PWM 锁存没清，不一定是控制问题；
2. **高度掉而油门顶到 99%** → 动力不足（电池/重量），不是算法错；
3. **`squal` 低且 `gyro_z` 大** → 机体在自转，此时光流数据不可信；
4. **`tof_age_ms` 变大** → 激光被强光或遮挡干扰，高度环应降级而不是硬算。
