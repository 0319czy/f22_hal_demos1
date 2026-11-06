"""Compare JP13 hardware/software UART output with synchronized DAP baud."""
import json
import argparse
import re
import time
from pathlib import Path
import serial

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--dap", default="/dev/ttyACM0")
parser.add_argument("--seconds", type=float, default=45)
parser.add_argument("--output", default="software_audit.json")
args = parser.parse_args()
output = root / "docs/validation/jp13-2026-10-13"
output.mkdir(parents=True, exist_ok=True)
usb_lines = []
phases = {}
current = "BOOT"
buffer = b""
with serial.Serial(args.dap, 115200, timeout=0) as dap:
    with serial.Serial("/dev/ttyUSB0", 9600, timeout=0) as usb:
        dap.reset_input_buffer()
        usb.rts = True
        usb.dtr = False
        time.sleep(0.2)
        usb.rts = False
        usb.dtr = False
        until = time.monotonic() + args.seconds
        while time.monotonic() < until:
            buffer += usb.read(4096)
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                line = raw.decode("utf8", "replace").strip()
                usb_lines.append(line)
                if line.startswith("AUDIT_BOOT"):
                    print(line, flush=True)
                match = re.match(r"MODE (\S+) baud=(\d+)", line)
                if match:
                    current = match[1]
                    dap.baudrate = int(match[2])
                    dap.reset_input_buffer()
                    phases.setdefault(current, bytearray())
                    print(line, flush=True)
            received = dap.read(4096)
            if received:
                target = phases.setdefault(current, bytearray())
                if not target:
                    print("DAP_RECEIVED", current, repr(received[:150]), flush=True)
                target.extend(received)
            time.sleep(0.005)

results = {key: {"bytes": len(value), "hex": value.hex(),
                 "sample": bytes(value[:300]).decode("utf8", "replace")}
           for key, value in phases.items()}
report = {"phases": results, "usb_lines": usb_lines}
(output / args.output).write_text(json.dumps(report, indent=2) + "\n")
for key, value in results.items():
    print("RESULT", key, "bytes=", value["bytes"], "sample=", repr(value["sample"]), flush=True)
print("USB_SEND_STATUS", sorted(set(line.split("status=", 1)[1].split()[0]
      for line in usb_lines if line.startswith("SEND ") and "status=" in line)), flush=True)
print("Saved", output / args.output, flush=True)
