#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_test_dir=$(mktemp -d)
trap 'rm -rf "$task_test_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_task_input.c src/flight/task_input.c src/flight/radio.c \
    src/flight/hover.c src/flight/pid.c -lm -o "$task_test_dir/task_input"
"$task_test_dir/task_input"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_imu_calibration.c src/flight/imu_calibration.c \
    -lm -o "$task_test_dir/imu_calibration"
"$task_test_dir/imu_calibration" docs/validation/bench-2026-10-10/imu-competition-samples.csv
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight -Isrc/bench tests/test_motor_bench.c src/bench/motor_bench.c \
    src/flight/radio.c -o "$task_test_dir/motor_bench"
"$task_test_dir/motor_bench"
python3 - <<'PY'
import csv
with open('vofa.csv', newline='') as handle:
    rows=list(csv.reader(handle))
assert len(rows)==2 and len(rows[0])==len(rows[1])==32
assert all(float(value)==0 for value in rows[1])
print('Competition CSV: 32 columns, all initial values zero')
with open('imu_check.csv', newline='') as handle:
    rows=list(csv.reader(handle))
assert len(rows)==2 and len(rows[0])==len(rows[1])==16
assert all(float(value)==0 for value in rows[1])
print('IMU check CSV: 16 columns, all initial values zero')
with open('motor_check.csv', newline='') as handle:
    rows=list(csv.reader(handle))
assert len(rows)==2 and len(rows[0])==len(rows[1])==9
assert all(float(value)==0 for value in rows[1])
print('Motor check CSV: 9 columns, all initial values zero')
PY
python3 scripts/check_docs.py
pio run -e contest_hover -e contest_imu_check -e contest_motor_preview \
    -e contest_motor_m1 -e contest_motor_m2 -e contest_motor_m3 -e contest_motor_m4 \
    -e contest_motor_m3_direct -e contest_motor_quad_direct -e contest_motor_quad_diag
for env in contest_hover contest_imu_check contest_motor_preview; do
    if arm-none-eabi-nm ".pio/build/$env/firmware.elf" | \
       grep -Eq 'Board_MotorPwm|HAL_TIM_PWM_Start'; then
        echo "$env unexpectedly links physical motor PWM code" >&2
        exit 1
    fi
done
for env in contest_motor_m1 contest_motor_m2 contest_motor_m3 contest_motor_m4; do
    if ! arm-none-eabi-nm ".pio/build/$env/firmware.elf" | \
         grep -Eq 'Board_MotorPwm|HAL_TIM_PWM_Start'; then
        echo "$env lacks expected PWM driver" >&2
        exit 1
    fi
done
for env in contest_hover contest_imu_check contest_motor_preview \
           contest_motor_m1 contest_motor_m2 contest_motor_m3 contest_motor_m4 \
           contest_motor_m3_direct contest_motor_quad_direct contest_motor_quad_diag; do
    cmp ".pio/build/$env/firmware.bin" "firmware/$env.bin"
done
# This GPIO entry is intentionally physical despite having no PWM symbols.
for env in contest_motor_m3_direct contest_motor_quad_direct; do
    if arm-none-eabi-nm ".pio/build/$env/firmware.elf" | \
       grep -Eq 'Radio_|MotorBench_|Board_MotorPwm|HAL_TIM_PWM_Start|MX_USART'; then
        echo "$env unexpectedly links radio/PWM/UART initialization" >&2
        exit 1
    fi
done
arm-none-eabi-nm .pio/build/contest_motor_quad_diag/firmware.elf > "$task_test_dir/diag_symbols"
for symbol in MX_USART1_UART_Init MX_USART2_UART_Init; do
    if ! grep -Eq "[[:space:]]$symbol$" "$task_test_dir/diag_symbols"; then
        echo "Motor diagnostic entry lacks $symbol" >&2
        exit 1
    fi
done
if grep -Eq 'Radio_|MotorBench_|Board_MotorPwm|HAL_TIM_PWM_Start' "$task_test_dir/diag_symbols"; then
    echo 'Motor GPIO diagnostic entry unexpectedly links radio/PWM logic' >&2
    exit 1
fi
python3 - <<'PY'
import hashlib
import json
from pathlib import Path

root = Path('.')
manifest = json.loads((root / 'docs/firmware_sha256.json').read_text())
for relative, expected in manifest.items():
    actual = hashlib.sha256((root / relative).read_bytes()).hexdigest()
    assert actual == expected, f'{relative}: SHA256 mismatch'
print('Competition firmware isolation, build snapshot and manifest passed')
PY
