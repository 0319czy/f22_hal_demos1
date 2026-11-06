# 当前四轴电机布局

以操作者最后确认的飞机机头为前方，旋向均从飞机上方看。整体更换机头基准后的布局如下，旧fly笔记保留在archive中。

| 位置 | 接口 | 桨叶字母 | MCU/通道 | 俯视旋向 |
|---|---|---|---|---|
| 右前 | M1 | R | PC6 / TIM3_CH1 | 顺时针 |
| 左前 | M2 | L | PC7 / TIM3_CH2 | 逆时针 |
| 右后 | M3 | L | PC8 / TIM3_CH3 | 逆时针 |
| 左后 | M6 | R | PB6 / TIM4_CH1 | 顺时针 |

依据：用户最新实测记录同时列出电机位置、L/R桨标记和旋向；当前源码Aircraft1_MapMotors与该四口配置对应。电机位置、桨叶字母及旋向已核实；实际扶正力矩、气流方向和动力表现仍需验收，见PROJECT_PLAN的P3/P4。

当前机体轴映射配置见[closed_loop_config.h](../src/flight/closed_loop_config.h)，模块接线见[MODULE_WIRING](MODULE_WIRING.md)。按这架飞机的实测记录，L对应逆时针位置、R对应顺时针位置；仍需确认装桨朝向能产生向下气流。
