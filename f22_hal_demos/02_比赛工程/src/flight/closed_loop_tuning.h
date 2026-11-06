#ifndef F22_CLOSED_LOOP_TUNING_H
#define F22_CLOSED_LOOP_TUNING_H

/* 这个文件是闭环调参的唯一默认参数入口。单位都是 duty fraction，
 * 例如 0.005f 等于 5 个千分点。日常调参只改这里，不要把数值散落到
 * closed_loop.c。下面按本轮实际调参顺序排列，数值保持当前版本不变。
 *
 * 本轮目标和顺序：
 *   0. 先确认电机编号、旋向和重心；拆桨时不能用姿态现象判断方向。
 *   1. 只调“现场 trim”四项，解决持续左/右、前/后、偏航和高度偏差。
 *   2. trim 稳定后才调 rate PID，目标是无来回摆动、gyro 误差约 ±10 deg/s。
 *   3. ToF 连续有效后才调高度 PI，目标是高度误差约 ±20 mm、vz 约 ±50 mm/s。
 *   4. 近地 ToF 无效或 fault=11/12 时，先修传感器/起飞保护判决，不调 PID。
 *   5. 光流参数只给启用 flow-assist 的固件使用，本轮默认不调。
 *
 * 每次只改一个宏；现场 trim 每次改 0.003f~0.005f，观察一轮再继续。
 * 如果现象变反，回退这一步或把该 trim 反号。当前无线 DAP 上行未通，
 * S 命令不会到达飞控；修改默认值后需要重新编译烧录。
 */

/* ===================== 本轮第一组：机械/方向 trim ===================== */

/* 左右漂移补偿。正值提高左侧 M2/M6、降低右侧 M1/M3，使机体向右修正。
 * 看到持续向左漂移或左倾：逐步增大；向右漂移或右倾：减小，必要时改负。
 * 目标：松绳后不持续向一侧走，roll 长时间保持在约 ±2 deg 内。 */
#define CL_LEFT_DRIFT_TRIM .000f

/* 前后漂移补偿。正值提高前侧 M2/M1、降低后侧 M3/M6，使机头抬起、向后修正。
 * 看到向前漂移或机头下压：逐步增大；向后漂移或机头上仰：减小或反号。
 * 目标：机体不持续向前/后滑，pitch 长时间保持在约 ±2 deg 内。
 * 2026-10-27 实测：两次起飞 pitch 都从 0.4° 一路向负走（-4.7°/-25°），
 * 俯仰修正顶在 4% 限幅。本机先给 +0.005 的抬头补偿，允许用 -D 覆盖。 */
#ifndef CL_FRONT_DRIFT_TRIM
#define CL_FRONT_DRIFT_TRIM .000f
#endif

/* 偏航补偿。正值提高代码定义的 CCW 对 M2/M3、降低 CW 对 M1/M6。
 * 看到持续顺/逆时针转动：只改这一项；若实际旋向与记录相反就反号。
 * 目标：松绳后无持续偏航，yaw 角速度约在 ±10 deg/s 内。
 * 2026-10-27 实测：悬停段 gyro_z 持续 +19~22°/s（机头向右偏），故取负值。
 * 允许用 -D 覆盖。 */
#ifndef CL_YAW_TRIM
#define CL_YAW_TRIM .000f
#endif

/* 公共升力偏置，四个电机同量增加或减少，不改变姿态差分。
 * ToF 有效且稳定悬停仍偏高：用小负值；偏低：用小正值。
 * 目标：先用机械/姿态 trim 稳住，再让高度误差落在约 ±20 mm。 */
#define CL_COLLECTIVE_TRIM .000f

/* ===================== 本轮第二组：姿态 rate 内环 ===================== */

/* 角速度比例增益：决定对 gyro 误差的即时纠正力度。
 * 机体 2~5 Hz 来回抖动：减小；响应明显迟钝且不振荡：小幅增大。
 * 目标：姿态受到扰动后能回正，不能用它修正固定方向的重心偏差。
 * 2026-10-27 抗扰需求：悬停段 gyro 尖峰 25~75°/s、差分限幅常顶满，
 * 先加 50%（0.0008→0.0012）提升扰动回正速度；允许 -D 覆盖。 */
#ifndef CL_RATE_KP
#define CL_RATE_KP .0008f
#endif

/* 角速度积分增益：消除长时间残留的角速度偏差。
 * 只有 rate_kp 已稳定、仍有慢性偏差时才增加；出现慢摆或累积偏转就减小。
 * 目标：稳态 gyro 误差接近 0，积分不持续顶到上限。 */
