# 第 4 课：悬停逻辑软件演示

目标：先看懂状态机和控制层次。这个 demo 的高度、位置、姿态全部来自理想软件模型；四路电机是虚拟需求，物理 PWM 不启动。

```bash
pio run -e demo04_hover_logic
pio run -e demo04_hover_logic -t upload
```

VOFA：JustFloat / 115200 / DTR=1 / RTS=0，约 50 Hz。模板 [vofa.csv](vofa.csv) 有 20 列，初始全 0。`actual_pwm` 恒为 0，`input_simulated` 恒为 1。

启动后空闲；松开 KEY1 再按一下，软件模型开始起飞，目标高度逐步升到 0.5 m；高度/位置/姿态满足演示条件时累计稳定时间，超过 5 s 后继续保持，等待你松开再按 KEY1 发出降落请求。降落时目标高度逐步降到 0，模型落地停机。可以提前请求降落，但不满足比赛的悬停时长。KEY2 立即停止虚拟输出。停止时如果模型还在空中，它会模拟下落，落回地面后才能再次启动。

状态：0=IDLE，1=TAKEOFF，2=HOLD，3=LAND，4=FAULT。高度/位置/IMU/命令链路无效、非有限值、大倾角、超高或错误 dt 会锁存 FAULT 并清空虚拟输出；先 STOP 再 START 才能恢复。本例故障停输出是模型/台架门控演示，真实飞行的失效降落策略还需独立设计与验证。

控制层次：

1. 位置误差生成水平速度目标，速度误差生成 roll/pitch 目标。
2. 角度外环生成角速度目标，陀螺角速度内环 PID 生成三个轴的修正量。
3. 高度误差生成垂直速度目标，垂直速度 PI 修正基础推力。
4. 虚拟四旋翼混控把基础推力和修正量分配给前左/前右/后右/后左。

PID 有积分限幅、输出饱和时停止继续积分、测量微分与微分低通；混控会压缩超范围差动量并移动共同推力，使虚拟电机落在 0~1。

| CH | 数据 |
|---|---|
| 0 | state |
| 1 / 2 / 3 | height_m / height_target_m / vertical_speed_mps |
| 4 / 5 | roll_deg / pitch_deg |
| 6 / 7 | x_m / y_m |
| 8 / 9 | vx_mps / vy_mps |
| 10 | collective，归一化基础推力 |
| 11 / 12 / 13 | roll / pitch / yaw correction |
| 14 / 15 / 16 / 17 | virtual_FL / FR / RR / RL |
| 18 / 19 | actual_pwm=0 / input_simulated=1 |

注意：本课没有使用真实 SPL06 高度。0.45 基础推力、PID 参数、50 Hz 模型周期和四旋翼顺序仅供讲解，不能套到未知机架。真实输入还需轴变换、时间戳、ToF/光流驱动、速度估计、遥控心跳、机架与电机旋向确认，并完成实机调参。

练习：将模型目标高度改为 0.4 m，观察起飞/保持/降落；在主机测试里让 height_valid=0，说明为什么进入 FAULT；指出气压计数据平滑为何不能代替可靠测距。

比赛流程与评分对应见 [规则映射](../../../docs/COMPETITION.md)。本课持续保持时可以看 `controller.stable_seconds`；整合课将此数值和资格标记独立发送。
