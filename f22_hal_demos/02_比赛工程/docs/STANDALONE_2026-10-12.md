# 单机闭环烧录与 USB 采集

用户要求先不接遥控，烧录单机版并查看数据。新增环境 `contest_aircraft1_standalone`，继承同一真实 PWM 控制器、M1/M2/M3/M6 映射、15% 名义推力和 20% 单路上限。光流轴向未验收，仍只监测，不参与横向反馈。

## 单机接口

- E49 M0/M1 均拉高到模式3，不修改无线模块保存参数，不烧录遥控器。
- 不启动 USART1 中断接收协议；板载 USB 115200 8N1 输出 `[CL LOCAL]` 文本，异步发送。`link=1` 是本地控制条件成立，不代表无线心跳有效；`packets=0`。
- 上电 PWM=0。KEY1 松开稳定 50ms 后的新按下稳定 30ms 才产生一次操作：IDLE 启动，运行中请求降落，FAULT 清故障但不启动。KEY2 立即停机。
- 按住 KEY1 上电不启动，长按不重复事件；KEY2 后一直按着 KEY1 不重启。KEY1 在单机版不再作为 SysTick 急停，KEY2 和 20ms 超时保护保留。

## 软件检查

四环境构建成功；六组主机合成输入/寄存器替身测试通过，包括单机按键、两种模式的 PWM 保护、控制反馈、任务协议与布局。ELF 确认单机版有真实 PWM 驱动，且未链接 `HAL_UART_Receive_IT`。主机测试不证明真实波形或恢复力矩。

证据：[software-check.log](validation/standalone-2026-10-12/software-check.log)。

## 真实板身份、写入及启动

当前仅一个 CH340，`/dev/ttyUSB0`，USB 路径 1-13。先通过只读 ROM 握手得到 STM32 Device ID 0x0414，并读取 UID 地址 0x1FFFF7E8 的12字节为 `30FFD90530574B3036751643`；这是先前记录的板B。USB端口历史位置曾不同，本轮以真实只读响应和 UID 定位，不按旧端口号猜测。

固件 `firmware/contest_aircraft1_standalone.bin`，37000 字节，SHA256 `2d2e177c8acc6ba3acb7d49f724d329168e557b738b34387b01861f655f0bba5`。已按用户授权烧录，逐块写入读回校验至100%，GO 0x08000000 成功；持续 USB 日志证明程序实际运行。

证据：[identify.log](validation/standalone-2026-10-12/identify.log)、[upload.log](validation/standalone-2026-10-12/upload.log)、[serial.log](validation/standalone-2026-10-12/serial.log)、[stats.json](validation/standalone-2026-10-12/stats.json)。

## 当前验收边界

16:14:20～16:14:35 观察到空闲状态、故障0、四路PWM0、输出保护未触发；IMU与光流持续有效。ToF 通信与读数在更新，距离过近，未得到有效距离。日志中的 dt_ms 是发送时的单次循环周期，不代表整个窗口的最大延迟或已验收200Hz。

本轮没有操作者 KEY1/KEY2 动作记录，没有实际电机闭环或升降目标验证。结论 PARTIAL：单机固件已烧录并运行，空闲零输出与传感器采集通过；真实闭环、有效测距及按键停止路径仍待现场动作对照。

## 首次启动停机后的诊断修订

用户随后报告供电问题已修好，按 KEY1 后电机转一下便停止。16:21:54～16:22:09 的后续串口已处于 `state=5 fault=2`、PWM0。此为首版输入条件故障锁存；日志没有触发瞬间快照，不能仅凭后来恢复的 `imu=1 armlevel=1` 定责。用户动作时间、原始 MPU 读取结果与瞬间高度未捕获。

代码审查发现确定的边界错误：`Height_Update` 允许原始 40mm 有效距离，倾斜投影后可能为39.x mm，但控制器又要求投影高度≥40mm，会误触发输入故障。修订将投影高度下限改为30mm以容纳允许倾角的投影；ToF驱动原始距离下限仍40mm，不把近地无效读数强制置为有效。新增真实 `Height_Update`→控制器的合成输入回归测试通过。