#ifndef CL_RATE_KI
#define CL_RATE_KI .0001f
#endif

/* roll 方向的姿态差分上限在 closed_loop_profile.h 中定义；本宏是 pitch/yaw
 * 差分上限。出现单轴修正过猛或 PWM 差值过大时减小，响应不足才增大。 */
/* 2026-10-28 实飞：起飞高油门段 pitch 掉到 -15°，而俯仰修正长期顶在 4% 限幅
 * 上（correction_pm=40），权限不足导致机头下压、推力前倾漂移。允许 -D 覆盖。 */
#ifndef CL_RATE_PITCH_YAW_LIMIT
#define CL_RATE_PITCH_YAW_LIMIT .04f
#endif

/* rate 积分限幅，防止长时间姿态误差把积分累到过大。
 * 松手/降落后仍有明显反向冲击时减小；稳态误差消不掉且未饱和时才增大。 */
/* 2026-10-28 实飞：偏航长期 -20~-37°/s 且随油门增大，属固定力矩偏置；
 * rate 积分限幅仅 ±10‰ 早已饱和，无法抵消。放宽到 ±50‰。 */
/* 偏航专用积分增益与限幅（roll/pitch 仍用 CL_RATE_KI）。 */
#ifndef CL_RATE_KI_YAW
#define CL_RATE_KI_YAW 0.010f
#endif
#ifndef CL_RATE_INTEGRAL_LIMIT_YAW
#define CL_RATE_INTEGRAL_LIMIT_YAW 0.20f
#endif

#ifndef CL_RATE_INTEGRAL_LIMIT
#define CL_RATE_INTEGRAL_LIMIT .01f
#endif

/* 姿态角误差转换为角速度目标的比例。
 * 回正太慢时增大；接近目标时来回摆动时减小。先保持 rate PID 稳定再调。 */
#define CL_ANGLE_TO_RATE_GAIN 3.0f

/* 角速度目标限幅，防止大姿态误差产生过激动作。
 * 起飞时突然猛修正或 PWM 很快饱和时减小；大角度回正太慢才增大。 */
#define CL_RATE_REF_LIMIT_DPS 45.0f

/* 起飞低油门阶段的姿态差分权重起点。
 * 该值不是左右 trim；过小会导致低油门扶不住，过大会在刚起转时抖动。 */
/* 低油门阶段的姿态差分权重起点。2026-10-28 实飞：起转段 roll 从 2.6°
 * 长到 5~7.5°，开环漂移明显，提高低油门姿态权限；允许 -D 覆盖。 */
#ifndef CL_ATTITUDE_AUTHORITY_COLLECTIVE
#define CL_ATTITUDE_AUTHORITY_COLLECTIVE .08f
#endif

/* ===================== 本轮第三组：ToF 高度环 ===================== */

/* 低空固件的目标测距值：当前低空测试为 0.12 m，普通系留版本为 0.20 m。
 * 只在 ToF 连续 valid=1 后调整；改变它会直接改变悬停高度。 */
#ifdef CL_LOW_HOVER_TEST
#ifndef CL_TARGET_RANGE_M
#define CL_TARGET_RANGE_M .12f
#endif
#else
#ifndef CL_TARGET_RANGE_M
#define CL_TARGET_RANGE_M .20f
#endif
#endif

/* 高度误差转换为垂直速度目标的比例。
 * 高度回正太慢才增大；上下反复追高度时减小。ToF 无效时调它没有意义。 */
/* 2026-10-28 实飞：目标 200mm 时冲到 287mm 再回落，说明接近目标时速度没提前收。
 * 降到 0.8 让高度误差产生的速度目标更保守；允许 -D 覆盖。 */
#ifndef CL_HEIGHT_TO_VZ_GAIN
#define CL_HEIGHT_TO_VZ_GAIN 1.0f
#endif

/* 垂直速度目标限幅，限制高度环一次要求的上升/下降速度。
 * 上下动作过猛时减小；高度恢复太慢且没有振荡时增大。 */
#ifndef CL_VZ_TARGET_LIMIT_MPS
#define CL_VZ_TARGET_LIMIT_MPS .15f
#endif

/* 高度比例增益：高度误差产生的垂直修正力度。
 * ToF 有效但高度来回上下摆动时减小；高度偏差恢复慢且无摆动时增大。 */
#ifndef CL_VERTICAL_KP
#define CL_VERTICAL_KP .18f
#endif

