# 遥控发射端 demo：TLE100 / AT89S52

此目录是**遥控器固件**，不是F22固件。按照本地《TLE100可编程遥控装置用户手册》主控AT89S52、11.0592MHz及按键引脚编写；SDCC已编译出 [tle100.hex](tle100.hex)。新任务协议须配套飞控 `contest_hover`，不能和原厂F/B/L/R等字符版混用。

| 按键 | 引脚 | 新任务动作 |
|---|---|---|
| F1 | P0.6，低有效 | 校准请求，仅空闲时应允许 |
| F2 | P0.7，低有效 | 一键起飞请求 |
| F3 | P1.3，低有效 | 一键降落请求 |
| F4 | P1.4，低有效 | 急停请求，按住持续重复 |

方向与加减速键不参与自主任务。P1.2/P1.1分别为手册的WL_M0/WL_M1，本例均置0透传；原厂代码名字存在交叉，但两脚都0时行为一致。引脚P3.1/P3.0为UART发/收。

## 编译与下载

```bash
bash scripts/build_remote.sh
```

脚本使用SDCC mcs51，small模型，8KB程序空间、256B内部RAM、不使用外部RAM。本次SDCC4.5.0产物代码583字节，RAM/链接记录在 `build/tle100.mem`；机器上的编译器临时解包在`/tmp/f22-sdcc`，重启清理后若未安装SDCC，需要安装编译器才能重建。已生成的HEX不依赖临时编译器。

### 用 PlatformIO 烧录配套遥控器

2026-10-07 已在当前遥控器实测：配套烧录器 USB ID 为 `03eb:c8b4`，名称为 `zhifengsoft USBHID`；ProgISP 识别 `ZF-008 HV Programmer Ver:1.6`，读取 AT89S52 签名成功，执行擦除、空白检查、写入及 FLASH 校验成功。PIO 返回 SUCCESS。此烧录器使用原厂 HID 协议，需要调用配套 ProgISP；普通 USBasp 的 AVRDUDE 命令无法直接替代。

在项目根目录执行（遥控器通过 ISP 排线连接烧录器）：

```bash
pio run -d 01_教学demo/remote/tle100 -e tle100 -t upload
# 比赛工程的对应入口：
pio run -d 02_比赛工程/remote/tle100 -e tle100 -t upload
```

配置采用 `intel_mcs51@2.2.0`、`board = AT89S52`。PIO 编译本次程序为 531 字节；上传通过 `tools/upload_progisp.py` 调用原厂 ProgISP 1.72。脚本先读取签名、备份完整 8192 字节，再载入本次 HEX、验证芯片签名、擦除、检查空白、写入及校验，不修改锁位或熔丝。仅出现完整成功结果才返回 0；失败会让 PIO 返回非零。

Linux 依赖：`wine64`、`xvfb`、`xdotool`、`gcc-mingw-w64-x86-64`。当前电脑已经具备这些工具。默认从仓库本地的 `竞赛相关技术资料/软件工具/烧录软件progisp1.72` 找配套软件；该资料目录没有提交到 Git；本地生成的教学资料 ZIP 会把配套软件放入 `remote/tle100/vendor`，上传脚本优先使用该目录。直接克隆源码换电脑时需要复制完整 ProgISP 目录，并可用 `PROGISP_DIR=/实际目录 pio run ...` 指定，不能只复制 exe。

USB HID 权限规则（当前电脑已安装）：

```bash
sudo install -m 644 01_教学demo/remote/tle100/tools/99-tle100-progisp.rules /etc/udev/rules.d/99-tle100-progisp.rules
sudo udevadm control --reload-rules
# 重新插拔烧录器；当前用户须属于 plugdev 组。
```

原固件备份保存在 `~/.cache/f22-progisp-pio/original-时间.bin`；成功后的记录在 `~/.cache/f22-progisp-pio/last-upload.json`。最初实机备份为 `original-20261007-202129.bin`，8192 字节。脚本使用独立虚拟显示器和专用 Wine 前缀，会关闭此前该专用前缀的 ProgISP 实例。坐标桥接适配配套 1.72 英文界面；改用其他版本需要重新验证，不能依据 GUI 打开或 PIO 编译成功就认定下载成功。

接口虽称2×10PIN牛角座，手册列出的ISP信号是1 MOSI、2 5V、3 NC、4 GND、5 RST、6 GND、7 SCK、8 GND、9 MISO、10 GND。实际排座形状和排线须核对1脚与上述信号，不能凭USBASP另一款插头脚数假设直插兼容；供电方式按下载器说明和遥控器手册确定。

验收：先不接动力系统，只让F22接收日志；空闲仍有link=1；依次F1/F2/F3/F4出现4/1/2/3；上电按住F2不触发START；按住STOP持续有请求；遥控器关机约300ms后link=0。参见 [协议](../../docs/RADIO_PROTOCOL.md) 和 [比赛接收与串口配置](../../README.md#无线与串口)。

当前已完成遥控器实机烧录和 FLASH 校验；无线通信、飞控接收与按键时序仍需联调验收。本例不等同于已完成真机一键起飞。
