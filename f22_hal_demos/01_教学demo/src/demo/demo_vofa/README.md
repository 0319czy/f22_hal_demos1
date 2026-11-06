# 第 1 课：VOFA 观察传感器

目标：读懂加速度 g、角速度 °/s、姿态 °，理解 float32 数据帧，观察传感器和姿态模型的关系。

代码入口：`main.c`。环境 `demo01_vofa` 与 `demo_vofa` 使用同一份代码。接板载 CH340 USB，开机将板子静止放置，前约 0.5 s 用于陀螺零偏采样。

```bash
pio run -e demo01_vofa
pio run -e demo01_vofa -t upload
```

VOFA 用 JustFloat、115200、8N1、无流控。运行控制线为 DTR=1、RTS=0；连接后无数据先检查端口、占用和 RESET。20 列全零模板在 [vofa.csv](../../../docs/vofa.csv)，完整通道表和 Cube 四元数绑定见 [VOFA.md](../../../docs/VOFA.md)。

实验：

1. 静止：加速度模长约 1g、角速度接近 0、ImuOK/BaroOK 为 1。
2. 缓慢绕一根轴转动：观察对应 gyro，再看 roll/pitch/yaw 变化；避免超出当前 ±250 °/s 陀螺量程。
3. Cube 绑定 quat_x/y/z/w，模型随板子动；初始显示方向可用位姿偏置调整。
4. 看 CH19 `relative_altitude_m`，解释气压波动与漂移。它经过平滑，但不表示准确离地高度。

提问：板子静止时 acc_z 为什么接近 1 而不是 0？gyro_z 归零后 yaw 为什么不一定归零？平滑高度是否等于提高测距准确度？

本课没有电机输出。CSV 是列名/初始值模板，`vofa+.csv` 是用户采集文件，不能互相覆盖。当前教学验证状态见 [验证记录](../../../docs/LESSON_VALIDATION.md)。