增加故障首次触发快照 `[FAULT]`，保存先前状态、使用的/原始IMU有效位、校准/收敛时间、原始加速度、角速度、投影高度、ToF原始距离/状态/年龄、输出保护与循环周期。快照保留到异步串口可发，避免周期遥测错过瞬间。故障码11区分IMU输入，12区分高度输入，2保留配置/横向输入条件。

修订版四环境编译、六组主机测试及输出隔离检查通过；见[fault-software-check.log](validation/standalone-2026-10-12/fault-software-check.log)。修订固件候选与烧录状态见[standalone_diagnostic_firmware.json](standalone_diagnostic_firmware.json)。该软件边界问题已证实；是否解释用户这一次停机，仍须重新按键捕获故障快照或正常运行数据。

用户确认动力电池拔掉、只接 USB 后，重新读取同一 UID，诊断修订已写入读回校验100%并GO运行。修订 BIN 37716字节，SHA256 `67d69e0c4d6588f0d1d65690a222439c5639f5e9f712ba4a205a0c9587177544`；见[诊断上传](validation/standalone-2026-10-12/diagnostic-upload.log)。

16:31:01～16:31:15 的首轮修订日志为 IDLE、fault0、PWM0，俯仰约-27.5°、armlevel0，尚无新启动事件。该姿态不符合启动条件，不能以此验证电机闭环。已提示先水平静置、待armlevel1再按KEY1，且继续保留启动高度、离地超时和姿态保护。用户报告供电问题已修复，本轮不据旧故障记录否定该报告；本次重烧前的动力断开状态由用户明确确认。

用户随后确认平放桌面、底部有铜柱，16:33:46～16:34:01 重新采集：roll约-2.22～-2.25°，pitch约-0.22～-0.25°，armlevel持续1，state0/fault0/PWM0。当前接近水平并满足启动姿态条件，先前-27.5°不代表这次平放姿态。小角度偏差可能来自机架、安装或加速度偏置，尚未据此做静态姿态置零。此次仍无启动事件。

## 16:35 首次捕获启动与停机原因

用户报告接电池开始测试，捕获 16:35:40～16:35:42 的 LAUNCH（state1）。四路 CCR 对应占空比由0.5%逐渐上升，最后 M1/M2/M3/M6 为13.5%/16.5%/13%/16%，均低于20%上限；两秒后输出全零并锁存fault7。

故障快照明确为 code7/from1，原始和实际使用的IMU有效位均1，calibrated1，armlevel1，ToF原始14mm/status9/fresh1、age18ms，height_valid0，output_trip0，dt6000µs。这次是2秒离地阶段未获得≥8cm有效高度的保护停机，不是本轮IMU通信、周期超时或输出驱动保护失败。完整关键帧：[first-start.log](validation/standalone-2026-10-12/first-start.log)。不能据此追溯首版fault2的触发原因。

后续16:36:20～16:37:20仍是fault7/PWM0，没有新的复位或启动动作。提示KEY2复位，模块下方留空、距桌面9～10cm且机身水平静止时重新KEY1启动，便于直接进入定高阶段。当前只证明非零PWM寄存器输出与离地失败清零行为；尚未验证大范围姿态方向和有效高度反馈，更不代表能实飞悬停。

## 用户指定的30秒观察：16:39:45～16:40:15

共148组周期遥测，加2组故障快照，数据见[test-30s-163945.log](validation/standalone-2026-10-12/test-30s-163945.log)和[统计](validation/standalone-2026-10-12/test-30s-163945.json)。本轮未更改或烧录代码。

- 开始时已在fault6，后被操作者清零；本轮未捕获该fault6首次触发快照，不推断其具体原始测距状态。
- 16:39:49 code8/fromIDLE拒绝启动：姿态接近水平、原始距离92mm、IMU有效，但角速度18.11/17.46/30.24°/s超过启动每轴10°/s限制。不能把code8仅解释为高度过高。
- 16:39:54 成功进入ASCEND，16:39:55进入HOLD，共77组有效运行周期记录。HOLD只表明目标高度已爬升到20cm，不能表明实测高度达到20cm。真实距离主要在55～117mm；多路需求达到20%上限。
- 正roll例：16:39:57 roll+5.65°，M1/M2/M3/M6=20/14/20/15%，右侧M1/M3较高；负roll例：16:39:59 roll-14.55°，四路14.5/20/15.5/20%，左侧M2/M6较高。正pitch+16.09°例四路14/16/20/20%，后侧增加；负pitch-11.68°例四路20/20/14.5/13.5%，前侧增加。显示计算与映射方向相符，仍不代替真实恢复力矩或自主稳定验证。
- 16:40:09 code5/fromHOLD停机：roll=-25.20°触及25°倾斜保护，IMU/ToF均有效、无输出看门狗触发，四路随后清零。
- 148组中的flow有效位全部0，dx/dy全部0；当前遥测不含quality或present，不能区分低质量和通信/初始化状态，更不能据此直接判模块坏。光流横向反馈未启用，定点未验证。

