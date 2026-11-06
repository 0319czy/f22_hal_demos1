# 比赛工程验证记录

日期：2026-10-03；范围为本目录独立构建与主机任务桥接测试。未进行烧录、遥控实机、测距/光流或飞行验收。

- `bash scripts/check_contest.sh`：C11 主机测试开启 AddressSanitizer 与 UndefinedBehaviorSanitizer；覆盖事件与心跳分离、STOP/LAND/START 优先级、事件过期、序号/时钟回绕、空闲校准限制、无线命令到控制器状态门控。PASS。
- `vofa.csv`：32 列，32 个初始值全部为 0。PASS。
- `python3 scripts/check_docs.py`：本项目内所有 Markdown 相对链接存在。PASS。
- `pio run -e contest_hover`：STM32F103RCT6 / ststm32 19.6.0 / stm32cube F1 1.8.6；Flash 32168 bytes、RAM 1904 bytes。PASS（最终重建结果见构建记录）。
- `bash scripts/build_remote.sh`：SDCC mcs51 编译并生成 `remote/tle100/tle100.hex`。PASS。

主机测试只使用限定数学输入，没有模拟设备替代真实无线验收。飞控链接产物不含电机 PWM 启动/占空比应用调用；实际输出启用通道恒 0。未装的新模块、未到的遥控器烧录器、安装坐标和光流补偿、电机旋向映射/供电、PID 与故障降落均标记待验收，不以编译成功代替真机证据。

最终完整命令输出见 [build_validation.log](build_validation.log)；固件校验见 [firmware_sha256.json](firmware_sha256.json)。

## 2026-10-07 回归

重新运行`bash scripts/check_contest.sh`：主机任务桥接测试、32列全零CSV、Markdown链接检查及`contest_hover`固件构建全部通过。验收脚本新增两项持续门禁：最终ELF不得包含`Board_MotorPwm`/`HAL_TIM_PWM_Start`，构建出的BIN必须与仓库中的`firmware/contest_hover.bin`逐字节一致，并通过`docs/firmware_sha256.json`校验。完整输出见 [2026-10-07回归日志](build_validation_2026-10-07.log)。当前结果仍只是代码/构建验证，没有烧录或实机传感器、无线、飞行验证。

使用SDCC 4.5.0重建比赛目录的TLE100发射端；其HEX与教学工程重建结果逐字节一致，且匹配本目录固件SHA256清单。构建记录见 [遥控固件重建日志](remote-build-2026-10-07.log)。

## 2026-10-10 比赛程序校准修复

[换板复测](BENCH_2026-10-10.md)中，板A未经校准的gyroX约3.4°/s，而校准后30秒均值约0.008°/s。旧比赛入口逐样本要求原始gyro绝对值低于2°/s，会拒绝这块板。改为100样本稳定窗口后原子提交零偏，保留加速度模长和异常gyro上限，并增加各轴标准差/峰峰值检查。非有限、采样不足、运动或突跳时失败，不覆盖旧bias。

`bash scripts/check_contest.sh`通过：任务桥接回归、稳定偏置/运动/独立标准差与峰峰值门限/失败重试测试、实测校准后噪声加注3.4°/s偏置的主机回放、32列全零CSV、Markdown链接、固件构建、无电机PWM符号、BIN快照与SHA256核对。日志见 [校准软件检查](validation/bench-2026-10-10/calibration-software-check.log)。Flash32676 bytes、RAM1904 bytes；更新了固件快照与校验清单。

回放使用已扣偏的记录另加测试偏置，不是100Hz/200Hz真实原始校准窗口，更不是新增校准程序的板上验收。新比赛固件本轮未烧录，板上仍为此前VOFA遥测。恒定缓慢绕重力轴旋转无法仅由IMU统计辨别，必须人为固定静止；10°/s异常上限不是允许移动校准的标准。安装矩阵、动力、实际失联停机、姿态闭环、光流/ToF和悬停仍未验收。

## 随后独立校准入口的实板验证

增加 `contest_imu_check`，仅构建IMU读取和同一校准算法，不依赖光流/ToF，不含PWM。16通道全零模板为 `imu_check.csv`，接板载USB 115200 / JustFloat；与比赛主入口的32通道UART4模板不同，详见 [IMU检查](IMU_CHECK.md)。

