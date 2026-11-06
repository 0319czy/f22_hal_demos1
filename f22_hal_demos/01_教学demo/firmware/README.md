# 教学固件快照

这些BIN来自本教学项目各独立环境的已验证构建，校验见 [manifest.json](manifest.json)。修改源码后应重新编译，再运行`scripts/package_teaching.py`更新包。

`demo03_motor_bench.bin`会在规定按键门控后输出单电机PWM，仅供拆桨台架；`demo_selftest.bin`只读 EEPROM 且不包含电机 PWM。其他数值教学环境不启动电机PWM。每次下载会替换板上程序，按对应课教程选固件。遥控器不能用这些BIN，它有`remote/tle100/platformio.ini`独立AT89S52环境；配套HID烧录器通过PIO调用ProgISP上传。USB识别与接线须先确认，详见遥控README。

可在本项目使用`pio run -e <环境> -t upload`；默认ISP序列在旧板实测成功，2026-10-09两块板采用另一电平并通过ROM GO运行，不能假定所有板的启动电路一致。具体见`docs/BOARD_CHECK_2026-10-09.md`。没有自动下载动作。教学软件模型和控制器预览不是真机参赛固件。
