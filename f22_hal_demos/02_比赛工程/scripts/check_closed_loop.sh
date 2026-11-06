#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_closed_dir=$(mktemp -d)
trap 'rm -rf "$task_closed_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_imu_calibration.c src/flight/imu_calibration.c -lm \
    -o "$task_closed_dir/imu_calibration"
"$task_closed_dir/imu_calibration" docs/validation/bench-2026-10-10/imu-competition-samples.csv
python3 tests/test_imu_recalibration.py
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/sensors tests/test_flow_driver.c src/sensors/flow.c \
    -o "$task_closed_dir/flow_driver"
"$task_closed_dir/flow_driver"
python3 tests/test_flow_recovery.py
python3 tests/test_flow_spi.py
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/bench tests/test_flow_capture.c src/bench/flow_capture.c -lm \
    -o "$task_closed_dir/flow_capture"
"$task_closed_dir/flow_capture"
python3 tests/test_flow_capture_idle.py
python3 tests/test_capture_flow_reconnect.py
python3 tests/test_flow_capture_bus.py
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 -DCL_FLOW_PROVISIONAL_CONTROL=1 \
    -DCL_TETHERED_FLOW_DEBUG_OUTPUT=1 -Isrc/flight -Isrc/sensors \
    tests/test_tof_control.c src/sensors/tof.c src/flight/closed_loop.c \
    src/flight/flow_assist.c src/flight/aircraft1.c src/flight/pid.c \
    src/flight/observation.c -lm -o "$task_closed_dir/tof_control"
"$task_closed_dir/tof_control"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 -DCL_FLOW_PROVISIONAL_CONTROL=1 \
    -DCL_TETHERED_FLOW_DEBUG_OUTPUT=1 -Isrc/flight -Isrc/sensors \
    tests/test_roll_tuning.c src/flight/closed_loop.c src/flight/flow_assist.c \
    src/flight/aircraft1.c src/flight/pid.c -lm -o "$task_closed_dir/roll_tuning"
"$task_closed_dir/roll_tuning"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DFLOW_ID_PROBE=1 -DFLOW_CAL_CAPTURE=1 -DCL_HOST_REPLAY_ONLY=1 -DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 \
    -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight \
    tests/test_aircraft1_pwm.c src/hal/Src/aircraft1_pwm.c -lm \
    -o "$task_closed_dir/calibration_pwm_lock"
"$task_closed_dir/calibration_pwm_lock"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_HOST_REPLAY_ONLY=1 -DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 \
    -DCL_FLOW_PROVISIONAL_CONTROL=1 -Isrc/flight \
    tests/test_flow_assist.c src/flight/flow_assist.c src/flight/closed_loop.c \
    src/flight/aircraft1.c src/flight/pid.c -lm -o "$task_closed_dir/flow_assist"
"$task_closed_dir/flow_assist"
for lock_profile in tether tether100 flow; do
    lock_flags=(-DCL_TETHERED_FLIGHT=1)
    if [[ "$lock_profile" == tether100 ]]; then lock_flags+=(-DCL_TETHERED_PWM100=1); fi
    if [[ "$lock_profile" == flow ]]; then lock_flags+=(-DCL_FLOW_ASSIST=1); fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${lock_flags[@]}" -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight \
        tests/test_incident_lock.c src/hal/Src/aircraft1_pwm.c src/flight/closed_loop.c \
        src/flight/flow_assist.c src/flight/aircraft1.c src/flight/pid.c -lm \
        -o "$task_closed_dir/lock_$lock_profile"
    "$task_closed_dir/lock_$lock_profile"
done
for embedded_macro in STM32F103xE __arm__ __thumb__ USE_HAL_DRIVER; do
    if cc -E "-D$embedded_macro=1" -DCL_HOST_REPLAY_ONLY=1 src/flight/closed_loop_profile.h \
        > "$task_closed_dir/replay-denied.log" 2>&1; then
        echo 'ERROR: embedded target accepted host replay override' >&2; exit 1
    fi
    grep -qF 'Host replay cannot enable an embedded ARM flight image' "$task_closed_dir/replay-denied.log"
done
for flow_runtime in noflow flow; do
    runtime_flags=()
    if [[ "$flow_runtime" == noflow ]]; then runtime_flags+=(-DCL_TETHERED_FLIGHT=1 -DCL_TETHERED_PWM100=1); fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${runtime_flags[@]}" -Isrc/flight -Isrc/sensors tests/test_flow_runtime.c \
        src/flight/flow_runtime.c -o "$task_closed_dir/runtime_$flow_runtime"
    "$task_closed_dir/runtime_$flow_runtime"
