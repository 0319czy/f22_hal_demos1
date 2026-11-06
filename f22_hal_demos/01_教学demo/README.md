> 此目录集中保存教学源码、手册、CSV、测试与遥控固件。单份讲义见[教学总讲义](教学总讲义.md)。比赛代码另在`02_比赛工程`目录。

# UAV-F22 悬停组队内教学工程

面向现有 F22、板载 MPU6050/SPL06、TLE100 遥控器和已购买的正点原子 **ATK-PMW3901 4m（集成 VL53L1X ToF）**。每课有独立入口、构建环境、操作说明和实验，串口数值课有独立 CSV 全零模板。硬件未到的部分已写代码和教学文档，实机验收单独记录。

| 课 | 内容 | PlatformIO 环境 | 教程 / CSV |
|---|---|---|---|
| 1 | VOFA、IMU、气压、姿态 Cube | `demo01_vofa` | [第1课](src/demo/demo_vofa/README.md) / [CSV](src/demo/demo_vofa/vofa.csv) |
| 2 | 原厂遥控字符、新心跳任务协议 | `demo02_remote` / `demo02_remote_framed` | [第2课](src/demo/demo02_remote/README.md)，UART4文本日志 |
| 3 | 单电机门控与拆桨台架 | `demo03_motor` / `demo03_motor_bench` | [第3课](src/demo/demo03_motor/README.md) / [CSV](src/demo/demo03_motor/vofa.csv) |
| 4 | PID、混控、起飞/保持/降落软件模型 | `demo04_hover_logic` | [第4课](src/demo/demo04_hover_logic/README.md) / [CSV](src/demo/demo04_hover_logic/vofa.csv) |
| 5 | VL53L1X 测距、状态/时效与高度差分 | `demo05_tof` | [第5课](src/demo/demo05_tof/README.md) / [CSV](src/demo/demo05_tof/vofa.csv) |
| 6 | PMW3901光流计数与质量 | `demo06_flow` | [第6课](src/demo/demo06_flow/README.md) / [CSV](src/demo/demo06_flow/vofa.csv) |
| 7 | 真实传感器、估计器与悬停控制器联调 | `demo07_integration` | [第7课](src/demo/demo07_integration/README.md) / [CSV](src/demo/demo07_integration/vofa.csv) |

另有 [TLE100发射端源码及已编译HEX](remote/tle100/README.md)。配套 HID 烧录器现已通过遥控子项目的 PIO 自定义上传完成写入和校验；它有独立的 AT89S52 环境，不能套用 F22 的串口下载参数。新协议两端必须同时替换。

**先读 [教师授课安排](docs/DEMO_COURSE.md)**，再看 [比赛规则映射](docs/COMPETITION.md)、[手册梳理与板载现状](docs/HARDWARE_OVERVIEW.md)、[模块接线](docs/MODULE_WIRING.md)。课堂记录使用 [实验记录表](docs/LAB_RECORD.md)。

第3课默认不启动PWM，台架环境仅启动编译时选中的一只电机，必须拆桨。第4课是理想软件模型；第7课读取真实传感器并预览控制器，实际PWM恒为零。目标0.5m、悬停超过5秒后等待一键降落，不能将模型曲线当作真机飞行验证。

## 使用

在`01_教学demo`目录只编译一课：

```bash
pio run -e demo01_vofa
```

按该课教程连接设备、关闭占用串口的软件，再烧录：

```bash
pio run -e demo01_vofa -t upload
```

一次烧录替换板上原固件。`demo_vofa`旧名称保持可用；板载只读诊断使用`demo_selftest`，默认只探测 EEPROM 地址、不改写内容，也不编译电机 PWM 测试。电机请用`demo03_motor_bench`逐路验证并拆桨。除此七课外，工程还提供模板、LED、I2C扫描、只读综合自检、VOFA与串口探针示例；当前验收脚本会构建全部16个有效环境。

```bash
bash scripts/check_demos.sh
bash scripts/build_remote.sh
python3 scripts/check_teaching_assets.py
```

这些命令只测试和编译，不烧录。16个F22环境、主机逻辑与传感器替身测试已有构建记录；详见 [验证记录](docs/LESSON_VALIDATION.md)。2026-10-09 当前板已完成安全自检及 VOFA 实机采样，具体范围、警告和启动问题见 [当前板测试记录](docs/BOARD_CHECK_2026-10-09.md)。无线两端联调、测距、光流、动力与飞行仍待实机验收。

项目约定与后续实飞任务见 [上下文](PROJECT_CONTEXT.md)、[架构](ARCHITECTURE.md)、[路线图](ROADMAP.md)。原采集文件在`data/vofa+.csv`，原根目录保留兼容链接，不用全零模板覆盖。

## 单文件讲义与资料包

[教学总讲义.md](教学总讲义.md) 汇总18份授课/实验/协议/接线文档；`firmware/`保存16个有效教学环境的BIN快照。向队员发旁边根目录的`教学资料.zip`，解压即可得到本教学工程、CSV、手册与遥控HEX。

重新生成讲义与包：

```bash
python3 scripts/build_handbook.py
bash scripts/check_demos.sh
python3 scripts/package_teaching.py
```

编译命令需在这个教学项目目录中运行；没有源码的旧环境仍不列入教学验收。比赛项目使用独立的配置/源码，修改教学课不会直接修改比赛代码。
