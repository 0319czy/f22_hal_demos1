# 遥控F1启动、四路固定50%拆桨台架

环境 `contest_motor_radio50`，入口 `src/bench/motor_radio50.c`，固件 `firmware/contest_motor_radio50.bin`。本版满足用户先持续转动、固定50%、遥控第一功能键启动的台架要求，不运行IMU/ToF/光流或飞行闭环。原闭环固件及15%本地按键版保留。

## 按键与停止

- 上电四路PWM0。TLE100按手册F1（用户称P1，P0.6，协议命令4）在已有有效心跳后启动，M1/M2/M3/M6均固定50%。重复3包不会重复切换，运行时间不限。F2在本版没有启动动作。
- F3（命令2）或F4（命令3）立即停机；板载KEY1/KEY2均急停。停止与启动同批到达时优先停止。
- 300ms未收到新有效报文停机；重新收到心跳保持停止，须之后新的F1请求才能启动。停止后残留重复F1包被抑制到新心跳到来。
- 串口错误/队列溢出、输出非法或主循环20ms未刷新输出均清PWM并锁存；F3/F4或本地急停可清除输出故障。急停后不自动重新启动。
- 只在此环境将PWM驱动上限设为50%，定时器PSC23/ARR199/CCR100，设计15kHz。原闭环默认上限20%未放宽。波形与实际推力未经测量。

## 串口与接线

E49置模式0，沿用已保存的信道/地址参数，USART1只接收9600，不发送调试文本到无线。发射端固件不改动；接收端在本版把原F1校准命令解释为启动。

独立JP13 USART2诊断115200、8N1：JP13引脚1=5V，2=TXD2/PA2，3=RXD2/PA3，4=GND，依据F22手册3.19与原理图。接飞控2脚TX到USB-TTL的RX，4脚GND共地，USB-TTL的TX及电源脚留空。脚号以PCB1脚标记为准，不凭照片方向推定顺序。已枚举的Horco CDC是 `/dev/ttyACM0`，重新插拔后端口名可能变化；板载CH340仅用于下载。

读取：

```bash
/home/czy/.platformio/penv/bin/python scripts/capture_motor_serial.py /dev/ttyACM0 30
```

日志前缀 `[MOTOR50 RADIO]`，字段：`running`、`link`、`age_ms`、有效包累计`packets`、解析拒绝计数`reject`、相邻有效包序号缺口累计`seq_missing`、本次/历史最大包间隔`gap_ms/max_gap_ms`、UART错误累计`uart_err`、最后协议命令`cmd`、F1包数`p1_requests`、实际启动次数`starts`、运行中断链停机次数`disconnects`、输出锁存`trip`、四路CCR占空比千分数。停止原因为0初始/启动、1本地急停、2遥控停止、3链路超时、4UART错误、5输出保护。`reject`含滑动窗口解析尝试，不是丢包率；`seq_missing`仅在有效链路内计缺口，不能直接判定射频器件故障。`max_gap_ms`包含断链间隔，不能解释为单次按键延迟；当前没有发射时间戳或ACK来量端到端时延。

运行应为 `running=1 link=1 trip=0 PWM_permille_M1_M2_M3_M6=500,500,500,500`。接电池测试前先拆桨；当前是固定PWM台架，不能据此进行自主飞行。

## 验证与固件

主机合成输入测试覆盖F1映射、重复包、60秒持续需求、停止优先级、300ms断链停机、重连不启动、UART故障锁存、过期命令、序号/时间回绕和CRC；PWM寄存器替身检查50%上限/CCR100、未选电机输出隔离、看门狗及两本地急停。实际无线/按键和50%电机运转分别记录，不把合成测试当实物验证。

固件12756字节，SHA256 `f6aa054c40bb7987b9c0ab442ad1fd03059d1479e38880b0326a75565ff24777`，同UID `30ffd90530574b3036751643` 写入读回100%并GO运行。见[元数据](motor_radio50_firmware.json)、[上传](validation/standalone-2026-10-12/motor-radio50-upload.log)、[构建](validation/standalone-2026-10-12/motor-radio50-build.log)、[回归](validation/standalone-2026-10-12/motor-radio50-regression.log)。已从JP13读到真实 `[MOTOR50 RADIO]` 停止日志，首轮link0/packets0，尚不能宣布无线已连接或延迟/掉线问题修复。
