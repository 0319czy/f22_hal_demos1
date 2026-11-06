#!/usr/bin/env python3
"""Exercise the actual main.c calibration wrapper with synthetic sensor I/O."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.c').read_text()
wrapper = main[main.index('/* A rejected F1 window'):main.index('static void Rotate(')]
harness = r'''
#include "imu_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef struct { int16_t accel[3], gyro[3]; } ImuSample;
static unsigned mode, sample_index, init_calls;
static HAL_StatusTypeDef Sensors_ImuReadId(uint8_t *id) {
    *id = mode == 4 ? 0x69 : 0x68;
    return mode == 3 ? HAL_ERROR : HAL_OK;
}
static HAL_StatusTypeDef Sensors_ImuInit(void) {
    ++init_calls; return mode == 6 ? HAL_ERROR : HAL_OK;
}
static HAL_StatusTypeDef Sensors_ImuRead(ImuSample *s) {
    if (mode == 2 && sample_index == 10) return HAL_ERROR;
    float noise = (sample_index++ & 1U) ? .04f : -.04f;
    if (mode == 1) noise *= 50;
    s->accel[0] = s->accel[1] = 0; s->accel[2] = 16384;
    s->gyro[0] = (int16_t)(((mode == 5 ? .5f : 3.4f) + noise) * 131);
    s->gyro[1] = s->gyro[2] = 0;
    return HAL_OK;
}
static void HAL_Delay(unsigned ms) { (void)ms; }
'''
checks = r'''
int main(void) {
    float bias[3] = {0};
    assert(Calibrate(bias, 0)); assert(init_calls == 1);
    float accepted = bias[0]; assert(fabsf(accepted - 3.4f) < .02f);
    mode = 1; sample_index = 0;
    assert(Calibrate(bias, 1)); /* F1 motion rejection retains old ready/bias. */
    assert(init_calls == 1 && bias[0] == accepted && !calibration_io_failed);
    sample_index = 0;
    assert(!Calibrate(bias, 0)); /* No previous calibration: never invent ready. */
    assert(bias[0] == accepted);
    for (mode = 2; mode <= 4; ++mode) {
        sample_index = 0;
        assert(!Calibrate(bias, 1)); /* Read error, ID error, wrong sensor ID. */
        assert(calibration_io_failed && bias[0] == accepted);
    }
    mode = 6; sample_index = 0;
    assert(!Calibrate(bias, 0)); assert(calibration_io_failed);
    mode = 5; sample_index = 0;
    assert(Calibrate(bias, 1)); assert(fabsf(bias[0] - .5f) < .02f);
    puts("main.c recalibration wrapper PASS (synthetic I/O): retain motion rejection, fail closed on I/O/ID/init, atomic successful replacement");
}
'''
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / 'recalibration.c'
    binary = Path(directory) / 'recalibration'
    source.write_text(harness + wrapper + checks)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                    '-fsanitize=address,undefined', '-g', '-I' + str(root / 'src/flight'),
                    str(source), str(root / 'src/flight/imu_calibration.c'), '-lm',
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
