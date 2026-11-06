# 当前固件状态（2026-10-18）

当前板上已烧入`contest_tethered_flight_debug_jp13_pwm100`，45224字节，SHA256=`ce55accb93ef98e4bd66a14c77d19ca726711d1cfeba132eb1568c25da22d632`。DAP `814dc35a`通过JP11烧录到STM32F103RC；完整45224字节读回与BIN逐字节一致。烧录前256KiB备份SHA256=`2468c249d688c3e7ee0adc353fac215d743da3beb9c673176795d2f19956931f`。这个镜像具有电机输出权限，单路PWM软件上限1000‰；**它不是锁电机诊断版，也未做本轮飞行验证。**

本镜像沿用当前检查过的系留100%无光流控制路径：IMU姿态/角速度闭环、ToF垂直观测与高度控制、无线起降命令。它**没有把光流接入横向闭环**，所以不是[控制设计](../docs/CONTROL_DESIGN_2026-10-16.md)所描述的IMU+ToF+光流完整融合实现。按键为F2起飞、F3受控降落、F4立即停止；F4不是缓降。主机回归、PWM寄存器替身测试、11个PlatformIO环境构建和镜像标记/符号检查通过；烧录后尚未抓串口确认启动状态，也未验证实际飞行。详见[本轮烧录清单](../docs/validation/tethered-debug-output-2026-10-18/manifest.json)和[项目计划](../PROJECT_PLAN.md)。

下面全部是按时间追溯的历史固件状态，不代表当前板上状态。源码其它系留配置仍锁止电机；只有明确的`contest_tethered_flight_debug_jp13_pwm100`目标开放物理输出。接收端单独交换F3/F4的候选已撤回。软件检查通过不等于飞行验收通过。

---

# 固件索引（历史候选与烧录记录）

**10月16日候选（历史，未烧录）：** `contest_tethered_flight_jp13_pwm100_softland.bin`，42628字节，SHA256=`ef529418d6e7caea2f4c99bc48e607b9bec208488a18a19a5dd36989470f28fe`。该候选曾提议F4缓降/F3急停，但没有烧录；当前镜像仍为F3受控降落/F4立即停止。[历史记录](../docs/validation/soft-landing-2026-10-16/manifest.json)。

10月16日之前板上曾烧录 `contest_tethered_flight_jp13_pwm100_noflow`。其完整读回、启动标记和一次F2/F4操作记录见[历史记录](../docs/validation/tethered-pwm100-noflow-2026-10-16/manifest.json)；已由10月18日镜像替代。

10月16日该无光流版本使用100%控制器/驱动上限、0至100%缓升和3秒离地超时；IMU、遥控、ToF与看门狗停机保留。它与当前版本的控制路径相近，但当时输出仍受隔离策略约束。[历史证据](../docs/validation/tethered-pwm100-noflow-2026-10-16/manifest.json)。

10月16日光流复查时，用户接回断线后芯片ID49/反码B6、present1、errors0，运动原始数据正常变化；当时未完成光流标定或飞行验收。[复查数据](../docs/validation/flow-recheck-2026-10-16/113836-result.json)。

10月16日曾烧录 `contest_flow_spi_duplex_probe_jp13.bin` 电机锁止诊断版，43504字节完整读回一致，SHA256 `8897ed5d610c12ac5f945b65278484207b87e568dce9585e3004872af3c11b69`。[历史证据](../docs/validation/flow-transport-ab-2026-10-16/manifest.json)。

10月15日至16日曾使用 `contest_flow_id_probe_jp13.bin`、`contest_flow_spi_duplex_probe_jp13.bin` 等锁止诊断版检查光流链路；相关读回记录见[光流诊断记录](../docs/validation/flow-identification-2026-10-15/manifest.json)。这些诊断版均已被替代。

此前识别恢复候选（已被临时锁止诊断版替代）：`contest_flow_assist_jp13_pwm100_spi_candidate.bin`，43200字节、SHA256=`ce4a0e9cc031efbfddd81c9183081a530d6767f3b4e117ce5ea260fdd53a266c`，JP11完整读回一致。补片选复位/页0/单寄存器读等待，100%上限及标定检查保留。实际光流识别恢复未验收。[最新记录](../docs/validation/flow-id49-ff-2026-10-15/manifest.json)。

此前共同控制候选（本轮已被SPI调整候选替代，历史烧录读回通过）：`contest_flow_assist_jp13_pwm100_candidate.bin`，43124字节，SHA256=`9a0a2d318e7ad0971dda035ce9b4a14b8066012b330005f4c38939bc0d2fb6ad`。按用户要求开放100%上限，起飞仍缓升、姿态/高度共同调节输出；IMU全程稳姿，可信光流修正横向速度。九环境及控制器→PWM驱动100%寄存器替身联测通过，实际识别/标定及稳定飞行未通过；F2目前仍因光流未识别/未标定而拒绝起飞。此前未枚举到DAP；用户要求烧录后，814dc35a已识别，实际擦写21页并完整读回43124字节一致。[烧录记录](../docs/validation/flow-assist100-2026-10-15/last-successful-flash.json)。[行为与下一步](../docs/FLOW_ASSIST.md)、[软件证据](../docs/validation/flow-assist100-2026-10-15/manifest.json)。

