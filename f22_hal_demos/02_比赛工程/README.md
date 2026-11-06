# F22 悬停比赛工程

唯一开发目录：`/home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程`。先看[比赛计划与当前进度](PROJECT_PLAN.md)。

**当前工作：**[IMU、ToF、光流协同方案](docs/CONTROL_DESIGN_2026-10-16.md)的第一片代码已接入系留控制源码：飞行状态下用IMU做最多100ms的短时高度预测，新鲜有效ToF校正高度/竖直速度；40mm以下仍是有限起转斜坡，首个有效测距后高度反馈开始修正。当前状态机还把8cm作为默认的起飞阶段交接判据；已知地面测距时判据为地面距离再加4cm。8cm也是光流的工作距离门限，但不再是高度反馈的启动条件。待命/故障时不沿用旧惯性高度，启动高度守卫读取当前原始ToF。主机合成输入检查和锁止构建通过；真实传感器轨迹回放与硬件验证尚未完成，不能作为飞行版本。气压计不使用。

**版本状态：**板上最后确认的是旧100%无光流试验版，F2起飞/F3降落/F4立即停机；本轮没有访问硬件，不能据此确认现在板上版本或供电。机架断裂后，接收端单独换F3/F4的缓降候选已撤回。当前源码的硬件系留配置默认电机锁止，旧板上固件不会自动获得该保护。见[事故复盘](docs/FRAME_INCIDENT_2026-10-16.md)。

## 当前使用的两端代码

| 端 | 状态 | 入口/配置 |
|---|---|---|
| 飞控源码 | 硬件系留配置事故锁止；旧控制逻辑留作审查 | [控制器](src/flight/closed_loop.c)、[输出许可](src/flight/closed_loop_profile.h) |
| 飞控最后确认的板上映像 | 旧100%无光流，F4立即停机；不作为可飞版本 | [事故证据](docs/validation/frame-incident-2026-10-16/manifest.json) |
| 遥控 | tle100_latency_fast；未成对修改新按键语义 | [入口](remote/tle100/latency_main.c) |
| 主机显示 | JP13→无线DAP→电脑，115200 | [显示脚本](scripts/show_remote_keys.py) |

当前锁止产物为`firmware/contest_incident_review_locked.bin`，未烧录。本轮构建的`contest_flow_id_probe_jp13`也继承硬件输出锁，仅用于代码编译检查，未烧录。正常降落/独立急停新映射需成对验证，不能把已撤回候选当作下一步下载目标。光流原始读取正常不等于速度比例/旋转补偿或横向闭环已经验收。

## 构建与验证

```bash
cd /home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程
/home/czy/.platformio/penv/bin/pio run -e contest_tethered_flight_jp13_pwm100
bash scripts/check_closed_loop.sh
python3 scripts/check_docs.py
```

当前上述系留环境生成锁止硬件镜像。这些命令不会烧录。历史BIN可能仍有电机输出，不能以文件名中的100%或日期判断是锁止版。

## 操作者看串口

工整的中文滚动日志，按时间分组显示状态、高度姿态、四路PWM、光流；按键与故障即时打印，可往上翻看（Ctrl+C退出）：

```bash
/home/czy/.platformio/penv/bin/python -u /home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程/scripts/show_remote_keys.py --pretty
```

`--pretty`自动保存`logs/serial/日期时间.pretty.log`和完整的`.raw.log`，启动时打印文件路径；默认状态每0.5秒一组，`--interval 0`显示每条状态，原始数据始终完整保存。只看按键用`--keys-only`，看原始日志用`--full`（替换`--pretty`）。主机DAP唯一ID为814dc35a，脚本默认用它的by-id路径；避免ttyACM编号变化。一个串口只开一个监视器。

## 目录与资料

| 目录/文档 | 用途 |
|---|---|
| [PROJECT_PLAN.md](PROJECT_PLAN.md) | 比赛要求、实施计划、进度、问题记录 |
| [docs/COMPETITION.md](docs/COMPETITION.md) | 规则出处及实现边界 |
| [docs/MODULE_WIRING.md](docs/MODULE_WIRING.md) | 模块接线；出现疑问查原理图 |
| [docs/MOTOR_MAPPING.md](docs/MOTOR_MAPPING.md) | 当前机头、电机位置和俯视旋向 |
| `src/flight/` | 状态机、PID、观测、混控、遥控协议 |
| `src/hal/`、`src/sensors/` | 板级接口和传感器驱动 |
| `src/bench/` | 显式选用的诊断例程，非正式起飞入口 |
| `remote/tle100/` | 遥控器源码及配置 |
| `scripts/`、`tests/` | 工具与主机验证 |
| [firmware/README.md](firmware/README.md) | 当前固件与历史BIN说明 |
| `docs/references/`、`docs/validation/` | 手册/规则与原始证据 |
| [archive/README.md](archive/README.md) | 历史文件、迁移备份，不作为当前状态 |

Downloads里的旧工程路径已改为指向此工程的软链接，旧命令仍能找到同一套文件。旧笔记中的“当前/待确认”属于对应日期的历史状态，最新判断以PROJECT_PLAN和当前源码为准。
