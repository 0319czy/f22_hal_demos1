#!/usr/bin/env python3
import sys
import serial

if len(sys.argv) != 3:
    raise SystemExit('用法: python3 scripts/tune.py 参数 值')
name, value = sys.argv[1:]
with serial.Serial('/dev/ttyACM0', 115200, timeout=1.0) as port:
    port.write(f'SET {name} {float(value):.6f}\n'.encode())
    port.flush()
    print(port.readline().decode('utf-8', 'replace').strip())
