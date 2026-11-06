# 光流距离比例和旋转补偿标定

当前板上 `contest_flow_calibration_jp13`，44040字节完整读回，SHA256为 `4d6fed6c8fcb00532cd3e7fd78a30048a392f5be827669ec99beb72ee3bb14b4`。继承主循环STOP和底层非零PWM拒绝，不会启动电机。未更改 `FLOW_CALIBRATED`、`CL_FLOW_AXES_VERIFIED`；标定输出只生成候选，不自动写参数或解锁。

**本项工作当前暂停（2026-10-16，按用户要求）。**不再安排光流平移、抬高或比例/旋转标定。现在优先解决系留起飞无法稳定越过8cm；光流用于横向反馈，不是产生垂直升力的环节。以下流程和采集结果保留作历史记录，恢复前先看[当前计划](../PROJECT_PLAN.md)。

**2026-10-18控制器实现更新：**源码已使用历史定性方向候选`forward=+dy, right=+dx`，并接入相对位置保持；这只确定符号，不代表比例或旋转补偿通过。`FLOW_CALIBRATED=0`和`CL_FLOW_AXES_VERIFIED=0`未改变；位置保持候选经过主机回归/固件构建，但没有烧录，也没有用于实体电机控制。比例、旋转补偿和独立验证数据仍缺，不能据此解锁横向闭环。[实现与验证记录](validation/flow-position-hold-2026-10-18/manifest.json)。

操作者已澄清实际移动飞机、纹理不动。定性方向为 `forward=dy, right=dx`，候选translation为 `{0,1,1,0}`。这些方向结果只作历史证据；没有接受比例与旋转补偿系数。

## 为什么改采集

原来的200ms遥测只打印当时一个样本，不能将那些数相加换算厘米。新诊断在每次实际光流采样窗口累计，200ms只输出累计端点；少显示一行不会丢掉中间计数。当前实测约43个采样窗口/秒，配置周期20ms，实际受到主循环调度影响。

原始计数和有效标定累计分开。有效累计要求同一窗口内IMU、光流、有效高度与时长均可信；剔除时同时剔除counts和gyro。首个不完整窗口不参与拟合。重校准或恢复增加epoch，跨epoch段作废；有拒绝采样的动作区间不能当作完整轨迹。计数上限65535个原始样本后明确sat，不回绕继续伪造累计。

| FCAL字段 | 单位/含义 |
|---|---|
| polls / raw_n / n / bad | 尝试、原始成功、用于标定、剔除的累计窗口数 |
| raw_dx / raw_dy | 原始成功读取的累计计数，不要求有效高度 |
| dxsum / dysum | 有效窗口累计计数 |
| hx_mcount_milli / hy_mcount_milli | `Σheight_m*dx/dy` ×1000，取差后除1000得到m·count |
| angle_urad | 同一批有效窗口的 `Σgyro_dps*dt*π/180` ×1e6，顺序roll/pitch/yaw |
| hangle_umrad | `Σheight_m*gyro_dps*dt*π/180` ×1e6，除1e6后单位m·rad |
| epoch / pollerr / sat | 窗口分代、轮询错误累计、累计饱和 |
| why | 本次启动以来剔除原因按位累计，地面出现4/16是历史记录，不代表当前一直无效 |
| i2c_boot | 低位为开机清总线脉冲数，bit8为清理后总线仍低；0为无需清理且两线高 |

清I²C只在本诊断启动、输出关闭时、正常I²C2初始化之前执行。GPIO开漏释放、最多9个SCL脉冲、STOP；SCL被保持低则报告失败。这次实际下载复位后IMU/ToF健康，不宣称长期总线问题已永久解决。

## 采集动作

用尺标记真实20cm位移，模块朝向纹理且离桌面约20cm，机头朝向不变。每段起点停3秒，平移2～3秒，终点停3秒，随后可以放下再回复。分别记录去程、返程、左右；整段往返净位移为零，不能混成一次20cm去程。

标定比例必须知道实际位移，不能以“有动作”代替尺量。保持高度/姿态稳定能减小误差，高度加权累计可以处理一定高度变化。明显转头或手持倾转区不当作纯平移。端点选取保留在证据中，不把选择窗口隐藏掉。

旋转补偿需要分别做roll、pitch单程转动，镜头光心应尽量固定在空间同一点；绕机身中心转动会让偏置镜头产生真实平移，不能全部拟合成光学误差。正负方向分段，保留独立重复动作验证。没有可信的旋转中心时不接受旋转标定。

## 拟合模型与验收边界

与生产 `FlowEstimate_Update` 一致，令 `C=[Σh*dy, Σh*dx]`，`B=[Σh*Δroll_rad, Σh*Δpitch_rad]`，则每段位移 `D=k*C-R*B`，其中k为radians_per_count，R为2×2旋转补偿。

当前脚本先拟合固定swap基线，共5个未知参数；必须有平移及独立roll/pitch激励，归一化设计矩阵满秩。输出训练残差、独立holdout预测误差与条件数，始终 `calibration_accepted=false`。纯平移、无旋转时单轴粗估k可用 `distance/Σh*counts`，不能将粗估当作最终值。

若存在交轴残差，先检查实际轨迹、转头、端点与手持倾转，不能凭单次记录假定是安装角，也不直接增加参数把残差拟合掉。必要时再比较完整 `A=k*T` 与R共8参数的模型，须增加独立动作/保留组并规定k与T分解规范；目前未实现或采用该扩展。

第一条用户自报20cm去程的候选端点146.31～151.53秒，218个完整有效窗口；不补偿的固定swap模型残差约5.1cm，这是对“纯前移20cm”假设的模型误差，不是实测侧移。该段gyro累计yaw变化约−7.6°且有倾角变化，不能单独接受比例。

