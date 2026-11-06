# 遥控与接收板通信测试（电机关闭）

本轮仅测试 TLE100 遥控 → E49 无线 → F22 接收板 → JP13 → 无线 DAP → 电脑。
F1～F4 都只是信号，没有启动、校准、起飞或停转动作。

当前接收固件 `contest_radio_latency_compact`：M1～M7 控制脚全部固定 LOW，不初始化
电机定时器、PWM、IMU、ToF 或闭环。USART1 9600 仅接收，JP13 USART2 115200
逐帧输出，按秒补充通信统计。

当前遥控固件 `tle100_latency_fast` 为独立诊断版本，原 `tle100` 默认固件保留。每约50ms
发送心跳，按键经两次扫描消抖后立即发事件。F1～F3 事件重复三包，F4 按住持续发。
F1/F2/F3/F4 的 cmd 分别为4/1/2/3；心跳为0。
为保证E49短包约20ms串口空闲，应用帧起点至少间隔35ms（12字节9600/8N1占12.5ms）。
按键边沿锁存到下一个允许发送的时刻，短按F4松开后也保留事件；心跳仍约50ms。遥控 USB 直接监听同一二进制包，
不向共享无线 UART 添加文字日志。

12字节诊断包：`A5 5A 02 seq低 seq高 cmd keys tx_ms[4字节小端] CRC8`。
CRC8 为多项式07、初值0，覆盖前11字节；keys 位0～3 对应 F1～F4。
`tx_ms` 为遥控设备时间，Timer2 每922个921600Hz计数递增，tick约1.000434ms。
接收程序也支持旧6字节v1包，旧包没有设备发送时间。

接收日志例：

```text
RX v=2 seq=42 cmd=4 keys=1 tx_ms=2100 rx_ms=8300 raw=... motors=0
STAT bytes=... frames=... reject=0 err=0 drop=0 tx_err=0 link=1 motors=0
```

`rx_ms` 取最后一个CRC字节的接收中断时间，避免串口打印阻塞改变接收时间。
`link` 为最近300ms是否收到有效遥控包，与无线DAP的配对状态无关。

## 当前稳定配置与修复

两端本地UART均9600，E49空中速率50kbps。每次启动在配置模式用C2应用临时参数、
读回确认；地址、信道与选项保持读取值，失败时显式回退并核对原参数。
目标SPED低6位为0x1D；常见完整参数为`C0 00 00 1D 2E 00`。
C2不改EEPROM，断电后应用在下次启动重新配置。配置文字在无线关闭的配置模式输出。

诊断接收器的分字节超时由30ms改为250ms，避免E49跨无线包拆分应用帧时被误丢。
这仅作用于诊断解析器，未修改原飞行控制器的保护超时。
JP13使用固定64字节ASCII记录，并带独立CRC8；电脑会拒绝损坏或截断的compact行。
64字节是UART记录长度，不代表USB一定用一个数据包回传。

主机脚本将`R`逐帧日志展开为上面的`RX`格式；`S`为累计统计，`D`为CRC/超时诊断。
直接用串口工具观察时会看到compact格式：

```text
R A55A02... <接收毫秒数十六进制> ... *HH
S <bytes> <frames> <reject> <err> <drop> <tx_err> <link> <motors> ... *HH
```

9600/RF50k上一轮180秒实测3598发送、3598完整匹配、零内部序号缺口，
CRC、分字节超时、UART错误、接收队列溢出和日志发送错误均为0。
[上一轮证据](validation/radio-stability-2026-10-13/phase2/result.json)。
同样RF50k但提高UART到57600出现缺字节与CRC失败，已退回9600，
`*_uart_fast`仅保留实验代码，不是当前烧录版本。

这套诊断协议提供序号、完整字节与CRC核对，没有无线ACK重传。
有限时间零缺包不代表任意距离或干扰下永不丢包；观察延迟仍包含JP13、DAP与USB。

