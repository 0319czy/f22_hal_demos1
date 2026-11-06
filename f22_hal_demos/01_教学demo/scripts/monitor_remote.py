#!/usr/bin/env python3
"""Read USB remote debug text without sending bytes to the shared radio UART."""
import argparse
import re
import time
from pathlib import Path

import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", default="/dev/ttyUSB0")
parser.add_argument("--seconds", type=float, default=45)
parser.add_argument("--log", type=Path)
args = parser.parse_args()

uart = serial.Serial(port=None, baudrate=9600, timeout=0.2)
# Match the final levels used by ROM GO on the current board; do not pulse reset.
uart.dtr = True
uart.rts = True
uart.port = args.port
output = args.log.open("a", encoding="utf-8") if args.log else None
try:
    uart.open()
    deadline = time.monotonic() + args.seconds
    pending = bytearray()
    lines = 0
    while time.monotonic() < deadline:
        pending.extend(uart.read(uart.in_waiting or 1))
        while b"\n" in pending:
            raw, _, pending = pending.partition(b"\n")
            line = raw.decode("ascii", errors="replace").rstrip("\r")
            match = re.fullmatch(r"L([01]) C([0-4]) N(\d+) E(\d+)", line)
            if match:
                link, command, frames, errors = match.groups()
                name = ("心跳", "F2 起飞请求", "F3 降落请求", "F4 急停请求", "F1 校准请求")[int(command)]
                line = f"{'已连接' if link == '1' else '未连接'} | {'收到 ' + name if link == '1' else '无有效心跳'} | frames={frames} err={errors} | {line}"
            stamp = time.strftime("%H:%M:%S")
            record = f"{stamp} {line}"
            print(record, flush=True)
            if output:
                output.write(record + "\n")
                output.flush()
            lines += 1
    print(f"Captured {lines} lines; partial_bytes={len(pending)}", flush=True)
finally:
    uart.close()
    if output:
        output.close()