此前90%光流软件候选 `contest_flow_assist_jp13_candidate.bin` 保留，43124字节，SHA256=`d1a8af9bd5babd3b0fc783db36b6b540bf174d10b9452960f84bb00730356201`，同样未烧录。[旧候选证据](../docs/validation/flow-assist-2026-10-15/manifest.json)。

此前已烧入飞控（本轮被共同控制候选替代）：`contest_tethered_flight_jp13_pwm90.bin`，41624字节，SHA256=`dec7f44022cc41ad253163e2a990fc2a8547893549230a669167d97ce54cc73e`。起飞基础最多90%、单路硬上限90%、3秒离地段，捕获离地推力参考，首轮目标测距20cm。F2起飞/F3降落/F4立即清零，上电0。八环境及回归通过；JP11/814dc35a实际擦写40960字节20页、跳过2048字节，整份41624字节读回一致。[本轮记录](../docs/validation/tether90-2026-10-15/manifest.json)、[操作说明](../docs/TETHERED_FLIGHT.md)。实飞尚未验收。

此前 `contest_attitude_check_jp13_pwm20.bin` 为15%基础/20%上限拆桨台架，40204字节，SHA256=`2ad9dcb2434d71a12ab70720620eec56471f1e4e457ee1e8c9c187ff51b464a9`。[历史记录](../docs/validation/attitude-check20-2026-10-15/manifest.json)。操作者已确认四方向静态电机差速和F4实际全停。

此前100%候选 `contest_tethered_flight_jp13_pwm100.bin` 保留追溯，41616字节，SHA256=`b9b9f91c44fae86000dc5644205531881ae02e325e3a1a5312c86daa82ff5843`，未烧录；按用户最新指令改为90%候选。[历史候选记录](../docs/validation/tether100-2026-10-15/manifest.json)。

此前 `contest_motor_direction_pwm5_imu_mount_fix.bin` 为固定5%及IMU映射修正版，38684字节、SHA256=`c12599db821d3245044665a184007a2b9b760948f44d880a4a47100e32172a6e`，[旧记录](../docs/validation/imu-mount-fix-2026-10-15/manifest.json)。用户已口述显示方向正确，四路同速不证明扶正。

此前 `contest_motor_direction_pwm5.bin` 保留为原IMU映射的5%诊断历史版，38688字节、SHA256=`fdd55cd01d71840f1dd7dfaf43c09756832175506183dc655ec280fe402f1317`，[旧记录](../docs/validation/motor-direction-pwm5-2026-10-15/manifest.json)。

原 `contest_motor_direction_pwm1.bin` 保留为历史1%版本，操作者反馈偏小，[旧记录](../docs/validation/motor-direction-pwm1-2026-10-15/manifest.json)。

此前90%起飞试验版 `contest_aircraft1_radio_v2_jp13_pwm90.bin` 已被本诊断版替代；[历史记录](../docs/validation/pwm90-2026-10-15/manifest.json)、[实际失败试验](../docs/validation/pwm90-ground-test-2026-10-15/result.json)。飞行尚未验收。

历史功率版本保留追溯：

- `contest_aircraft1_radio_v2_jp13_pwm70.bin`：[记录](../docs/validation/pwm70-2026-10-15/manifest.json)；两轮达到700‰后fault7，无有效离地高度。
- `contest_aircraft1_radio_v2_jp13_pwm60.bin`：[记录](../docs/validation/pwm60-2026-10-15/manifest.json)；已烧录读回，随即被70%替代，无实机试验记录。
- `contest_aircraft1_radio_v2_jp13_pwm50_outputfix.bin`：[驱动上限修复](../docs/validation/pwm50-output-fix-2026-10-15/manifest.json)；两轮达到500‰后fault7，[操作分析](../docs/validation/pwm50-ground-test-2026-10-15/result.json)。
- `contest_aircraft1_radio_v2_jp13_pwm50_candidate.bin`：控制器50%但驱动20%的历史错误版本，不用于验证50%起飞。
- `contest_aircraft1_radio_v2_jp13.bin`：旧15%/20%校准修复版，[旧记录](../docs/validation/imu-recalibration-2026-10-14/candidate.json)。

其他BIN是历史诊断/单机/固定推力/无PWM预览产物，保留供追溯，不代表当前飞控程序，也不是已经验收的比赛飞行固件。构建后.pio中BIN不会自动代表已烧录；须保存烧录及读回记录。

遥控器固件另在remote/tle100目录，手动主线tle100_latency_fast。自动按键测试固件不得用于正常操作。

未烧录传输对比候选：`contest_flow_spi_duplex_probe_jp13.bin`，43504字节，SHA256 `8897ed5d610c12ac5f945b65278484207b87e568dce9585e3004872af3c11b69`。继承电机锁止，日志 `spi_transport=duplex`；只供SPI诊断，非飞行固件。电脑当前没有DAP枚举，未改变板上最后验证的43448字节识别诊断版。[本轮证据](../docs/validation/flow-transport-ab-2026-10-15/manifest.json)。