/* 高度积分增益：消除长期偏高/偏低。
 * 只有 vertical_kp 稳定后才调；慢性上下摆动或降落前冲击增大时减小。 */
/* 2026-10-28 实飞：HOLD 段 1 秒周期 ±5cm 极限环（油门 900~950‰ 还在追），
 * 降积分与比例压振荡；允许 -D 覆盖。 */
#ifndef CL_VERTICAL_KI
#define CL_VERTICAL_KI .03f
#endif

/* 高度反馈总输出限幅，围绕起飞时捕获的基础推力工作。
 * 反馈经常顶到此上限，先检查基础推力/供电/ToF，不要直接加大。 */
/* 2026-10-28 实飞：交接前馈 ~936‰、真实悬停 ~885‰，需要下修 51‰，
 * 而 ±5% 限幅正好卡死 → 高度从 82mm 冲到 318mm。放宽到 ±10% 并允许 -D 覆盖。 */
#ifndef CL_VERTICAL_LIMIT
#define CL_VERTICAL_LIMIT .05f
#endif

/* 高度积分限幅，防止 ToF 暂时失效后积分积累过多。
 * 恢复 ToF 后突然冲高/下压时减小。 */
#define CL_VERTICAL_INTEGRAL_LIMIT .015f

/* 公共升力向目标值上升的速度，单位 duty/s。
 * 起飞加力太急时减小；离地困难且传感器有效时才增大。 */
/* 2026-10-28 实飞：上升斜率仅 0.10/s，而下降被我调到 1.0/s，比例 1:10。
 * 结果高度环"砍油门过快、补油门过慢"，每次超调后都补不回来 → 掉高度。
 * 放宽上升斜率让两者对称（允许 -D 覆盖）。 */
#ifndef CL_COLLECTIVE_SLEW_UP
#define CL_COLLECTIVE_SLEW_UP .10f
#endif

/* 公共升力向目标值下降的速度，单位 duty/s。
 * 正常降落过快时减小；降力拖沓且不下冲时才增大。 */
#ifndef CL_COLLECTIVE_SLEW_DOWN
#define CL_COLLECTIVE_SLEW_DOWN .25f
#endif

/* 进入降落状态后的下降速度，不能用来修正空中姿态漂移。 */
#define CL_LAND_COLLECTIVE_SLEW_DOWN .50f

/* 最后近地阶段的公共升力衰减速度，防止落地前悬空或砸落。 */
#define CL_LAND_FINAL_FADE_PER_SEC .40f

/* ===================== 本轮第四组：起飞/保护参数 ===================== */

/* ToF 有效交接距离；这是传感器交接阈值，不是“已经离地”的证明。
 * 2026-10-27 满电实测：LAUNCH 段融合高度最高 74mm，而门限 80mm 导致 5 次
 * 起飞都停在 LAUNCH（ToF 60～83mm 却进不了 ASCEND）。允许 -D 覆盖，
 * 低空试验用 60mm；仍远高于 ToF 有效门限 40mm。 */
#ifndef CL_TAKEOFF_RANGE_M
#define CL_TAKEOFF_RANGE_M .08f
#endif

/* ToF 证据断档宽限（秒）。单次坏转换（例如 ST 原始状态 4 相位失败）不应立即
 * 停机：2026-10-27 实机记录中，一次坏样本就让起飞在 581‰ 被打断、LAUNCH 只
 * 存活 9 帧。超过该时长仍没有新鲜测距才报 CL_TOF_LOST；器件完全缺席仍立即
 * 判故障。调参方向：起飞老被坏样本打断就加到 0.15~0.20；担心空中失测太久
 * 就减小，但不小于一次转换周期（本驱动 40 ms）。 */
#define CL_TOF_GAP_GRACE_SEC .10f

/* 已知地面距离上增加的安全余量，用于近地目标判断。 */
#define CL_GROUND_CLEARANCE_MARGIN_M .04f

/* 起飞目标测距值向最终目标爬升的速度，单位 m/s。起飞目标变化过快时减小。 */
#define CL_TARGET_RANGE_SLEW .10f

/* 起飞阶段垂直速度目标限幅；起飞冲得过猛时减小。 */
#define CL_LAUNCH_VZ_TARGET_LIMIT .15f

/* 起飞阶段 trim 接近高度 PI 目标的速度，单位 1/s。 */
#define CL_LAUNCH_TRIM_SLEW .25f

/* 起飞阶段相对基础推力的最大 trim。频繁顶限时先查 ToF 和推力，不要盲目增大。 */
#define CL_LAUNCH_TRIM_LIMIT .05f

