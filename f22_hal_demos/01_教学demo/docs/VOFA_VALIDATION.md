# VOFA+ 遥测验证记录（2026-10-02）

> 下方是2026-10-02不同固件阶段的历史记录，含14/19通道结果。当前第1课为20通道，CH19是滤波相对气压高度；此次教学构建及未实机验证项见[教学验证记录](LESSON_VALIDATION.md)，当前通道表见[VOFA](VOFA.md)。

状态：**PARTIAL**。固件编译、板端真实串口帧和传感器读取已通过；VOFA+ 界面曲线待用户确认。

当前固件已增加融合姿态及 CH14~CH18 的 yaw/四元数输出，19 通道版本编译并烧录成功，
stm32flash 报告完整写入 21772 字节并执行启动。旧 14 通道版本此前实机收过
190 帧且 IMU/气压计状态均为 1。新 19 通道烧录后，独立读取工具因串口控制线切换
没有收到帧；仍待用户在 VOFA+ 重连 DTR=1/RTS=0 并按板上 RESET 后确认 quaternion 通道。

## 最新实机结果（优先于下方历史排查记录）

直接对照 PIO 监视器：DTR=1/RTS=1 监听 12 秒无字节；DTR=1/RTS=0
立即收到连续 JustFloat 帧。此前从烧录退出序列推导 monitor_rts=1 的结论不成立，
配置和使用说明已恢复为 monitor_rts=0，monitor_dtr=1。

按 PIO 的先打开串口再设置控制线顺序，Python 实际读取 4 秒得到 **190 帧**，
全部 ImuOK=1、BaroOK=1。最后一帧：加速度 (0.01660,-0.02075,0.99316) g，
角速度 (0.0458,0.3664,-0.2214) °/s，IMU 温度 30.106°C，气压 1006.288 hPa，
气压计温度 29.829°C，帧间隔 21 ms。用户仅 USB 供电，这次实测传感器正常；
不需要仅因之前失败标志而推断必须增加电池。

最小诊断固件已恢复为 demo_vofa，监听进程已结束并释放串口。
固件含向量表显式设置及两传感器同时失败时的 I2C2 外设重置重试；
不能用最终成功单独证明这两项是此前无数据的根因。

## 后续排查更新

用户报告 VOFA+ RawData 也无字节。释放 VOFA+ 后，四种 DTR/RTS 组合均未立即
收到输出；补齐遥测入口 VTOR=FLASH_BASE 及中断启用后重新烧录。
临时最小 HSI/寄存器 USART1 诊断固件写入读回校验通过但仍未立即收到输出，
随后已恢复 demo_vofa。用户手动 RESET 后报告 LED2 闪烁，重新读取实际收到
22 个 14 通道 JustFloat 帧，帧中 ImuOK=0、BaroOK=0、IntervalMs=21。
这证明曾收到板端真实帧，尚未证明连续接收稳定或传感器工作正常。
50 秒监听曾出现 pyserial 的断开/多访问错误；后续短时间重开又收到 0 字节。

本地 stm32flash 与 pyserial 源码确认 RTS/DTR 的软件断言状态对应；ISP 退出
`rts,-dtr,dtr` 最终为 RTS=True、DTR=True。工程 monitor_rts 和使用说明已从
0 更正为 1，不能继续沿用之前 RTS=0 的建议。

用户确认目前仅 USB 供电。尚未实测板载 3.3V，不能断定 USB 是传感器失败原因。
最新版本在两只传感器都失败时，每秒重置 I2C2 外设状态并重试初始化；
已烧录成功，Flash 15864 bytes，二进制 16356 bytes。

## 范围与基线

本次链路是 MPU6050 / SPL06 → I2C2 → STM32 → USART1 / CH340 → VOFA+。
没有新增网络服务、SSH 或远程主机依赖，ASD-Host / lzr-host 不参与此功能。
工程使用 HAL + PlatformIO，沿用已有串口和 I2C2 驱动及 ISP 配置。
当前目录没有可读取的 Git 仓库元数据，`git status` 报 not a git repository；
保留现有示例，仅新增遥测入口并扩展传感器接口。

改动：`platformio.ini`、`src/demo/demo_vofa/main.c`、
`src/hal/Inc/sensors.h`、`src/hal/Src/sensors.c` 和文档。

## 已验证

| 工作 | 证据 |
|---|---|
| 原工程基线 | `pio run -e demo_selftest`，退出码 0，Flash 28408 bytes |
| 兼容自检编译 | `pio run -e demo_vofa -e demo_selftest`，两个环境 SUCCESS；自检 Flash 28704 bytes |
| 最终遥测编译 | `pio run -e demo_vofa`，退出码 0，Flash 15404 bytes，静态 RAM 572 bytes |
| 补偿公式来源 | 本地厂家 SPL06_001.c 与系数解码、缩放、计算表达式逐项核对 |
| 驱动主机测试 | 实际 sensors.c 用桩 I2C 编译，穷举 signed12/16/20、10000 组系数解码、负原始码补偿、读取失败、全部 8 个初始化通信失败位置通过 |
| 协议依据 | VOFA+ 官方 JustFloat 文档，小端 float32 + 00 00 80 7F |
| 遥测主机测试 | 当前此记录对应旧 14 通道版本；新 19 通道姿态融合版本待编译和实机验证 |

主机桩测试仅验证代码计算及失败路径，不代表真实传感器或硬件通信通过。
临时测试位于 `/tmp/f22_baro_test`，启用 ASan / UBSan。
根代理复跑 `ASAN_OPTIONS=detect_leaks=0 /tmp/f22_baro_test/test` 退出码 0；
当前工具环境的 ptrace 导致 LeakSanitizer 无法运行，因此只关闭泄漏检测。
遥测临时测试位于 `/tmp/f22_vofa_test`，同样启用 ASan / UBSan；
根代理复跑 `ASAN_OPTIONS=detect_leaks=0 /tmp/f22_vofa_test/test` 通过。
编译时 PlatformIO 需要写工作区外的共享缓存，按权限规则在获准后沙箱外运行。

资源观察：可用内存约 8.6 GiB，swap 使用 0，工作区所在盘剩余约 6 GiB。
没有进行安装新固件、启动电机或写 EEPROM 的操作。

## 实机验收待办

此前本机没有 `/dev/ttyUSB*`；本次用户要求烧录后，在沙箱外检测到 CH340
`/dev/ttyUSB0`（VID 1a86），执行 `timeout 90s pio run -e demo_vofa -t upload`。
首次进入下载模式超时；第二次退出码 0，15896 字节写到 100%，
stm32flash 报告启动地址 0x08000000，退出下载模式 GPIO 序列执行成功。

烧录后使用 `/tmp/f22_vofa_capture.py` 读取串口，每次约 4 秒，三次均收到 0 字节。
尝试了文档运行态 DTR/RTS 和与下载退出序列对应的信号设置及复位脉冲，
尚未确认无输出原因。**烧录通过，运行数据流仍未验证通过**；未打开 VOFA+。
后续板上验收：

1. `pio run -e demo_vofa -t upload` 烧录。
2. 按 [VOFA.md](VOFA.md) 设置串口及通道。
3. 静置确认 ImuOK / BaroOK=1、IntervalMs≈20、加速度模长≈1g。
4. 缓慢倾斜和转动，确认对应加速度、倾角、角速度曲线变化。
5. 核对气压与温度量级，确认重连 CH340 后能够重新接收数据。

现有硬件问题（CH340 间歇掉线、供电疑点和板 3 磁力计故障）仍按 HANDOFF.md 记录。
