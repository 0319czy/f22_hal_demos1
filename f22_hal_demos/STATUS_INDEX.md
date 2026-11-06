# F22 工程现状索引

> **更新**：2026-10-26
> **本机路径**：`/home/czy/f22_hal_demos1/f22_hal_demos`
> **用途**：接手或继续开发前先读本文。本文只写**当前可核实的事实**；历史过程看 [HANDOFF_2026-10-26.md](HANDOFF_2026-10-26.md)、[02_比赛工程/PROJECT_PLAN.md](02_比赛工程/PROJECT_PLAN.md) 和 `02_比赛工程/docs/validation/`。
> **维护**：每轮实机后更新第 6 节"复验基线"和第 4.5 节"已完成/未完成"，原始证据存 `02_比赛工程/docs/validation/<日期>-<主题>/`。

## 1. 一句话现状

本仓库是**一架 F22 四轴**（STM32F103RC + HAL + PlatformIO）的教学工程 + 悬停组比赛工程。
比赛工程的软件闭环、主机回归、多环境构建是齐的；但**真实飞行验收没有完成**：
未证明能稳定离地、悬停、横向保持和落回中心。当前结论是 **PARTIAL，不能上场**。
下一阶段的主攻点不是加功能，而是：**现场确认板上固件 → 恢复可控系留短飞 → 稳住姿态 rate 环 → 解决 ToF 近地盲区 → 独立完成光流标定**。

## 2. 本机路径与仓库状态

| 项 | 值 |
|---|---|
| 工作区 | `/home/czy/f22_hal_demos1/f22_hal_demos`（仓库根） |
| git | 分支 `main`，HEAD `dbf3dc5`（共 5 个提交，跟踪 `origin/main`） |
| 工作区状态 | 37 个已跟踪文件被修改 + 136 个未跟踪项；比赛工程的大部分源码、文档、证据**尚未入库** |
| 未提交改动时段 | 2026-10-24～10-26（VOFA 遥测、运行期调参、JP13 上行诊断），见第 4.4 节 |
| 文档落后代码 | `PROJECT_PLAN.md` 最后记录停在 10-21，10-24/25 的改动没有对应记录 |
| 工具链 | `pio` = `~/.local/bin/pio`（penv 同）；`arm-none-eabi-*` = `~/.platformio/packages/toolchain-gccarmnoneeabi/bin`；主机 `cc` = 系统 gcc |
| 本机缺失 | **没有 ripgrep**（`rg`），已因此修正 `check_closed_loop.sh`（见第 6 节） |
| 修改前备份 | `/home/czy/f22_hal_demos1/_pathfix_backup_20261026/`（2026-10-26 路径修正前的 18 个文件） |

> 2026-10-26 已把操作性文档里的过期路径（`/home/sz/coderepo/...`、`/data/coderepo/...`、`/data/Downloads/...`、`/home/sz/.platformio`、`/home/sz/无人机资料`）统一改为本机实际路径。
> **`docs/validation/**`、`docs/architecture 类历史记录、archive/、fly.md 的调试记录没有改**——它们是当时的证据，改了就失真；里面出现的旧路径按"当时机器"理解。

## 3. 目录地图

| 路径 | 性质 | 说明 |
|---|---|---|
| `01_教学demo/` | 活代码 | 7 课教学工程，独立入口，17 个 PlatformIO 环境，默认不驱动电机 |
| `02_比赛工程/` | 活代码 | 唯一比赛开发目录；飞控闭环、传感器、遥控、台架诊断 |
| `02_比赛工程/docs/` | 活文档 | 27 份专题文档（控制、标定、接线、事故、验证） |
| `02_比赛工程/docs/validation/` | **只读证据** | 62 个日期目录：命令、串口原文、烧录读回、分析 |
| `02_比赛工程/firmware/` | 历史快照 | 50 个 `.bin`，按名字猜版本会错；用途见 `firmware/README.md` |
| `02_比赛工程/remote/tle100/` | 活代码 | 遥控器 AT89S52 独立工程（SDCC），当前配置 `tle100_latency_fast` |
| `02_比赛工程/propgen/` | 独立子仓库 | 3D 打印桨生成器（Python/build123d），自带 `.git`，与飞控无关 |
| `02_比赛工程/archive/` | 历史 | 不做编译/扫描，不按里面说明烧录 |
| `竞赛相关技术资料/`、`无人机资料/` | 资料 | 被 `.gitignore` 排除；规则以 `docs/references/比赛实施方案_2026.7.pdf` 为准 |
| `教学资料.zip` | 交付包 | 由 `01_教学demo/scripts/package_teaching.py` 生成 |

## 4. 比赛工程（`02_比赛工程`）

### 4.1 控制链与关键文件

```text
TLE100 -> USART1/E49 接收 -> 命令门控 -> ClosedLoop_Step
IMU --------------------------^          |
ToF -> 高度观测/垂直环 ------------------ |
PMW3901 -> 光流辅助(默认关闭) ----------- |
姿态/角速度/高度/光流 -> 混控 -> M1/M2/M3/M6 PWM
                              -> JP13 遥测/日志（含 VOFA、运行期调参）