/* ===================== 本轮第五组：光流 XY（默认不调） ===================== */

/* 爬升/悬停阶段相对高度环前馈的最大下砍幅度（油门比例）。
 * 防止一次超调把油门砍到悬停值以下导致掉高度。默认 1.0 表示不限制。 */
#ifndef CL_HOLD_CUT_LIMIT
#define CL_HOLD_CUT_LIMIT 1.0f
#endif

/* 爬升速度预判时间（秒）：目标 = target - LEAD*vz，用于抵消惯性冲高。 */
#ifndef CL_HEIGHT_VZ_LEAD_SEC
#define CL_HEIGHT_VZ_LEAD_SEC 0.0f
#endif

/* 保持型高度控制（CL_HEIGHT_BANG_BANG）参数：
 * 死区 ±DEADBAND 内冻结油门；低于目标按 CLIMB 斜率升、高于目标按 DESCEND 斜率降。 */
/* 只有高于目标这么多才允许主动下降。设大值等于"到高度就冻结、绝不主动降"，
 * 因为一旦开始下降就会进入自身下洗气流，油门加到顶也救不回。 */
/* 检测到下沉时的紧急补油斜率（远快于正常爬升），用于抢在下洗气流形成前把油门补上去。 */
/* 死区内的垂直速度阻尼：爬得快就减油、沉得快就加油，代替"死冻结油门"。 */
/* 爬升速度上限：超过就停止加油，避免"带速度冲高→在够不到的高度塌陷"，
 * 同时让光流在缓慢上升中保持可用的读数精度。 */
/* 互补滤波参数：K1 = 速度校正带宽(1/s)，K2 = 零偏学习速率，LIMIT = 速度限幅(m/s)。 */
/* 固定悬停前馈（占空比）。定义后交接时直接使用，不再按起飞速段缩放。 */
#ifndef CL_HOVER_FF_FIXED
#define CL_HOVER_FF_FIXED 0.82f
#endif

#ifndef CL_INS_K1
#define CL_INS_K1 3.0f
#endif
#ifndef CL_INS_K2
#define CL_INS_K2 0.6f
#endif
#ifndef CL_INS_VEL_LIMIT
#define CL_INS_VEL_LIMIT 1.5f
#endif
/* 光流用于校正时，要求垂直速度足够小（爬升时光流会因地面缩放而失真）。 */
#ifndef CL_INS_FLOW_VZ_MAX
#define CL_INS_FLOW_VZ_MAX 0.30f
#endif

/* 远高于目标（超过 OVERSHOOT）时允许把油门减到的最低值。 */
/* 高度控制用的 vz 低通时间常数（秒）。 */
#ifndef CL_HEIGHT_VZ_TAU
#define CL_HEIGHT_VZ_TAU 0.15f
#endif

/* 接近高度上限(米)时允许的油门下限，用于主动压回 100cm 以内。 */
#ifndef CL_HEIGHT_CEIL_M
#define CL_HEIGHT_CEIL_M 0.95f
#endif
#ifndef CL_HEIGHT_CEIL_FLOOR
#define CL_HEIGHT_CEIL_FLOOR 0.72f
#endif

#ifndef CL_HEIGHT_FAR_ABOVE_FLOOR
#define CL_HEIGHT_FAR_ABOVE_FLOOR 0.45f
#endif

#ifndef CL_HEIGHT_VEL_MAX_MPS
#define CL_HEIGHT_VEL_MAX_MPS 0.35f
#endif

#ifndef CL_HEIGHT_VZ_DAMP
#define CL_HEIGHT_VZ_DAMP 0.0f
#endif

#ifndef CL_HEIGHT_SINK_SLEW
#define CL_HEIGHT_SINK_SLEW 0.50f
#endif

#ifndef CL_HEIGHT_OVERSHOOT_M
#define CL_HEIGHT_OVERSHOOT_M CL_HEIGHT_DEADBAND_M
#endif

#ifndef CL_HEIGHT_DEADBAND_M
#define CL_HEIGHT_DEADBAND_M 0.04f
#endif
#ifndef CL_HEIGHT_CLIMB_SLEW
#define CL_HEIGHT_CLIMB_SLEW 0.40f
#endif
#ifndef CL_HEIGHT_DESCEND_SLEW
#define CL_HEIGHT_DESCEND_SLEW 0.30f
#endif

/* 推力上限诊断的油门斜率（每秒）。0.35 → 约 2.9 秒从 0 到 100%。 */
#ifndef CL_THRUST_RAMP_RATE
#define CL_THRUST_RAMP_RATE 0.35f
#endif

