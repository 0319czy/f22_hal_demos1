#!/usr/bin/env python3
"""Show received key presses and ToF distance from the DAP serial port."""
import argparse
import re
import sys
import time
from datetime import datetime
from pathlib import Path

from capture_radio_latency import RX_PATTERN, decode_hex_frame, parse_receiver_line


def format_height(raw):
    line = raw.decode("utf-8", errors="replace").strip()
    if not re.match(r"^\[CL(?: LOCAL)?\] ", line):
        return None
    fields = dict(re.findall(r"\b(tof|valid|mm)=(\d+)\b", line))
    if not {"tof", "valid", "mm"} <= fields.keys():
        return None
    if fields["tof"] != "1":
        return "测距高度：无效（ToF未识别）"
    centimeters = int(fields["mm"]) / 10
    if fields["valid"] != "1":
        return f"测距高度：无效（原始读数 {centimeters:.1f} cm）"
    return f"测距高度：{centimeters:.1f} cm"


class KeyDisplay:
    def __init__(self):
        self.previous_keys = 0
        self.previous_command = 0
        self.event_token = None

    def feed(self, raw):
        # New closed-loop firmware emits an asynchronous notice per key event.
        notice = re.fullmatch(rb"KEY F([1-4]) event=\d+ seq=\d+ allowed=[01] ready=[01]", raw.strip())
        if notice:
            return [f"已按下 F{notice.group(1).decode('ascii')}"]
        line = parse_receiver_line(raw.rstrip(b"\n"))
        match = RX_PATTERN.fullmatch(line) if line else None
        if not match:
            return []
        event = decode_hex_frame(match.group(7))
        if event is None:
            return []
        command = event["command"]
        command_key = {4: 0, 1: 1, 2: 2, 3: 3}.get(command)
        if event['version'] == 3:
            if command_key is None or event['event_token'] == self.event_token:
                return []
            self.event_token = event['event_token']
            return [f"已按下 F{command_key + 1}"]
        messages = []
        if event["keys"] is not None:
            keys = event["keys"]
            pressed = keys & ~self.previous_keys
            messages = [f"已按下 F{bit + 1}" for bit in range(4) if pressed & (1 << bit)]
            # A latched short press may be transmitted after its release.
            if (command_key is not None and command != self.previous_command
                    and not (keys | self.previous_keys) & (1 << command_key)):
                messages.append(f"已按下 F{command_key + 1}")
            self.previous_keys = keys
        elif command_key is not None and command != self.previous_command:
            messages.append(f"已按下 F{command_key + 1}")
        self.previous_command = command
        return messages


