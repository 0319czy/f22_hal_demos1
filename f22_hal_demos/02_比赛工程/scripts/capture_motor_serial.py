"""Bounded, read-only bench capture: PORT [SECONDS] [BAUD]."""
import sys
import time
from pathlib import Path

import serial

port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyACM0'
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 20
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
log_path = Path(__file__).resolve().parents[1] / 'docs/validation/standalone-2026-10-12/serial.log'
log_path.parent.mkdir(parents=True, exist_ok=True)
uart = serial.Serial(port=None, baudrate=baud, timeout=.2, exclusive=True)
uart.dtr = True
uart.rts = True
uart.port = port
with log_path.open('a', encoding='utf-8') as log:
    uart.open()
    try:
        end = time.monotonic() + duration
        while time.monotonic() < end:
            raw = uart.readline()
            if raw:
                line = time.strftime('%H:%M:%S ') + raw.decode('utf-8', errors='replace').rstrip()
                print(line, flush=True)
                log.write(line + '\n')
                log.flush()
    finally:
        uart.close()