## 双端采集

遥控USB为9600；无线DAP主机为115200。两个CH340可能具有相同by-id名称，
因此遥控优先使用核对过的 `/dev/serial/by-path`，不要仅凭ttyUSB编号自动烧录。
2026-10-14 这轮遥控使用USB物理口4，对应ttyUSB1；接收板板载USB为9.2口、
ttyUSB0；无线DAP主机814dc35a为ttyACM0，从机8120c32d为ttyACM1。
采集只打开遥控USB与DAP主机。tty编号可能变化，不能作为设备身份依据。
换插孔后须重新核对二进制发送帧和DAP序列号。

在比赛工程目录运行：

```bash
/home/czy/.platformio/penv/bin/python -u scripts/capture_radio_latency.py \
  --remote /dev/serial/by-path/pci-0000:00:14.0-usb-0:4:1.0-port0 \
  --receiver /dev/serial/by-id/usb-Horco_Horco_CMSIS-DAP_814dc35a-if02 \
  --seconds 30
```

看到 `READY` 后依次按 F1～F4，中间间隔约2秒。脚本只读，不向任一串口发送字节，
也不打开接收板板载CH340。当前遥控已在DTR/RTS均True下验证持续发包；换板时
需重新确认控制线电平。输出 events JSONL、匹配CSV和摘要JSON。

同一台电脑的monotonic时钟按完整原始包匹配发送和接收。`observed_delay_ms`
是电脑看到接收日志的时间减看到遥控完整发送包的时间，包含接收日志序列化、
无线DAP、USB缓冲及调度，并不等于严格的无线单程延迟。两块板开机时间不同，
**禁止用rx_ms直接减tx_ms**。负观察延迟保留报告，不隐去。

未匹配包还受采集起止、遥控重启、USB和DAP回传丢失影响，不能全部解释为无线丢包。
超过5秒的待匹配记录过期；检测遥控设备时间回退时清理待配记录，避免跨重启误配。

## 验证

```bash
pio run -e contest_radio_latency_compact
pio run -d remote/tle100 -e tle100_latency_fast
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -Isrc/flight tests/test_radio_trace.c src/flight/radio_trace.c src/flight/radio.c \
  -o /tmp/f22_radio_trace_test
/tmp/f22_radio_trace_test
python3 tests/test_capture_radio_latency.py
```

验收需要真实双端日志：帧原始字节一致、F1～F4分别出现4/1/2/3、心跳持续、
err/drop/tx_err均为0，并汇总观察延迟分位数。编译和合成测试不能替代真机证据。
两个profile均为诊断固件，不能把此版本当成电机遥控或飞行控制程序。

## 历史：2026-10-13 21:13 初始诊断结果（修复前）

两端已烧录并校验。接收板UID为 `30ffd90530574b3036751643`。本轮用57600和
`rts,-dtr,,,dtr,,,` 的已验证复位时序下载接收板；遥控原固件已备份后写入。
遥控USB0确实输出v2发送帧，DAP主机814dc35a在ACM1输出接收日志。

60秒采集：遥控发送1203帧，完整原始字节一致配对1064帧；F1/F2/F3/F4分别
配对2/6/4/43帧（包括按住及重按），cmd为4/1/2/3，心跳1009帧。
观察延迟min71.16ms、中位87.96ms、p95 104.54ms、max126.79ms；无负延迟。
遥控USB解析无坏帧或噪声，接收日志raw复核无坏帧；接收板err/drop/tx_err为0，
输出始终motors=0。

139个发送观测未配对，含采集边界和接收/回传缺帧，不能全部认定为RF丢包。
接收板reject累计仍增长，链路稳定性未通过。采集开头另有20帧DAP旧缓存，
会放大未过滤的RX序号跳跃；按与本轮TX完整raw匹配后的窗口统计，内部序号缺口135。

