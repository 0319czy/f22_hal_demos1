# M1/M2/M3/M6持续15%拆桨台架

环境 `contest_motor_m1236_pwm15`，入口 `src/bench/motor_m1236_pwm15.c`，固件 `firmware/contest_motor_m1236_pwm15.bin`。用户要求先持续运转，不再由高度、倾斜、角速度或离地2秒超时打断；此独立台架入口采用固定占空比，不运行姿态/高度闭环，也不采集IMU/ToF/光流。原闭环入口及固件保留。

- 上电四路PWM0。KEY1松开至少50ms后新按下30ms触发启动；四路固定15%持续运行，无运行时长上限。启动时按住KEY1不自动转。
- 再按KEY1直接停止。KEY2在SysTick及主循环中立即清零；松开KEY2不自动重新启动。
- 保留输出驱动每路20%硬上限、主循环20ms未更新时停机、非法输出停机、输出故障锁存，以及HardFault/Error_Handler先清PWM。
- M1=PC6/TIM3CH1、M2=PC7/CH2、M3=PC8/CH3、M6=PB6/TIM4CH1；M4/M5/M7保持LOW。TIM3/4 PSC23、ARR199、CCR30，15kHz、30/200=15%。实际波形及电机持续运行仍需实物验证。
- USB115200、8N1，E49置模式3且无无线接收。日志前缀 `[MOTOR15 LOCAL]`，运行时应看到 `running=1 trip=0 PWM_permille_M1_M2_M3_M6=150,150,150,150`，停止时四路0。这是独立的持续电机测试，不将日志当作飞控闭环或传感器验收。

构建：`pio run -e contest_motor_m1236_pwm15`。新控制器合成输入测试验证60秒持续需求、上电停止、KEY1切换、KEY2停止、按住按键不重新启动和输出故障锁存；实际PWM驱动另有寄存器替身测试验证引脚、定时器配置和急停。主机测试不代表实物持续转动通过。

固件构建、校验摘要及烧录状态见[motor_continuous_firmware.json](motor_continuous_firmware.json)。下载前断开动力电池、保持拆桨，仅接USB；下载会复位MCU。烧录需先核对实际板UID并读回校验。