done
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_height_observer.c src/flight/observation.c -lm \
    -o "$task_closed_dir/height_observer"
"$task_closed_dir/height_observer"
for floor_profile in noflow flow; do
    floor_flags=(-DCL_HOST_REPLAY_ONLY=1 -DCL_TETHERED_FLIGHT=1)
    if [[ "$floor_profile" == noflow ]]; then
        floor_flags+=(-DCL_TETHERED_PWM100=1)
    else
        floor_flags+=(-DCL_FLOW_ASSIST=1 -DCL_FLOW_PROVISIONAL_CONTROL=1)
    fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${floor_flags[@]}" -Isrc/flight tests/test_near_floor_recovery.c \
        src/flight/closed_loop.c src/flight/flow_assist.c src/flight/aircraft1.c \
        src/flight/pid.c -lm -o "$task_closed_dir/floor_$floor_profile"
    "$task_closed_dir/floor_$floor_profile"
done
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_HOST_REPLAY_ONLY=1 -DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 \
    -DCL_FLOW_PROVISIONAL_CONTROL=1 -DCL_FLOW_BRAKE_ONLY=1 -Isrc/flight \
    tests/test_flow_brake.c src/flight/closed_loop.c src/flight/flow_assist.c \
    src/flight/aircraft1.c src/flight/pid.c -lm -o "$task_closed_dir/flow_brake"
"$task_closed_dir/flow_brake"
for suite in aircraft1 closed_loop; do
    suite_flags=()
    if [[ "$suite" == closed_loop ]]; then
        suite_flags=(-DCL_RADIO_POWER_PROFILE=1)
    fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${suite_flags[@]}" -Isrc/flight "tests/test_$suite.c" src/flight/aircraft1.c \
        src/flight/closed_loop.c src/flight/hover.c src/flight/pid.c src/flight/observation.c -lm \
        -o "$task_closed_dir/$suite"
    "$task_closed_dir/$suite"
done
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_task_input.c src/flight/task_input.c src/flight/radio.c \
    src/flight/hover.c src/flight/pid.c -lm -o "$task_closed_dir/task_input"
"$task_closed_dir/task_input"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_closed_loop_radio.c src/flight/closed_loop_radio.c \
    src/flight/radio_trace.c src/flight/radio.c src/flight/closed_loop.c \
    src/flight/aircraft1.c src/flight/hover.c src/flight/pid.c -lm \
    -o "$task_closed_dir/radio_v2"
"$task_closed_dir/radio_v2"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DFLOW_ID_PROBE=1 -DFLOW_CAL_CAPTURE=1 -DFLOW_CAL_MARKERS=1 \
    -Isrc/flight tests/test_flow_cal_markers.c \
    src/flight/closed_loop_radio.c src/flight/radio_trace.c src/flight/radio.c \
    -o "$task_closed_dir/flow_cal_markers"
"$task_closed_dir/flow_cal_markers"
for profile in legacy radio_power tether tether100 tether100_debug flow flow_debug; do
    profile_flags=(-DCL_HOST_REPLAY_ONLY=1 -DTEST_CLOSED_LOOP_PWM=1)
    if [[ "$profile" == radio_power ]]; then
        profile_flags+=(-DCL_RADIO_POWER_PROFILE=1)
    fi
    if [[ "$profile" == tether ]]; then
        profile_flags+=(-DCL_RADIO_POWER_PROFILE=1 -DCL_TETHERED_FLIGHT=1)
    fi
    if [[ "$profile" == tether100 ]]; then
        profile_flags+=(-DCL_TETHERED_FLIGHT=1 -DCL_TETHERED_PWM100=1)
    fi
    if [[ "$profile" == tether100_debug ]]; then
        profile_flags+=(-DCL_TETHERED_FLIGHT=1 -DCL_TETHERED_PWM100=1 -DCL_TETHERED_DEBUG_OUTPUT=1)
    fi
    if [[ "$profile" == flow ]]; then
        profile_flags+=(-DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1)
    fi
    if [[ "$profile" == flow_debug ]]; then
        profile_flags+=(-DCL_TETHERED_FLIGHT=1 -DCL_FLOW_ASSIST=1 \
            -DCL_FLOW_PROVISIONAL_CONTROL=1 -DCL_TETHERED_FLOW_DEBUG_OUTPUT=1)
    fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${profile_flags[@]}" -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight \
        tests/test_aircraft1_pwm.c src/hal/Src/aircraft1_pwm.c \
        src/flight/closed_loop.c src/flight/flow_assist.c src/flight/aircraft1.c src/flight/pid.c \
        -lm -o "$task_closed_dir/pwm_$profile"
    "$task_closed_dir/pwm_$profile"
