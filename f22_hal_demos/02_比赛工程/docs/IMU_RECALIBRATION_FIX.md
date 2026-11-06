# IMU calibration recovery bench fix (2026-10-14)

## Observed failure

The operator's JP13 capture showed a valid IMU before F1. After F1, subsequent
samples reported `imu_raw=0 calibrated=0 acc_mg=0,0,0`; F2 was correctly rejected
with fault 11 and all motor outputs remained zero. This proves calibration
failure invalidated the active IMU state, but the capture does not identify
whether motion, sample spread, or an I2C error caused the rejected window.

## Change

Calibration now reports its result and failure reason on JP13 as `[CAL]`, with
sample count, failing axis, sample spread, and standard deviation. A failed
recalibration can reuse the last accepted bias only when a complete sample
window failed ordinary motion/noise limits. Incomplete, nonfinite, sensor ID,
initialization, or I2C read failures remain disarmed. A successful window
replaces the old bias atomically. An already-running IMU is not needlessly
reconfigured for F1. The existing one-second attitude settle and start gates
remain active.

## Verification

Host tests exercise the production `Calibrate()` wrapper with synthetic sensor
I/O for accepted calibration, motion rejection with prior-bias retention,
first-calibration rejection, I2C/ID/init failure, and successful bias update.
Calibration module tests also check each rejection class. The five PlatformIO
environments compile, and the motor-output isolation check passes. These tests
do not identify the physical cause of the failed F1 window.

The candidate was built as `contest_aircraft1_radio_v2_jp13`; its SHA-256 and
the readback of the previous firmware are recorded in
`validation/imu-recalibration-2026-10-14/`. The candidate was later uploaded and fully read back with an identical SHA-256.
Operator logs verified calibrated=1/reused=1 after variance or movement rejection.
Intermittent I2C HAL_BUSY failures still remain unresolved; see PROJECT_PLAN.md.