/* 重心偏置在线学习：KI 单位 1/s（姿态误差以度计），LIMIT 为差动上限。
 * KI=0 即关闭。典型 0.004：6° 误差每秒积累 24‰ 差动，2~3 秒收敛。 */
#ifndef CL_TRIM_LEARN_KI
#define CL_TRIM_LEARN_KI 0.0f
#endif
#ifndef CL_TRIM_LEARN_LIMIT
#define CL_TRIM_LEARN_LIMIT 0.10f
#endif

/* 起飞交接前馈缩放：实际记录的是起飞斜坡末段油门，高于真实悬停。
 * 1.0 = 原行为；<1 让高度环一开始就更接近悬停，减少冲高。 */
#ifndef CL_HOVER_FF_SCALE
#define CL_HOVER_FF_SCALE 1.0f
#endif

/* 光流刹车的偏航角速度门限（°/s）。2026-10-28 实飞：门限 15°/s 时 fg 全程为 0，
 * 先用 60°/s（≈关闭）验证是否门限所致，同时日志记录 gyro_z。 */
#ifndef CL_FLOW_YAW_GATE_DPS
#define CL_FLOW_YAW_GATE_DPS 15.0f
#endif

/* 光流开始参与控制的工作距离；只在 flow-assist 固件中生效。 */
#ifndef CL_FLOW_WORKING_RANGE_M
#define CL_FLOW_WORKING_RANGE_M .08f
#endif

/* 光流位置误差死区；过小会抖，过大则定点误差明显。 */
#ifndef CL_FLOW_POSITION_DEADBAND_M
#define CL_FLOW_POSITION_DEADBAND_M .015f
#endif

/* 光流速度误差转换为目标速度的比例；只在光流已标定后调整。 */
#ifndef CL_FLOW_VELOCITY_GAIN
#define CL_FLOW_VELOCITY_GAIN .8f
#endif

/* 光流目标速度限幅，防止横向控制动作过快。 */
#define CL_FLOW_VELOCITY_LIMIT_MPS .20f

/* 光流速度死区；小于此值不继续修正。 */
#define CL_FLOW_VELOCITY_DEADBAND_MPS .015f

/* 光流速度转换为姿态角目标的比例。 */
#ifndef CL_FLOW_ANGLE_GAIN_DEG_PER_MPS
#define CL_FLOW_ANGLE_GAIN_DEG_PER_MPS 5.0f
#endif

/* 光流横向姿态目标限幅。 */
/* 垂直速度门限(m/s)：超过它认为光流在爬升/下降中失真，暂停位置修正。 */
/* 光流整帧符号（+1 正常；-1 表示位置环整体反向，用于轴序未验证时的实验）。 */
#ifndef CL_FLOW_FRAME_SIGN
#define CL_FLOW_FRAME_SIGN 1.0f
#endif

#ifndef CL_FLOW_VZ_GATE_MPS
#define CL_FLOW_VZ_GATE_MPS 0.25f
#endif

#ifndef CL_FLOW_ANGLE_LIMIT_DEG
#define CL_FLOW_ANGLE_LIMIT_DEG 5.0f
#endif

/* 光流控制增益逐渐打开的速度，单位 1/s。 */
#define CL_FLOW_GAIN_RAMP_UP_PER_SEC 2.0f

/* 光流控制增益逐渐关闭的速度，单位 1/s。 */
#define CL_FLOW_GAIN_RAMP_DOWN_PER_SEC 4.0f

/* 光流丢失后等待多久才退出横向控制，单位 s。 */
#ifndef CL_FLOW_MISSING_TIMEOUT_SEC
#define CL_FLOW_MISSING_TIMEOUT_SEC .25f
#endif

/* 光流速度刹车增益；未完成光流标定前保持默认值。 */
#ifndef CL_FLOW_BRAKE_GAIN
#define CL_FLOW_BRAKE_GAIN 2.0f
#endif

/* 光流速度刹车死区。 */
#define CL_FLOW_BRAKE_DEADBAND_MPS .03f

/* 光流刹车产生的最大姿态角。 */
#ifndef CL_FLOW_BRAKE_ANGLE_LIMIT_DEG
#define CL_FLOW_BRAKE_ANGLE_LIMIT_DEG 1.0f
#endif

/* 起飞斜坡速度和起飞超时属于 profile 安全策略，不属于本轮 trim 调参。
 * 需要改变起飞策略时，单独建立新固件 profile 并重新做保护测试。 */

#endif