```

| 文件 | 职责 |
|---|---|
| `src/main.c` | 板级主循环、采样、遥控命令、状态与遥测、VOFA 输出、运行期调参协议 |
| `src/flight/closed_loop.c` | 起飞/保持/降落/失联/故障状态机与混控 |
| `src/flight/closed_loop_tuning.h` | **唯一默认调参入口**（trim、rate 环、ToF 高度环、起飞保护、光流） |
| `src/flight/closed_loop_profile.h` | profile 宏、PWM 上限、输出锁、起飞斜坡与超时 |
| `src/flight/aircraft1.c` | IMU 坐标映射、四电机映射、启动门控 |
| `src/flight/observation.c` | ToF/IMU 高度观测与短时融合 |
| `src/flight/flow_assist.c` | 光流相对 XY 保持（试验性，默认不参与） |
| `src/hal/`、`src/sensors/` | 板级与 PMW3901/VL53L1X 驱动 |
| `src/bench/` | 显式选择的诊断入口，非起飞入口 |

### 4.2 环境分组（43 个）

| 分组 | 环境 |
|---|---|
| 系留飞行（8） | `contest_tethered_flight_jp13`、`_pwm100`、`_debug_jp13_pwm100`、`contest_tethered_flow_debug_jp13`、`contest_tethered_flow_brake_trial_jp13`、`contest_imu_tof_low_hover_jp13`、`_jp14`、`contest_flow_assist_jp13` |
| 无输出联调（6） | `contest_hover`(默认)、`contest_aircraft1_closed_loop`、`_pwm_closed_loop`、`contest_aircraft1_standalone`、`contest_imu_check`、`contest_motor_preview` |
| 遥控/通信（13） | `contest_aircraft1_radio_v2_jp13`、`contest_radio_rx_probe`、`_rx_jp13`、`contest_radio_latency_*`(4)、`contest_jp13_uplink_echo`、`_loopback`、`_loopback_9600`、`_software_audit`、`_voltage_probe`、`contest_uart_dual_probe` |
| 电机台架（13） | `contest_motor_m1..m4`、`_m3_direct`、`_quad_direct`、`_quad_diag`、`_m1234_pwm15`、`_m1236_pwm15`、`_radio50`、`_radio50_usb`、`contest_motor_direction`、`contest_attitude_check_jp13` |
| 光流诊断（3） | `contest_flow_id_probe_jp13`、`contest_flow_spi_duplex_probe_jp13`、`contest_flow_calibration_jp13`(电机锁止) |

### 4.3 输出锁与安全规则

- 默认系留目标（`CL_TETHERED_FLIGHT` 且非 debug）→ `CL_FLIGHT_OUTPUT_LOCKED=1`，PWM 恒 0。
- 只有显式 debug/低空 profile 才解锁输出（`output_locked=0`），并有对应宏约束（互相组合错误直接 `#error`）。
- 上电默认 PWM 全零、不自动解锁；F2 起飞 / F3 受控降落 / F4 立即急停（F4 当降落键的候选已撤回）。
- 失联 300 ms、IMU/姿态异常、输出故障、高度异常均有故障停机路径；`CL_LAUNCH_TIMEOUT_SEC` 系留 3 s。
- 电机到板口映射以 `fly.md` 实测表为准：左前 M3/逆、右前 M6/顺、右后 M2/逆、左后 M1/顺。

### 4.4 当前默认开发目标（源码最新，未上机）

