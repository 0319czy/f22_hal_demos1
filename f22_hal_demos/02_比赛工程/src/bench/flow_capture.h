#ifndef F22_FLOW_CAPTURE_H
#define F22_FLOW_CAPTURE_H
#include <stdint.h>
#define FLOW_CAPTURE_WINDOW_LIMIT 60000U
/* Motor-locked calibration evidence only; never sets flight acceptance flags.
 * Accumulators are cumulative, so skipped telemetry does not lose samples.
 * A new epoch rolls the diagnostic window before int32 sample sums approach
 * overflow; marker pairs must remain within one epoch. */
typedef struct {
    uint32_t polls, raw_samples, accepted, rejected, io_errors, epoch, last_ms;
    int32_t raw_dx, raw_dy, dx, dy;
    float seconds, hx, hy, angle[3], hangle[3]; /* m*counts, rad, m*rad */
    unsigned invalid_reasons, saturated;
} FlowCapture;
typedef struct {
    uint32_t ms;
    int16_t dx, dy;
    float dt, height_m, gyro_dps[3];
    unsigned flow_ok, range_ok, imu_ok, same_window, io_error;
} FlowCaptureSample;
void FlowCapture_Init(FlowCapture *capture);
void FlowCapture_Add(FlowCapture *capture, const FlowCaptureSample *sample);
void FlowCapture_Break(FlowCapture *capture);
/* Calibration uses current measured range, not the IDLE flight observer. */
float FlowCapture_ProjectHeight(float range_m, float roll_deg, float pitch_deg);
#endif