done
for profile in tether tether100; do
    profile_flags=(-DCL_HOST_REPLAY_ONLY=1 -DCL_TETHERED_FLIGHT=1)
    if [[ "$profile" == tether ]]; then
        profile_flags+=(-DCL_RADIO_POWER_PROFILE=1)
    else
        profile_flags+=(-DCL_TETHERED_PWM100=1)
    fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
        "${profile_flags[@]}" -Isrc/flight \
        tests/test_tethered_flight.c src/flight/closed_loop.c src/flight/aircraft1.c \
        src/flight/pid.c -lm -o "$task_closed_dir/$profile"
    "$task_closed_dir/$profile"
done
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DAIRCRAFT1_STANDALONE=1 -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight \
    tests/test_aircraft1_pwm.c src/hal/Src/aircraft1_pwm.c -lm -o "$task_closed_dir/pwm_local"
"$task_closed_dir/pwm_local"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_DIRECTION_CHECK=1 -DTEST_DIRECTION_CHECK=1 \
    -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight -Isrc/bench \
    tests/test_aircraft1_pwm.c src/hal/Src/aircraft1_pwm.c src/bench/motor_direction.c \
    src/flight/closed_loop.c src/flight/aircraft1.c src/flight/pid.c \
    -lm -o "$task_closed_dir/direction"
"$task_closed_dir/direction"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -DCL_RADIO_POWER_PROFILE=1 -DCL_ATTITUDE_CHECK=1 -DTEST_ATTITUDE_CHECK=1 \
    -Itests/pwm_fake -Isrc/hal/Inc -Isrc/flight -Isrc/bench \
    tests/test_aircraft1_pwm.c src/hal/Src/aircraft1_pwm.c src/bench/attitude_check.c \
    src/flight/closed_loop.c src/flight/aircraft1.c src/flight/pid.c \
    -lm -o "$task_closed_dir/attitude"
"$task_closed_dir/attitude"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g \
    -Isrc/flight tests/test_standalone_keys.c src/flight/standalone_keys.c \
    -o "$task_closed_dir/keys"
