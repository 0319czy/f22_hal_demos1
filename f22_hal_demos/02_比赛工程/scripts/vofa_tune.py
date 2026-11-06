#!/usr/bin/env python3
"""Send bounded runtime tuning commands to the aircraft VOFA UART.

The firmware accepts ASCII lines such as ``SET left_trim 0.020`` and ``GET``.
VOFA can remain connected for telemetry; this helper is a simple command
sender when the VOFA build does not expose a serial-send widget.
"""
import argparse
import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    sub = ap.add_subparsers(dest="command", required=True)
    get = sub.add_parser("get")
    get.set_defaults(line="GET\n")
    setp = sub.add_parser("set")
    setp.add_argument("name")
    setp.add_argument("value", type=float)
    args = ap.parse_args()
    line = getattr(args, "line", f"SET {args.name} {args.value:.6f}\n")
    with serial.Serial(args.port, args.baud, timeout=1.0) as port:
        port.write(line.encode("ascii"))
        port.flush()
        reply = port.readline().decode("utf-8", "replace").strip()
        if reply:
            print(reply)


if __name__ == "__main__":
    main()