结论：**通信、时间戳和四键信号验证通过；稳定性仍有缺帧，需继续定位。**
证据：[结果摘要](validation/radio-latency-2026-10-13/latest_result.json)、
[逐包匹配CSV](validation/radio-latency-2026-10-13/matched-20261013-211308.csv)、
[双端原始事件](validation/radio-latency-2026-10-13/events-20261013-211308.jsonl)。

## 2026-10-14：35ms发送间隔修复前的5分钟复核

6001个发送包中6000个完整匹配，唯一损伤为F1序号13482；接收候选少了
发送时间最高字节00，CRC拒绝。同一次F1的重复包仍收到，三轮四键都有接收证据。
匹配数：心跳5870/5870、F1 8/9、F2 9/9、F3 9/9、F4 104/104。
观察延迟中位79.84ms、p95 119.06ms、最大161.80ms。
299个完整STAT区间内，板端接受增量5983与电脑有效RX数5983逐区间一致；
UART错误、队列溢出和回传发送错误均0。这支持损伤发生在接收解析前，不能归因于DAP漏日志。

坏帧起点比上一心跳仅晚20ms，E49短包可能合并排队；新增35ms间隔旨在隔离这个因素，
是待实测的修复假设，不把该时序关联当成已证实根因。原理图CH340 TX有隔离二极管D8，
目前没有证据把这次损伤归咎于USB空闲电平。
[该轮结果与失败证据](validation/radio-stability-2026-10-14/final/result.json)。

## 自动耐久测试

`tle100_latency_autotest`继承当前稳定profile，仅用合成按键替换GPIO读取：
每757ms按下一个键80ms，F1/F2/F3/F4循环，757ms与50ms心跳不同步以扫过不同相位。
它经过与手动相同的消抖、事件锁存、35ms最小发送间隔和实际无线发送；
电脑不会伪造RX记录，逐包核对真机发送与真实接收。该轮验证事件通信，不替代按钮触点测试。

```bash
pio run -d remote/tle100 -e tle100_latency_autotest
# 用现有ISP工具烧录后，运行上面的双端采集命令，--seconds 300
# 结束后恢复物理按键版：
pio run -d remote/tle100 -e tle100_latency_fast
```

恢复需实际烧录`tle100_latency_fast`，仅编译不会改变遥控器。
自动版不用于电机控制，接收板始终是通信诊断profile。

## 2026-10-14：最终自动测试结果

35ms发送间隔保护后，300秒真机自动测试：**6062发送、6062完整匹配，零内部序号缺口**。
396个完整合成按键时隙，F1/F2/F3/F4各99次，每次首个事件包均已匹配。
逐包计数：心跳5002、F1 297、F2 297、F3 297、F4 169，发送数与接收匹配数完全一致。
两条额外RX为采集起点前心跳，不属于本次发送窗口；末尾留1秒收完在途包。

实际发送起点间隔36～52ms，均不低于35ms。299个完整STAT区间内板端接受增量6043，
电脑有效RX也为6043，逐区间一致；CRC、分字节超时、UART错误、队列溢出、日志发送错误
与compact行损坏均0。gap_max35ms说明接收解析器仍需保留250ms分字节等待。

观察延迟：min54.17ms、中位80.56ms、p95 130.09ms、max162.60ms；无负值。
该数字包含JP13、无线DAP、USB缓冲与主机调度，无法承诺零延迟。
本轮有限测试零缺包不代表任意无线条件下永不丢包，也不能证明此前损伤的具体物理根因。

[最终验收数据](validation/radio-stability-2026-10-14/final-auto/result.json)。
接收固件继续保持电机全LOW；测试结束恢复手动`tle100_latency_fast`，保留35ms发送保护。

手动版已实际烧录并校验，恢复后10秒复核200包全部匹配，仅心跳cmd0，
自动按键信号已停止；接收板电机保持关闭。
[恢复复核](validation/radio-stability-2026-10-14/manual-restored/result.json)。