本轮实测推进到真实PWM对姿态变化作出差速响应、有效ToF接入、启动角速度和过倾保护触发。高度闭环确在计算，但未达到目标高度，不能宣布定高性能通过。PWM仅15%名义/20%上限台架参数，悬停油门、PID、光流、实际力矩和飞行仍未验收。

## 光流有效位说明与待烧录诊断

`flow` 是 `FlowSensor.valid`，与 `axes`（轴向标定/横向控制使能）独立。驱动要求初始化识别成功、SPI读取成功且原始SQUAL≥15；任何一个不满足均显示flow0。这不是模块损坏判定。原30秒日志不足以区分质量与通信故障，10月11日及本日早期flow1记录仍然有效。

用户追问后准备新增 `flow_present`（初始化/识别状态）、`squal`（当前读取质量）和 `flow_err`（当前驱动实例的失败计数，初始化重试会重置，不是全程累计故障）。present1且squal<15说明当前图像质量不足；present0说明初始化、SPI或ID复核未通过，仍须检查原因。控制行为未变。

两个物理环境已编译成功，诊断候选单独保存为 `firmware/contest_aircraft1_standalone_flow_diag.bin`，元数据见[standalone_flow_diag_firmware.json](standalone_flow_diag_firmware.json)。尚未烧录；当前电脑未枚举CH340或USB串口，无法取得新实机数据。板内最后验证写入仍是上一版37716字节带故障快照固件。

## USB恢复后的光流诊断实测

用户确认只接USB、动力电池已拔，重新只读确认同一UID后，37780字节光流诊断版已烧录校验100%并GO运行，SHA256 `6d9ec63f79c0710f0fada78ac962031a473e72de653f3d277f2412555f1d2742`。上段的未烧录/未枚举状态已由本次更新覆盖。元数据已更新，上传证据：[flow-diag-upload.log](validation/standalone-2026-10-12/flow-diag-upload.log)。

16:49:13～16:49:43 共149组诊断帧，149组全部flow=1、flow_present=1，SQUAL79～179（程序阈值15），当前驱动flow_err始终0；dx范围-11～21、dy范围-17～12，观察到运动计数变化。75组ToF有效，原始距离范围0～286mm；早期无效读数仍不当有效距离使用。四路PWM始终0，轴向标定开关仍0。

证据：[flow-diag-164913.log](validation/standalone-2026-10-12/flow-diag-164913.log)、[统计](validation/standalone-2026-10-12/flow-diag-164913.json)。结论：当前仅USB/电机未运行条件下，光流识别、质量与计数更新检查通过，不能据之前flow0认定烧坏。未捕获上一轮失效的present/SQUAL，根因仍未定位；需对比电池接入和电机运行时的数据，轴向及米制比例仍待标定。

## 用户报告接入动力电源后的比较与KEY1拒绝启动

16:52:51～16:52:58 收到40组周期帧及2组故障快照，全部flow1/present1，SQUAL97～141、flow_err0、PWM0。用户中途反馈KEY1无反应；程序实际收到了启动请求，但code8/fromIDLE拒绝：第一次最新原始距离147mm（超过120mm）且gyroZ15.54°/s，第二次原始122mm且gyroX/Y/Z约-11.21/-18.77/+15.96°/s，均不满足启动条件。日志：[battery-connected-165251.log](validation/standalone-2026-10-12/battery-connected-165251.log)。本次未获得完整30秒连续数据，不把40帧当完整窗口。