class StatusPanel:
    """Append-only Chinese log; keep control telemetry separate from display policy."""
    states = {"0": "待机", "1": "起转", "2": "上升", "3": "悬停", "4": "降落", "5": "故障"}
    faults = dict(enumerate(("无", "轴向未验证", "输入无效", "遥控掉线", "循环超时",
                            "倾角过大", "测距丢失", "起飞超时", "启动条件不满足",
                            "输出保护", "降落超时", "IMU不可用", "高度无效",
                            "光流未标定", "光流不可用", "起飞禁止")))

    def __init__(self, output, interval=.5, logfile=None):
        self.output = output
        self.interval = interval
        self.logfile = logfile
        self.fields = {}
        self.keys = KeyDisplay()
        self.events = []
        self.last_paint = 0
        self.last_condition = None

    def start(self):
        self.emit("时间      状态 / 传感器 / 电机输出（Ctrl+C 退出，可向上翻看）\n")

    def close(self):
        pass

    def emit(self, text):
        # Append only: preserve terminal scrollback and the readable transcript.
        if self.logfile:
            self.logfile.write(text)
            self.logfile.flush()
        self.output.write(text)
        self.output.flush()

    def log_rows(self):
        f = self.fields
        rows = self.rows()
        pwm = f.get("PWM_permille_M1_M2_M3_M6", "").split(",")
        try:
            motors = "  ".join(f"{name}={int(v)/10:5.1f}%" for name, v in
                               zip(("右前M1", "左前M2", "右后M3", "左后M6"), pwm)) if len(pwm) == 4 else "等待数据"
        except ValueError:
            motors = "数据格式错误"
        return [
            rows[1] + " | 遥控=" + ("在线" if f.get('link') == '1' else '离线'),
            rows[3] + " | " + rows[4],
            "PWM ：" + motors,
            rows[8] + " | " + rows[9],
            *( [f"ToF ：状态={f.get('tof_status', '--')}  数据年龄={f.get('tof_age_ms', '--')}ms  在线={f.get('tof_live', '--')}"]
                if 'tof_status' in f else [] ),
        ]

    def rows(self):
        f = self.fields
        def number(key, scale=1, unit=""):
            try:
                return f"{float(f[key]) / scale:.1f}{unit}"
            except (KeyError, ValueError):
                return "--"
        fault = f.get("fault", "?")
        try:
            reason = self.faults.get(int(fault), "未知故障")
        except ValueError:
            reason = "等待数据"
        height = number("mm", 10, "cm")
        if f.get("tof") == "0":
            height = "未识别"
        elif f.get("valid") == "0":
            height += "（无效原始读数）"
        flow = "未识别" if f.get("flow_present") == "0" else (
            "有效" if f.get("flow") == "1" else "无效")
        modes = {"0": "待机", "1": "近地：IMU稳姿", "2": "等待可信光流",
                 "3": "位置保持渐入", "4": "位置保持", "5": "光流失效降落", "6": "着地减推力"}
        pwm = f.get("PWM_permille_M1_M2_M3_M6", "").split(",")
        try:
            pwm_text = " / ".join(f"{int(v) / 10:5.1f}%" for v in pwm) if len(pwm) == 4 else "--"
        except ValueError:
            pwm_text = "--"
        output = {"0": "已启用", "1": "锁止"}.get(f.get("output_locked"), "--")
        model = "试用比例，未完成标定" if f.get("flow_model") == "provisional" else "--"
        fused = number('height_mm', 10, 'cm') if f.get('height_valid') == '1' else '无效'
        horizontal = modes.get(f.get('flow_mode'), '--')
        if f.get('state') in ('0', '5'):
            horizontal = '待机，未运行' if f.get('state') == '0' else '故障，已停止'
        return [
            "F22 飞行状态  |  Ctrl+C 退出",
            f"状态：{self.states.get(f.get('state'), '等待数据')}    故障：{fault} {reason}",
            f"遥控：{'已连接' if f.get('link') == '1' else '未连接'}    收包就绪：{f.get('ready', '--')}    电机输出：{output}",
            f"测距：{height}    融合高度：{fused}",
            f"横滚：{number('roll_cd', 100, '°')}    俯仰：{number('pitch_cd', 100, '°')}",
            f"IMU：{f.get('imu', '--')}    校准：{f.get('calibrated', '--')}    启动姿态：{f.get('armlevel', '--')}",
            "PWM 顺序：右前M1 / 左前M2 / 右后M3 / 左后M6",
            f"PWM：{pwm_text}",
            f"光流：{flow}    质量：{f.get('squal', '--')}    错误：{f.get('flow_err', '--')}",
            f"横向：{horizontal}    参与：{number('flow_gain', 10, '%')}",
            f"光流标定：{model}",
            "最近事件：",
            *(self.events[-4:] or ["  等待按键或故障变化"]),
        ]

    def feed(self, raw):
        line = raw.decode("utf-8", errors="replace").strip()
        notices = self.keys.feed(raw)
        now = time.monotonic()
        changed = False
        if re.match(r"^\[CL(?: LOCAL)?\] ", line):
            fields = dict(re.findall(r"\b(\w+)=([^\s]+)", line))
            if not {"state", "fault", "PWM_permille_M1_M2_M3_M6"} <= fields.keys():
                self.emit(time.strftime("%H:%M:%S ") + "串口记录不完整，原文保留在完整日志\n")
                return
            self.fields = fields
            condition = tuple(self.fields.get(k) for k in ("state", "fault", "link"))
            changed = condition != self.last_condition
            self.last_condition = condition
        elif line.startswith("[FAULT] "):
            match = re.search(r"\bcode=(\d+)", line)
            if match:
                code = int(match[1])
                notices.append(f"故障 {code}：{self.faults.get(code, '未知故障')}")
        elif not notices:
            return
        stamped = [time.strftime("%H:%M:%S ") + x for x in notices]
        self.events = (self.events + stamped)[-4:]
        for message in stamped:
            self.emit(message + "\n")
        if not re.match(r"^\[CL(?: LOCAL)?\] ", line):
            return
        if not changed and now - self.last_paint < self.interval:
            return
        self.last_paint = now
        rows = self.log_rows()
        self.emit(time.strftime("%H:%M:%S ") + rows[0] + "\n" +
                  "\n".join("         " + row for row in rows[1:]) + "\n\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/serial/by-id/usb-Horco_Horco_CMSIS-DAP_814dc35a-if02")
    parser.add_argument("--stdin", action="store_true", help="Read saved receiver logs instead of serial")
    parser.add_argument("--full", action="store_true", help="Also show closed-loop state and sensor telemetry")
    parser.add_argument("--keys-only", action="store_true", help="Hide distance and show key presses only")
    parser.add_argument("--pretty", action="store_true", help="Show tidy scrolling Chinese logs and save raw/readable session logs")
    parser.add_argument("--interval", type=float, default=.5, help="Pretty status interval in seconds; 0 shows every sample (all raw data is saved)")
    parser.add_argument("--log-dir", type=Path, default=Path(__file__).resolve().parents[1] / "logs" / "serial", help="Directory for saved raw and readable logs")
    args = parser.parse_args()
    if args.pretty and (args.full or args.keys_only):
        parser.error("--pretty 不能与 --full 或 --keys-only 同时使用")
    if not 0 <= args.interval < float('inf'):
        parser.error("--interval 必须是有限非负数")
    display = KeyDisplay()
    raw_log = pretty_log = None
    panel = None

    def show(raw):
        if panel:
            panel.feed(raw)
            return
        line = raw.decode("utf-8", errors="replace").strip()
        if not args.full and line.startswith(("[FAULT] ", "[CAL] ")):
            # Faults contain the trigger-time IMU/arming snapshot, not just the
            # later periodic fault code. Keep this evidence in concise mode.
            print(line, flush=True)
        if args.full:
            if line:
                print(line, flush=True)
        else:
            for message in display.feed(raw):
                print(message, flush=True)
        if not args.keys_only:
            if line.startswith("[FCAL] "):
                fields = dict(re.findall(r"\b(\w+)=([^ ]+)", line))
                needed = {"mm", "valid", "n", "raw_n", "dxsum", "dysum", "q", "output_locked"}
                if needed <= fields.keys():
                    try:
                        distance = f"{int(fields['mm']) / 10:.1f}cm"
                        if fields["valid"] != "1":
                            distance += "（近距或无效）"
                        print(f"光流标定 | 测距={distance} 质量={int(fields['q'])} "
                              f"原始采样={int(fields['raw_n'])} 有效累计={int(fields['n'])} "
                              f"计数dx/dy={int(fields['dxsum'])},{int(fields['dysum'])} "
                              f"电机锁止={fields['output_locked']}", flush=True)
                    except ValueError:
                        pass  # incomplete/corrupt telemetry remains in --full output
                return
            height = format_height(raw)
            if height:
                if re.search(rb"\bbench=attitude20\b", raw):
                    height += " | 模式=拆桨姿态差速"
                    radio_fields = dict(re.findall(r"\b(link|ready)=(\d+)\b", line))
                    if {"link", "ready"} <= radio_fields.keys():
                        height += (f" 遥控={radio_fields['link']}"
                                   f" 就绪={radio_fields['ready']}")
                flight_mode = re.search(rb"\bflight=tether(90|100)(?:_(?:noflow|flow|softland))?\b", raw)
                if flight_mode:
                    ceiling = flight_mode.group(1).decode("ascii")
                    if re.search(rb"\block_reason=incident\b", raw):
                        height += " | 模式=事故复查（电机锁止，禁止试飞）"
                    elif re.search(rb"\boutput_locked=1\b", raw):
                        height += " | 模式=光流识别诊断（电机锁止）"
                    else:
                        height += f" | 模式=系绳起飞（上限{ceiling}%）"
                    if re.search(rb"\bkeymap=F4land_F3stop\b", raw):
                        height += " F4=缓降 F3=急停"
                    flight_fields = dict(re.findall(r"\b(link|ready|u_permille|ref_permille|target_mm)=(\d+)\b", line))
                    state_match = re.search(r"\bstate=(\d+)\b", line)
                    flight_state = int(state_match.group(1)) if state_match else None
                    if {"link", "ready"} <= flight_fields.keys():
                        height += f" 遥控={flight_fields['link']} 就绪={flight_fields['ready']}"
                    if flight_state == 1:
                        height += " 定高=未启用 离地推力参考=尚未捕获"
                    if flight_state in (2, 3) and "ref_permille" in flight_fields:
                        height += f" 推力参考={int(flight_fields['ref_permille']) / 10:.1f}%"
                    if flight_state in (2, 3, 4) and "target_mm" in flight_fields:
                        height += f" 目标测距={int(flight_fields['target_mm']) / 10:.1f}cm"
                    flow_fields = dict(re.findall(r"\b(flow|flow_present|axes|flow_req|flow_mode|flow_gain|flow_land|flow_hold)=(\d+)\b", line))
                    if flow_fields.get("flow_present") == "0":
                        height += " 光流=未识别"
                    elif flow_fields.get("flow") == "0":
                        height += " 光流=数据无效"
                    elif flow_fields.get("flow") == "1":
                        height += " 光流=数据有效"
                    if flow_fields.get("flow_req") == "1":
                        modes = {"0": "待启动", "1": "IMU稳姿（近地光流不可用）", "2": "IMU稳姿（等待可信光流）",
                                 "3": "IMU稳姿+相对位置保持渐入", "4": "IMU稳姿+光流相对位置保持", "5": "IMU稳姿（光流失效降落）",
                                 "6": "IMU稳姿（着地减推力）"}
                        if flow_fields.get("axes") == "0":
                            height += " 横向闭环=待标定（起飞锁定）"
                        else:
                            height += f" 横向控制={modes.get(flow_fields.get('flow_mode'), '状态未知')}"
                            if flow_fields.get("flow_mode") == "3" and "flow_gain" in flow_fields:
                                height += f"({int(flow_fields['flow_gain']) / 10:.0f}%)"
                            if flow_fields.get("flow_hold") == "1":
                                errors = dict(re.findall(r"\b(flow_err_fwd_mm|flow_err_right_mm)=(-?\d+)\b", line))
                                if len(errors) == 2:
                                    height += (f" 位置误差前/右={int(errors['flow_err_fwd_mm']) / 10:.1f}/"
                                               f"{int(errors['flow_err_right_mm']) / 10:.1f}cm")
                        if flow_fields.get("flow_land") == "1":
                            height += " 光流丢失=已请求降落"
                    elif flow_fields.get("axes") == "0":
                        height += " 横向闭环=未启用"
                    diagnostics = dict(re.findall(r"\b(flow_fault|flow_reg|flow_io|flow_id|flow_inv|flow_reads|flow_init|flow_attempts)=(-?\d+)\b", line))
                    if flow_fields.get("flow_present") == "0" and diagnostics:
                        if diagnostics.get("flow_init") == "1":
                            height += " 光流初始化中"
                        else:
                            reasons = {"1": "接口配置", "2": "复位写失败", "4": "ID读取失败",
                                       "5": "反码读取失败", "6": "ID不匹配", "7": "清状态读取失败",
                                       "9": "配置写失败", "11": "运动数据读取失败",
                                       "12": "复核ID读取失败", "13": "复核反码读取失败",
                                       "14": "寄存器页选择失败"}
                            height += f" 诊断={reasons.get(diagnostics.get('flow_fault'), '等待诊断')}"
                        read_mask = int(diagnostics.get("flow_reads", "0"))
                        chip_id = f"0x{int(diagnostics['flow_id']):02X}" if read_mask & 1 and "flow_id" in diagnostics else "未读到"
                        inverse = f"0x{int(diagnostics['flow_inv']):02X}" if read_mask & 2 and "flow_inv" in diagnostics else "未读到"
                        height += f" ID={chip_id}/{inverse} IO={diagnostics.get('flow_io', '?')}"
                # Keep the bench acceptance fields visible in the concise view.
                line = raw.decode("utf-8", errors="replace")
                fields = dict(re.findall(r"\b(state|fault|PWM_permille_M1_M2_M3_M6)=([^\s]+)", line))
                if {"state", "fault", "PWM_permille_M1_M2_M3_M6"} <= fields.keys():
                    height += (f" | state={fields['state']} fault={fields['fault']}"
                               f" PWM(M1/M2/M3/M6)={fields['PWM_permille_M1_M2_M3_M6']}")
                imu_fields = dict(re.findall(r"\b(imu|armlevel|calibrated|roll_cd|pitch_cd)=(-?\d+)\b", line))
                if "imu" in imu_fields:
                    height += f" | IMU={imu_fields['imu']}"
                if "calibrated" in imu_fields:
                    height += f" 校准={imu_fields['calibrated']}"
                if "armlevel" in imu_fields:
                    height += f" 启动姿态={imu_fields['armlevel']}"
                if "roll_cd" in imu_fields and "pitch_cd" in imu_fields:
                    height += (f" 横滚={int(imu_fields['roll_cd']) / 100:.2f}°"
                               f" 俯仰={int(imu_fields['pitch_cd']) / 100:.2f}°")
                print(height, flush=True)

    try:
        if args.pretty:
            if not args.stdin:
                folder = args.log_dir
                folder.mkdir(parents=True, exist_ok=True)
                name = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
                raw_log = (folder / (name + ".raw.log")).open("xb")
                pretty_log = (folder / (name + ".pretty.log")).open("x", encoding="utf8")
            panel = StatusPanel(sys.stdout, args.interval, pretty_log)
        if panel:
            panel.start()
            if raw_log:
                panel.emit(f"完整原始记录：{raw_log.name}\n中文记录：{pretty_log.name}\n")
        if args.stdin:
            for raw in sys.stdin.buffer:
                show(raw)
            return
        import serial

        uart = serial.Serial(port=None, baudrate=115200, timeout=.2, exclusive=True)
        # Set the proven DAP control-line levels before opening; send no bytes.
        uart.dtr = uart.rts = True
        uart.port = args.port
        try:
            uart.open()
            buffer = bytearray()
            while True:
                received = uart.read(uart.in_waiting or 1)
                if raw_log:
                    raw_log.write(received)
                    raw_log.flush()
                buffer.extend(received)
                # A read may end mid-record; retain it until its newline arrives.
                while b"\n" in buffer:
                    raw, _, buffer = buffer.partition(b"\n")
                    show(raw)
                if len(buffer) > 4096:
                    buffer.clear()
        except serial.SerialException as error:
            if panel:
                panel.emit(time.strftime("%H:%M:%S ") + f"串口断开/不可用：{error}\n")
            print(f"串口 {args.port}：{error}", file=sys.stderr)
            raise SystemExit(1)
        finally:
            uart.close()
    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        if panel:
            panel.close()
        if raw_log:
            raw_log.close()
        if pretty_log:
            pretty_log.close()


if __name__ == "__main__":
    main()