- 环境：`contest_imu_tof_low_hover_jp13`（继承 `contest_tethered_flight_debug_jp13_pwm100`）
- 特性：**12 cm 低空测试目标**、1000‰ 上限、输出解锁、光流关闭、`CL_VOFA_JUSTFLOAT`+`CL_VOFA_FIREWATER` 双格式遥测、`-Wl,-u,_printf_float`
- 运行期调参：日志串口接受 `S left_trim 0.020`（短命令）/`SET`/`GET`，脚本 `scripts/vofa_tune.py`、`scripts/tune.py`；`trims` 最终作用在 `closed_loop.c:23-24`
- 新增诊断：`src/bench/jp13_uplink_echo.c`（JP13/DAP 上行回环，不驱动电机）
- **状态**：源码与 `firmware/*_20261024/20261025.bin` 已构建，但 `docs/validation/` 没有 10-24～10-26 的烧录/实飞证据，`PROJECT_PLAN.md` 未记录 → 只能算"已构建"，板上真实版本必须现场读回确认。

### 4.5 已完成 / 未完成

| 已验收（有证据） | 未完成（不可宣称） |
|---|---|
| 主机回归（ASAN/UBSAN、PWM 寄存器替身、故障/失联/降落） | 稳定离地、进入 HOLD、6 s 悬停 |
| 13 环境构建 + 符号/标记断言（锁定/解锁隔离） | 横向保持（光流比例/旋转补偿未标定，`FLOW_CALIBRATED=0`） |
| 多次 SWD 烧录 + 逐字节读回 + 启动 marker | ToF 近地盲区（地面 7～18 mm < 40 mm 门限）导致定高环不介入 |
| 传感器识别与启动日志（IMU/ToF/PMW3901 id 0x73、反码 0xB6） | 姿态 rate 环抗扰（实测峰值 ±250 °/s，偏弱） |
| 遥控按键/心跳/序号去重（有限窗口实测） | 正常降落/失联降落/急停的整机端到端 |
| 静态拆桨差速方向（操作者口述确认） | 比赛地图、中心落点、高度 ≤1 m 全流程演练 |

### 4.6 证据与固件怎么查

- 证据：`docs/validation/<日期>-<主题>/`，最近三批是 `flight-live-2026-10-21`、`faster-landing-2026-10-21`、`roll-trim-2026-10-21`；`manifest.json` 里 `built / flashed / readback verified / boot verified / motor tested / tethered flight / competition accepted` 是不同档位，别混用。
- 固件：`firmware/` 是历史快照，`docs/firmware_sha256.json` 是 11 项清单；**当前 `firmware/contest_hover.bin` 等 7 个快照已落后源码**（见第 6 节），选型一律以"源码 commit + 环境名 + 读回 hash"为准。

## 5. 教学工程（`01_教学demo`）

- 17 个环境：`demo01_vofa`（原 `demo_vofa`）、`demo02_remote`/`_framed`/`_usb`、`demo03_motor`/`_bench`、`demo04_hover_logic`、`demo05_tof`、`demo06_flow`、`demo07_integration`，外加模板/LED/按键/I2C 扫描/自检/UART 探针。
- 小偏差：`check_demos.sh` 只构建 16 个环境，新增的 `demo02_remote_usb`（2026-10-25 加的 USB 文本调试变体）还没进检查清单。
- 讲义 `教学总讲义.md` 是 `scripts/build_handbook.py` 的**生成物**，改完源文档要重跑；`教学资料.zip` 由 `scripts/package_teaching.py` 打包。
- 纪律：模型/主机测试 ≠ 实机；第 3 课台架必须拆桨；第 7 课物理 PWM 恒 0。

## 6. 本机复验基线（2026-10-26 实测）

环境：Ubuntu 24.04 + 本机 pio/arm-none-eabi，未烧录、未开串口。

| 检查 | 结果 | 说明 |
|---|---|---|
| `02_比赛工程/scripts/check_docs.py` | ✅ 通过 | 256 条本地 Markdown 链接全部存在 |
| `02_比赛工程/scripts/check_closed_loop.sh` | ✅ 通过 | 全部主机回归 + 13 环境构建 + 符号/标记断言通过 |
| `02_比赛工程/scripts/check_contest.sh` | ⚠️ 主机+10 环境构建通过，**最后快照比对失败** | `cmp .pio/build/$env/firmware.bin firmware/$env.bin`：10 个里 7 个不一致（`contest_hover`、`contest_imu_check`、`contest_motor_preview`、`contest_motor_m1/m2/m3/m4`）；3 个 GPIO 诊断一致 |
| `01_教学demo/scripts/check_demos.sh` | ✅ 通过 | 16 个环境构建成功、主机逻辑/传感器替身/教学资产通过；`check_teaching_assets.py` 亦通过（6 个零 CSV、TLE100 HEX 578 字节） |