"$task_closed_dir/keys"
pio run -e contest_hover -e contest_aircraft1_closed_loop -e contest_aircraft1_pwm_closed_loop -e contest_aircraft1_standalone -e contest_aircraft1_radio_v2_jp13 -e contest_motor_direction -e contest_attitude_check_jp13 -e contest_tethered_flight_jp13 -e contest_tethered_flight_jp13_pwm100 -e contest_tethered_flight_debug_jp13_pwm100 -e contest_flow_assist_jp13 -e contest_tethered_flow_debug_jp13 -e contest_flow_calibration_jp13
python3 - <<'PY'
import subprocess
from pathlib import Path
nm = Path.home() / '.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-nm'
for profile in ('contest_hover', 'contest_aircraft1_closed_loop', 'contest_aircraft1_pwm_closed_loop', 'contest_aircraft1_standalone', 'contest_aircraft1_radio_v2_jp13', 'contest_motor_direction', 'contest_attitude_check_jp13', 'contest_tethered_flight_jp13', 'contest_tethered_flight_jp13_pwm100', 'contest_tethered_flight_debug_jp13_pwm100', 'contest_flow_assist_jp13', 'contest_tethered_flow_debug_jp13', 'contest_flow_calibration_jp13'):
    image = Path(f'.pio/build/{profile}/firmware.bin').read_bytes()
    markers = {
        'contest_tethered_flight_jp13': (b'TETHER90', b'flight=tether90'),
        'contest_tethered_flight_jp13_pwm100': (b'TETHER100_NOFLOW', b'flight=tether100_noflow'),
        'contest_tethered_flight_debug_jp13_pwm100': (b'TETHER100_DEBUG_OUTPUT_ACTIVE', b'flight=tether100_noflow', b'output_locked=0 debug_output=1'),
        'contest_flow_assist_jp13': (b'TETHER100_FLOW', b'flight=tether100_flow',
                                     b'flow_hold=', b'flow_err_fwd_mm='),
        'contest_tethered_flow_debug_jp13': (b'TETHERED_FLOW_DEBUG', b'flight=tether100_flow',
                                             b'flow_hold=', b'flow_model=provisional',
                                             b'output_locked=0 debug_output=flow_provisional',
                                             b'[I2C2] boot_bus_clear='),
    }
    if profile in markers:
        for marker in markers[profile]:
            assert marker in image, (profile, marker)
        if profile in ('contest_tethered_flight_debug_jp13_pwm100',
                       'contest_tethered_flow_debug_jp13'):
            for marker in (b'FLIGHT_HOLD', b'output_locked=1 lock_reason=incident'):
                assert marker not in image, (profile, marker)
        else:
            for marker in (b'FLIGHT_HOLD', b'output_locked=1 lock_reason=incident'):
                assert marker in image, (profile, marker)
    result = subprocess.check_output([str(nm), f'.pio/build/{profile}/firmware.elf'], text=True)
    physical = 'Aircraft1Pwm_Apply' in result
    assert physical == (profile in ('contest_aircraft1_pwm_closed_loop', 'contest_aircraft1_standalone', 'contest_aircraft1_radio_v2_jp13', 'contest_motor_direction', 'contest_attitude_check_jp13', 'contest_tethered_flight_jp13', 'contest_tethered_flight_jp13_pwm100', 'contest_tethered_flight_debug_jp13_pwm100', 'contest_flow_assist_jp13', 'contest_tethered_flow_debug_jp13', 'contest_flow_calibration_jp13')), profile
    direction = profile == 'contest_motor_direction'
    attitude = profile == 'contest_attitude_check_jp13'
    assert (' T MotorDirection_Step\n' in result) == direction, profile
    assert (' T AttitudeCheck_Step\n' in result) == attitude, profile
    if profile in ('contest_aircraft1_radio_v2_jp13', 'contest_motor_direction', 'contest_attitude_check_jp13', 'contest_tethered_flight_jp13', 'contest_tethered_flight_jp13_pwm100', 'contest_tethered_flight_debug_jp13_pwm100', 'contest_flow_assist_jp13', 'contest_tethered_flow_debug_jp13'):
        assert ' T TaskInput_Feed\n' not in result, 'V2 main must not call the old v1 bridge'
        for symbol in ('ClosedLoopRadio_Feed', 'ClosedLoopRadio_Take', 'USART2_IRQHandler', 'RadioFast_Configure'):
            assert f' T {symbol}\n' in result, (profile, symbol)
    if profile == 'contest_aircraft1_standalone':
        assert ' T HAL_UART_Receive_IT\n' not in result, 'Local mode must not arm radio RX'
    if profile == 'contest_flow_calibration_jp13':
        assert b'output_locked=1' in image, 'flow calibration image must advertise the output lock'
        assert b'[FCAL]' in image, 'flow calibration image must contain calibration telemetry'
        assert ' T FlowCapture_Add\n' in result, 'flow calibration capture must be linked'
    if physical:
        for symbol in (('MotorDirection_Step' if direction else 'AttitudeCheck_Step' if attitude else 'ClosedLoop_Step'), 'Aircraft1Pwm_Trip', 'HAL_SYSTICK_Callback'):
            assert f' T {symbol}\n' in result, symbol
        assert ' T I2C2_ClearBusAtBoot\n' in result, (profile, 'missing pre-sensor I2C2 recovery')
    if direction:
        assert ' T ClosedLoop_Step\n' not in result, 'Direction profile must not run flight control'
    if attitude:
        assert ' T ClosedLoop_AttitudeMix\n' in result, 'Attitude bench must use the flight mixer'
        assert ' T ClosedLoop_Step\n' not in result, 'Attitude bench must not run takeoff/height control'
    flow_assist = ' T FlowAssist_Step\n' in result
    assert flow_assist == (profile == 'contest_tethered_flow_debug_jp13'), \
        (profile, 'flow-assist linkage does not match the explicitly enabled tether target')
    print(profile, 'physical PWM driver' if physical else 'no physical PWM driver')
print('Closed-loop build passed: default tether targets locked; explicit no-flow and provisional-flow tether targets unlocked; hardware acceptance pending')
PY
