#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d /tmp/f22-lessons.XXXXXX)
trap 'rm -rf "$task_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
   -fno-omit-frame-pointer -Isrc/flight \
   tests/test_flight.c src/flight/remote.c src/flight/radio.c src/flight/motor_guard.c \
   src/flight/pid.c src/flight/hover.c src/flight/hover_sim.c -lm -o "$task_dir/test_flight"
"$task_dir/test_flight"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
   -fno-omit-frame-pointer -Isrc/flight -Isrc/sensors \
   tests/test_sensors.c src/sensors/tof.c src/sensors/flow.c src/flight/observation.c \
   -lm -o "$task_dir/test_sensors"
"$task_dir/test_sensors"
pio run -e demo00_template -e demo01_gpio_led -e demo02_key_led -e demo05_i2c_scan \
  -e demo_selftest -e demo_vofa -e demo_uart_probe -e demo01_vofa \
  -e demo02_remote -e demo02_remote_framed \
  -e demo03_motor -e demo03_motor_bench -e demo04_hover_logic \
  -e demo05_tof -e demo06_flow -e demo07_integration
safe_envs=(
  demo00_template demo01_gpio_led demo02_key_led demo05_i2c_scan demo_selftest
  demo_vofa demo_uart_probe demo01_vofa demo02_remote demo02_remote_framed
  demo03_motor demo04_hover_logic demo05_tof demo06_flow demo07_integration
)
for env in "${safe_envs[@]}"; do
    if arm-none-eabi-nm ".pio/build/$env/firmware.elf" | \
       grep -Eq 'Board_MotorPwm|HAL_TIM_PWM_Start'; then
        echo "safe environment $env unexpectedly links motor PWM code" >&2
        exit 1
    fi
done
if ! arm-none-eabi-nm .pio/build/demo03_motor_bench/firmware.elf | \
     grep -Eq 'Board_MotorPwm|HAL_TIM_PWM_Start'; then
    echo 'demo03_motor_bench does not link its expected motor PWM driver' >&2
    exit 1
fi
echo 'motor PWM link check passed: only demo03_motor_bench contains motor output code'

python3 scripts/check_teaching_assets.py
