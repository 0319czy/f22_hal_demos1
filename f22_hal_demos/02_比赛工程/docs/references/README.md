# 手册与驱动来源

## 用户已有本地资料

- [UAV-F22无人机控制器用户手册 v2.0](F22_控制器手册_v2.0.pdf)：接口、板载传感器、扩展口、电机。
- [UAV-DM22电机组件产品手册](DM22_电机组件手册.pdf)：动力组件与PWM；实际供电与电机型号须对实物确认。
- [TLE100遥控装置用户手册](TLE100_遥控器手册.pdf)：AT89S52、11.0592MHz、按键、无线与USBASP。
- [比赛实施方案](比赛实施方案_2026.7.pdf)已集中在此目录；本次核对悬停组第2、5、6节，归纳见 [规则映射](../COMPETITION.md)。
- 本地原厂遥控`ex3.c`与飞控接收例程用于核对字符协议；新任务发射端自行编写，没有把旧例程固定油门连接到真实电机。

## 4m模块

[ATK-PMW3901手册PDF](ATK-PMW3901-manual.pdf)，正点原子编写，兼容2m/4m，21页；由 [公开PDF镜像](https://cdck-file-uploads-global.s3.dualstack.us-west-2.amazonaws.com/digikey/original/3X/f/6/f6d0f4c2dda56cee66c0d4eba1ad2602090b4b97.pdf) 下载。厂商官网为 [ALIENTEK](https://www.alientek.com/)。核对了参数、原理图、接口、电源/使能及厂家位移换算示例。

关键位置：PDF第3页=参数与芯片区别，第5页图2.1.4=电路及排母信号，第6页起=光流通信；测距章节区分VL53L0X与VL53L1X。照片不用于猜测排母翻面后的脚序。

## 驱动和芯片资料

- [Bitcraze PMW3901 MIT驱动](https://github.com/bitcraze/Bitcraze_PMW3901)：初始化序列和SPI模式移植到`src/sensors/flow.c`，许可证保留在`src/sensors/LICENSE.Bitcraze`。
- [Bitcraze Motion Burst字段](https://github.com/bitcraze/crazyflie-firmware/blob/master/src/drivers/interface/pmw3901.h)：核对12字节格式、motion标志；bit4是rawFrom0，不能误当溢出。
- [ST stm32duino VL53L1X源码](https://github.com/stm32duino/VL53L1X)：ST版权/BSD三条款，默认配置与ULD长距离设置移植入`src/sensors/tof.c`，增加有界等待和逐次错误处理；保留`src/sensors/LICENSE.ST`。
- [ST ULD说明](https://www.st.com/en/embedded-software/stsw-img009.html) / [UM2510](https://www.st.com/resource/en/user_manual/um2510-a-guide-to-using-the-vl53l1x-ultra-lite-driver-stmicroelectronics.pdf)：平台IO、时间预算与测距流程。
- [PMW3901芯片数据手册](https://wiki.bitcraze.io/_media/projects:crazyflie2:expansionboards:pot0189-pmw3901mb-txqt-ds-r1.40-280119.pdf)：工作距离/光照、IO电平、SPI上限和时序。

提交号、下载源文件SHA256和模块手册SHA256见 [sources.json](sources.json)。来源验证说明代码依据，不代替模块到货后的测距、光流比例与接线验收。