当前连续USB连接的板写入并100%校验，ROM GO启动。启动后20秒观察窗口内916帧全部imu_ok=1，完整校准100样本、read_errors=0、cal_failures=0。bias X/Y/Z为-3.40924/+0.72595/+0.44916°/s，扣偏后均值-0.00755/-0.00232/+0.00955°/s，加速度模长均值1.03362g。证明新算法在该次静止条件下可校准此板的原始零偏；没有验证运动拒绝、重连或失效恢复的实机路径。

日志：[烧录校验](validation/bench-2026-10-10/contest-imu-upload.log)、[原始采样](validation/bench-2026-10-10/contest-imu-samples.csv)、[统计](validation/bench-2026-10-10/contest-imu-stats.json)。`check_contest.sh`增加此环境的构建、16列模板、无PWM符号、BIN快照与SHA256核对，均通过；[软件检查](validation/bench-2026-10-10/contest-imu-software-check.log)。

当前板上是 `contest_imu_check`，并非 `contest_hover`；主比赛任务入口本轮仍未烧录。没有启动电机、姿态闭环或实飞。

## 单电机台架准备及M1下载

按F22 v2.0原理图重新确认M1～M4/M6/M7为N沟道低侧MOS驱动，改为高有效PWM、CCR0停止和未选GPIO低；旧低有效配置不能用于本板电机。仅目标脚切AF，其他旋翼保持GPIO低。增加门控主机边界测试，覆盖按住上电、释放重触发、STOP锁存、299/300ms失联、迟到重连、3秒限时、计数绕回和接收故障。四环境构建、三套CSV、PWM符号隔离、固件快照与哈希检查通过，见[软件检查](validation/bench-2026-10-10/motor-bench-software-check.log)。

用户确认动力电源3.7V、此前确认全拆桨。M1专用固件已通过板载USB写入并逐段100%校验，ROM GO三次ACK为79，见[下载](validation/bench-2026-10-10/motor-m1-upload.log)、[启动命令](validation/bench-2026-10-10/motor-m1-start.log)。当前下载固件为contest_motor_m1；ACK仅证明引导命令被接受，实际应用活动、M1位置、停止电平、无线停止及看门狗恢复尚待观察，不宣称电机测试通过。USB与3.7V主动力同时使用的接法见[MOTOR_CHECK](MOTOR_CHECK.md)：控制主开关关闭，USB供控制、CON+/CON−供电机。

旧教学电机固件/ZIP仍含不同的PWM极性，不可用于此次实板电机测试；本轮未重建教学包。

M2/M3/M4独立环境与对应BIN已构建，每个环境使用相同门控和对应单目标引脚；七环境构建、主机回归、PWM隔离、快照与校验全通过，见[后续逐路软件检查](validation/bench-2026-10-10/motor-all-software-check.log)。三路未下载，板上仍保持M1；实际映射与各停止结果记录在[MOTOR_MAPPING](MOTOR_MAPPING.md)，未知项未填写假定值。

## 随后用户报告烧损

用户报告接入电池、连接遥控后“A1和ams11175.0烧了”。已要求完全断电，停止上电及电机测试；此前同时供电指导未经实物验收，已撤回。故障部件定位、接线和原因待确认，见[供电故障记录](POWER_INCIDENT_2026-10-10.md)。之前软件构建通过仍不构成硬件安全验收。

用户报告板子已修复并要求M3下载。已写入contest_motor_m3并100%逐段校验，ROM GO三个ACK=79；[M3下载记录](validation/bench-2026-10-10/motor-m3-upload.log)、[启动命令](validation/bench-2026-10-10/motor-m3-start.log)。仅确认写入与引导命令，电机位置/旋向/停止及修复后供电尚未实测。

用户报告电机不转，明确要求上电即转作电路通断检查。新增隔离contest_motor_m3_direct：M3 PC8 GPIO持续HIGH，其他旋翼LOW，无无线/长按启动/低占空比/定时关闭，KEY2锁存停止；LED供电控制保持关闭。默认contest_hover未变。8环境构建、原有主机测试、模板与链接、快照/哈希通过，见[直通版软件检查](validation/bench-2026-10-10/motor-m3-direct-software-check.log)。尝试下载时/dev/ttyUSB0再次消失，stm32flash打开端口失败，没有写入本次新版，见[失败记录](validation/bench-2026-10-10/motor-m3-direct-upload-attempt.log)。上一成功下载仍为带门控的contest_motor_m3。直通版存在GPIO实际动力输出，不能因没有PWM符号视为无动力环境。