用户随后完成标记20cm回程。第一次长采集结束后，累计原始样本数在板上达到65535上限；该次回程记录虽显示ToF进入有效高度，但`sat=1`，累计器拒绝新样本，因此没有用于标定。通过DAP复位清零后重采，日志ID为49/B6，IMU/ToF正常，电机始终PWM=0。暂选host_seconds 26.6129～33.0399作为回程区间，获得270个有效窗口，窗口拒绝增量0、轮询错误增量0、sat=0；高度从317mm变化到222mm，光流质量124～161。区间加权特征`C=[forward,right]=[-86.812,20.163] m·count`，角度积分约`[roll,pitch,yaw]=[+0.1065,-0.2011,+0.0984] rad`。按旧名义`k=0.002131946`且不补偿旋转，预测约后退18.5cm、右偏4.3cm；仅是与用户报告20cm后移的粗略一致性检查。俯仰变化约−11.5°、滚转+6.1°、偏航+5.6°，且端点为暂选，因此不能接受该比例或侧向预测。回程无补偿粗估`k≈0.00230`，与去程候选`≈0.00426`差异明显；后续前移复测已补充，但有效区间包含升高/下降，仍不能厘清系数。接下来需要把有效高度下的起点静止、纯平移、终点静止分开采集，再分开采横移与roll/pitch旋转，并保留独立验证段。

随后又做一次20cm前移复测。采集139秒、686帧，DAP串口正常释放；电机输出始终硬锁止。完整抬高至放下区间暂取host_seconds 110.0652～122.9134：有效窗口增加547，区间内拒绝和轮询错误增量均为0、sat=0，SQUAL 116～140。ToF有效值范围86～323mm；加权特征`C=[forward,right]=[34.485,12.409] m·count`，累计姿态变化约`[roll,pitch,yaw]=[+0.0817,-0.0093,-0.0663] rad`。旧名义系数不补偿旋转时整段预测前进7.35cm、右偏2.65cm。该段从升高阶段延续到下降阶段，并非被精确标记的20cm纯平移；因此正向计数支持前移符号及有效高度下有运动响应，但7.35cm/2.65cm不是有效的距离或横漂测量，也不据此更新比例。后续应先把飞机稳定在ToF有效区间、姿态与机头方向固定，等起点停留完成后再开始短采集；平移后先抓住终点停留数据，再放下。证据见[复测检查](validation/flow-calibration-2026-10-16/forward-repeat-check-provisional.json)及其原始[采集目录](validation/flow-calibration-2026-10-16/live-level-forward-repeat/)。

### 最新受控前移（2026-10-16）

在操作者报告悬停高度稳定后，启动35秒短采集；遥测显示运动开始前基线基本稳定，随后按要求前移并保持终点。暂取host_seconds 8.0473～18.0894作为基线至终点区间，50条遥测对应424个有效采样窗口，bad、pollerr、sat增量均为0；每条区间记录的IMU、ToF、光流均有效，ID为49/B6，电机PWM全零。ToF 111～182mm，SQUAL 101～180。区间加权特征`C=[forward,right]=[143.669,-25.011] m·count`，高度加权角特征`B=[roll,pitch]=[0.013296,0.008846] m·rad`；姿态积分变化为滚转+5.404°、俯仰+3.398°、偏航−3.825°。

前向分量为正，和操作者要求的飞机前移方向一致，说明传感器在该高度区间能给出相符的前向响应。由于姿态和高度在区间中变化，实际20cm位移与起止时刻也没有用独立标记核实，右向分量不能当作实际侧移，且不能据此估计或更新距离比例、旋转补偿。证据见[受控前移区间检查](validation/flow-calibration-2026-10-16/controlled-forward-check-provisional.json)、[起点快照](validation/flow-calibration-2026-10-16/controlled-forward-start-provisional.json)、[终点快照](validation/flow-calibration-2026-10-16/controlled-forward-end-provisional.json)和[原始采集](validation/flow-calibration-2026-10-16/live-forward-controlled-retry/)。本项目当前暂停光流采集；恢复标定前先解决地面起飞问题。

## 工具和证据

采集工具不发任何串口字节，记录原始日志、全部累计帧及原子更新的latest.json；通过输出目录下STOP文件结束，或到限定时长自动结束。拟合依赖本机已安装的NumPy，输出仅供审查。

```bash
cd /home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程
/home/czy/.platformio/penv/bin/python -u scripts/capture_flow_calibration.py --out-dir docs/validation/flow-calibration-2026-10-16/live --seconds 1200
python3 scripts/fit_flow_calibration.py docs/validation/flow-calibration-2026-10-16/trials.json --output docs/validation/flow-calibration-2026-10-16/fit.json
```

目前有一条去程和一条回程候选，但两段都含明显姿态变化，尚无独立roll/pitch标定和holdout；拟合脚本应继续拒绝接受系数。上次20分钟采集加等待使板上累计计数达到65535，复位后短采集已正常，今后每次采集前检查`sat=0`。不要开第二个监视器争抢同一DAP串口。采集释放后，`show_remote_keys.py` 默认显示累计采样/计数，`--full`保留原始FCAL行。

[本轮清单](validation/flow-calibration-2026-10-16/manifest.json)、[去程候选检查](validation/flow-calibration-2026-10-16/forward-check.json)、[回程候选检查](validation/flow-calibration-2026-10-16/return-check-provisional.json)、[前期方向证据](validation/flow-axis-check-2026-10-16/signed-axis-result.json)。主机合成测试和编译通过不等于实际比例、旋转补偿或稳定飞行通过。