说明与处理：

1. `check_closed_loop.sh` 第 65 行原本依赖 `rg`（本机无），已改为 `grep -qF`（等价），备份在 `_pathfix_backup_20261026/`。
2. 快照不一致是**源码领先快照**（10-24/25 改了 `main.c`、`hal/Src/{common,tim,stm32f1xx_it}.c`、`sensors/flow.c` 等公共代码），不是代码回归。要恢复"快照=源码"的仓库纪律，需要把当前源码重新构建成基线快照：`pio run` 10 个环境 → 复制 `.pio/build/<env>/firmware.bin` 覆盖 `firmware/<env>.bin` → 重算 `docs/firmware_sha256.json`，并在 `PROJECT_PLAN.md` 记一笔。**这一步会改写证据类文件，需负责人确认后再做。**
3. `.vscode/c_cpp_properties.json`、`launch.json` 是 PlatformIO 自动生成的，本次已改成本机路径；下次构建会自动跟着刷新。

## 7. 已知坑（来自交接文档与本机实测）

1. 不要把 `archive/`、教学工程固件、`firmware/*.bin` 拿来直接烧；先核对环境、源码与 hash。
2. 不要把"主机测试通过 / 构建通过 / `state=3` / PWM 有输出 / 传感器识别"当成飞行验收。
3. `FLOW_CALIBRATED=0`、`CL_FLOW_AXES_VERIFIED=0` 时，光流数据不能当定点可靠；`present=1` 只代表识别。
4. PWM 达到 900‰/1000‰ 不代表推力或悬停油门已标定。
5. `link=0` + `state=0` + PWM=0 是"待机且无遥控链路"，不是"电机锁死故障"。
6. 改按键必须**发送端+接收端成对**改并验证；历史上 F3/F4 对调候选因发送调度优先级已撤回。
7. 桌面近地 ToF 5～39 mm 是盲区，不代表损坏，也不能当作有效高度；离地判据要看新鲜原始证据。
8. 不要用碰撞、绳子拉紧、触地后的角速度数据调横滚/偏航增益。
9. 本机没有 `rg`；新写的脚本别依赖它（或加 `grep -F` 回退）。
10. `02_比赛工程/propgen/` 是独立 git 仓库，`git status` 里会看到它，别误当成本工程改动提交。

## 8. 常用命令（本机可直接跑）

```bash
cd /home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程
export PATH="$HOME/.platformio/packages/toolchain-gccarmnoneeabi/bin:$HOME/.local/bin:$HOME/.platformio/penv/bin:$PATH"

# 主机回归 + 13 环境构建（不含烧录）
bash scripts/check_closed_loop.sh

# 基础检查（最后一步快照比对当前会失败，见第 6 节）
bash scripts/check_contest.sh

# 只构建低空系留目标（当前开发主线）
pio run -e contest_imu_tof_low_hover_jp13

# 文档链接自检
python3 scripts/check_docs.py

# 串口日志（工整中文滚动，自动存 logs/serial/）
python -u scripts/show_remote_keys.py --pretty

# 运行期调参（VOFA/日志串口）
python3 scripts/vofa_tune.py --port /dev/ttyACM0 set left_trim 0.020
```

烧录前必须：备份 Flash、断开动力或确认桨叶安全、记录探针 UID、把命令与读回 hash 写进新的 `docs/validation/` 目录。

## 9. 文档维护约定

- **不改写** `docs/validation/**`、`archive/**`、`fly.md` 的历史记录（可新增，不回改）。
- 每轮改动更新 `02_比赛工程/PROJECT_PLAN.md` 的进度/下一步/问题表。
- `教学总讲义.md` 改源后重跑 `python3 scripts/build_handbook.py`。
- `.vscode/*.json` 交给 PlatformIO 自动生成，不要手改。
- 本文件（索引）在**阶段切换**时更新，而不是每轮都改；细节留在 PROJECT_PLAN 与 validation。