## 四电机启动后延时2秒直通版下载

用户明确要求四电机同时在启动后2秒运行。新增独立contest_motor_quad_direct环境：初始化后等待2000ms，GPIOC PC6～PC9一次写HIGH使M1～M4持续全导通，M6/M7保持LOW；无无线/KEY1/15%限幅/3秒截止/LED供电。等待期间KEY2取消启动，运行时KEY2锁存四路LOW；默认contest_hover保持无动力。9环境及既有主机测试、模板、链接、GPIO入口无RF/PWM/UART初始化符号、BIN快照/清单全部通过，见[软件检查](validation/bench-2026-10-10/motor-quad-direct-software-check.log)。

USB重新可用后完成写入、逐段100%校验与ROM GO三个79 ACK，随后RTS置False选择正常启动。BIN SHA256：4bc9f80541de6a5dd7eaf0d09625b247b94a61d9fd3e2c4d428f822a214bbf21。见[下载](validation/bench-2026-10-10/motor-quad-direct-upload.log)、[启动命令](validation/bench-2026-10-10/motor-quad-direct-start.log)。目前下载程序为contest_motor_quad_direct。没有从本程序读取遥测，尚无四电机实际转动、电压/电流、KEY2和修复后供电的实物观察，不以GO ACK当作运行证明。延时起点是初始化完成，而非后续接入电池；控制先启动、动力后接会立即转动。

四路直通版下载后用户报告M4电机不转。目前没有交叉换口、M4端电压或PC9电平测量，尚不能定位为电机损坏或驱动损坏。JP13按手册3.19为5V TTL USART2（1=5V、2=TXD2/PA2、3=RXD2/PA3、4=GND），当前直通版没有启用串口。

## M4零电压与串口诊断准备

用户反馈“线路上通的，电压为0”，按M4端电压记录；未给C+对C−或两个电机端分别对GND电压，也未明确测量阶段。新增contest_motor_quad_diag，保持初始化后2秒四路持续导通，从USB USART1和JP13 USART2输出115200/8N1文本：GPIOC ODR/IDR、PC9命令/数字读回、CRH模式/CNF、KEY2和阶段；每500ms一行。不把寄存器当作实测电压或MOS/电机健康证明。

10环境构建、既有主机测试、GPIO环境无RF/PWM、诊断环境双UART初始化符号、模板/链接/快照/校验均通过，见[诊断版软件检查](validation/bench-2026-10-10/motor-quad-diag-software-check.log)。电脑暂未检测到USB串口，诊断版尚未下载、尚无真实PC9日志；上一成功下载仍为contest_motor_quad_direct。KEY2停止依赖软件轮询，两路日志各25ms超时期间可能延迟或漏短按，须持续按住直到停转。

## 诊断版随后实际下载及PC9读回

用户确认USB已接好、动力电池断开，下载contest_motor_quad_diag逐段100%校验，GO ACK三次79，采集10秒USB文本17帧。WAIT日志ms=11：PC9_cmd/read=0/0，mode/cnf=2/0；ON首帧ms=2011：cmd/read=1/1，ODR=0x33C0、IDR=0xF7C0，后续约500ms持续同状态直到ms=9751。证明该次无动力测试中PC9配置为推挽输出、2秒后输出锁存与数字读回为HIGH；未测PC9精确电压、MOS栅/漏极、电机动力电压、电流或转动。

见[上传](validation/bench-2026-10-10/motor-quad-diag-upload.log)、[真实串口](validation/bench-2026-10-10/motor-quad-diag-serial.log)、[统计](validation/bench-2026-10-10/motor-quad-diag-stats.json)。当前下载固件为contest_motor_quad_diag，依然保留四路2秒后持续导通的要求；此时后接动力不会重新延时。不能用本次断电池时的电机端0V判断M4驱动坏，也不能以IDR=1证明MOS管已导通。下一步需明确动力接法和分别对GND电压读数。