随后16:54:30～16:55:00 重新连续读取30秒，已回IDLE、fault0、PWM0，近水平且armlevel1，距离约1cm仍在近地盲区，未捕获新KEY1启动事件。证据：[key1-recheck-165430.log](validation/standalone-2026-10-12/key1-recheck-165430.log)和[统计](validation/standalone-2026-10-12/key1-recheck-165430.json)。已提示保持模块距桌面9～10cm、静止再按KEY1，未修改保护阈值。

当前接动力电源/电机未运行时光流仍有效；尚未在新质量诊断日志下获得电机运行时的对比，不宣称已排除电机干扰。

## 17:36串口恢复

用户反馈串口无输出后，电脑仍识别CH340和ttyUSB0；独占读取10秒、20秒均无数据。共享日志最后17:30:53之前已出现code11，IMU无效、ToF未识别，光流有效；这只提示共同I2C链路/供电需排查，不足以确定静默根因。未自动复位或重烧。

用户再次运行20秒采集后，17:36:32～17:36:52收到99组周期帧，全部state0/fault0、imu1、tof1、flow1/present1、flow_err0、四路PWM0。原始距离0～196mm，近地部分valid0、其余有valid1；姿态有明显摆动，不能以整段判启动水平。证据：[serial-recovery-173632.log](validation/standalone-2026-10-12/serial-recovery-173632.log)、[统计](validation/standalone-2026-10-12/serial-recovery-173632.json)。串口与IMU/ToF识别已恢复，此前静默及瞬时共同失效的根因仍未确定。

用户后续继续采集并按键，17:37:53首次故障快照code8/fromIDLE：原始距离165mm大于120mm，gyroX11.37°/s也大于10°/s，IMU与ToF均有效；四路输出0。见[start-rejected-173753.log](validation/standalone-2026-10-12/start-rejected-173753.log)。这是启动条件拒绝，与前述串口静默或IMU失效不同。

## 20:01串口复查

当前ttyUSB0枚举路径变为USB1-4，未进入ROM读取UID，不以端口名声称确认了原板身份；未烧录或复位。此前/tmp采集脚本已不存在，重新创建同样115200、DTR/RTS高电平的读取脚本，并加端口独占打开。

20:01:03～20:01:23收到99组周期帧和3组故障快照，见[serial-recheck-200103.log](validation/standalone-2026-10-12/serial-recheck-200103.log)、[统计](validation/standalone-2026-10-12/serial-recheck-200103.json)。IMU、ToF识别、光流有效位始终1；ToF原始6～101mm，近地部分valid0；SQUAL106～146、flow_err0。捕获了LAUNCH非零PWM，20:01:15 code7/fromLAUNCH停机，快照原始61mm、投影57.866mm、height_valid1、status9/fresh1、imu_raw/used1、output_trip0。对应2秒内未进入≥8cm有效高度阶段，不能解释为本次传感器通信失效。

20:01:21、20:01:22再次启动被code8拒绝：距离54/58mm满足起始上限，姿态检查通过，但角速度分别X28.93/Y-12.16°/s及X16.28/Z14.91°/s超过每轴10°/s。最后state5/fault8、四路PWM0。本轮当前采集有效，不据此声称此前IMU/ToF同时失效的根因已修复。

## 用户报告需要留空30cm后的ToF复查

17:02:38～17:02:58 重新读取20秒，周期帧持续 state5/fault6、tof1/valid0/mm0、四路PWM0；光流持续flow1/present1、flow_err0。俯仰约+18.8°，armlevel0。见[tof-recheck-170238.log](validation/standalone-2026-10-12/tof-recheck-170238.log)和[统计](validation/standalone-2026-10-12/tof-recheck-170238.json)。本窗口未捕获首次fault6快照，不判断故障最初发生在哪个距离或动作。

当前ToF初始化识别状态为1，但没有有效测距；mm0不能解释为真实零距离。用户估计的30cm尚未与模块测距窗口到目标面的距离核对。控制器有效原始距离超过12cm会拒绝启动，因此30cm放置并不能作为正确启动的依据，也不能仅凭本窗口确认模块损坏。下一步先KEY2停止/清故障，保持电机关闭，无遮挡窗口面对平整白纸目标，比较窗口到目标10/20/30cm时的有效位及毫米读数，再决定安装、驱动或电气排查。
