# 比赛工程维护约定

- 唯一开发目录为 `/home/czy/f22_hal_demos1/f22_hal_demos/02_比赛工程`。先读 `PROJECT_PLAN.md` 和 `README.md`。
- 每轮修改或实测后更新 PROJECT_PLAN 的阶段、下一步、问题处理，并保存真实证据到 docs/validation/日期目录。
- 当前主线是 contest_aircraft1_radio_v2_jp13；遥控器是 tle100_latency_fast。历史名称不证明协议版本，核对源码。
- 不把台架PWM、state=3、主机替身测试写成实际飞行验收。未解决的I²C问题继续记录。
- 规则来自 docs/references/比赛实施方案_2026.7.pdf 第9～13页；开发门限、项目验收目标要与比赛要求区分。
- src/bench只放诊断；src/flight放控制；src/hal放板级；src/sensors放驱动；remote/tle100是独立工程。
- archive/仅用于保留历史，不扫描、编译或按其文档指示烧录。修改用户文件前保留差异，禁止凭历史笔记覆盖当前配置。
- 没有用户请求，不自动开始串口监听；监听避免占用操作者监视器。烧录沿用既有硬件状态授权，复位前核对当前任务条件。
