#include "observation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void Near(float actual, float expected, float tolerance)
{
    assert(isfinite(actual));
    assert(fabsf(actual - expected) <= tolerance);
}

static void StationaryAndHold(void)
{
    HeightEstimate h;
    Attitude a;
    const float at_rest[3] = {0.0f, 0.0f, -1.0f};
    Height_Reset(&h);
    Attitude_Init(&a);
    a.valid = 1U;

    assert(Height_Correct(&h, 0.20f, 0.0f, 0.0f, 100U));
    for (uint32_t now = 105U; now <= 200U; now += 5U)
        assert(Height_Predict(&h, &a, at_rest, now));
    Near(h.height, 0.20f, 1e-5f);
    Near(h.vz, 0.0f, 1e-5f);
    assert(!Height_Predict(&h, &a, at_rest, 205U));
    assert(!h.valid);
}

static void VerticalAccelerationAndRangeCorrection(void)
{
    HeightEstimate h;
    Attitude a;
    const float thrust_up[3] = {0.0f, 0.0f, -1.2f};
    Height_Reset(&h);
    Attitude_Init(&a);
    a.valid = 1U;

    assert(Height_Correct(&h, 0.10f, 0.0f, 0.0f, 100U));
    for (uint32_t now = 105U; now <= 150U; now += 5U)
        assert(Height_Predict(&h, &a, thrust_up, now));
    assert(h.height > 0.10f);
    assert(h.vz > 0.0f);
    Near(h.vz, 0.2f * 9.80665f * 0.05f, 0.002f);

    /* ToF directly anchors height, while its differentiated speed gently
     * corrects the inertial velocity estimate. */
    assert(Height_Correct(&h, 0.14f, 0.0f, 0.0f, 160U));
    Near(h.height, 0.14f, 1e-6f);
    assert(h.vz > 0.30f && h.vz < 0.40f);
    assert(!Height_Correct(&h, 0.039f, 0.0f, 0.0f, 200U));
    assert(Height_Correct(&h, 0.040f, 0.0f, 0.0f, 200U));
    Near(h.height, 0.040f, 1e-6f);
}

static void TiltProjectionAndGapRecovery(void)
{
    HeightEstimate h;
    Attitude a;
    const float at_rest[3] = {0.0f, 0.0f, -1.0f};
    Height_Reset(&h);
    Attitude_Init(&a);
    a.valid = 1U;

    assert(Height_Correct(&h, 0.20f, 20.0f, 10.0f, 100U));
    Near(h.height, 0.20f * cosf(20.0f * 0.0174532925f) *
                         cosf(10.0f * 0.0174532925f), 1e-6f);
    assert(!Height_Predict(&h, &a, at_rest, 151U));
    assert(!h.coast_valid);

    /* A later valid range re-anchors the estimate with zero assumed speed. */
    assert(Height_Correct(&h, 0.18f, 0.0f, 0.0f, 160U));
    Near(h.height, 0.18f, 1e-6f);
    Near(h.vz, 0.0f, 1e-6f);
    assert(h.valid && h.coast_valid);
}

static void ToFInnovationGate(void)
{
    HeightEstimate h;
    HeightEstimate edge;
    Attitude a;
    const float at_rest[3] = {0.0f, 0.0f, -1.0f};
    Height_Reset(&h);
    Attitude_Init(&a);
    a.valid = 1U;

    assert(Height_Correct(&h, 0.20f, 0.0f, 0.0f, 100U));
    for (uint32_t now = 105U; now <= 140U; now += 5U)
        assert(Height_Predict(&h, &a, at_rest, now));
    assert(!Height_Correct(&h, 0.60f, 0.0f, 0.0f, 140U));
    assert(h.valid);
    assert(h.tof_innovation_rejects == 1U);
    Near(h.height, 0.20f, 1e-5f);
    Near(h.last_tof_height, 0.20f, 1e-6f);

    /* A following plausible sample must still correct the coasted estimate. */
    assert(Height_Correct(&h, 0.22f, 0.0f, 0.0f, 145U));
    assert(h.valid);
    Near(h.height, 0.22f, 1e-6f);
    assert(h.tof_innovation_rejects == 1U);

    Height_Reset(&edge);
    assert(Height_Correct(&edge, 0.20f, 0.0f, 0.0f, 100U));
    for (uint32_t now = 105U; now <= 140U; now += 5U)
        assert(Height_Predict(&edge, &a, at_rest, now));
    assert(Height_Correct(&edge, 0.319f, 0.0f, 0.0f, 140U));
    assert(edge.tof_innovation_rejects == 0U);
}

int main(void)
{
    StationaryAndHold();
    VerticalAccelerationAndRangeCorrection();
    TiltProjectionAndGapRecovery();
    ToFInnovationGate();
    puts("Height observer: rest, vertical sign, ToF correction/gate, projection, hold and recovery passed (synthetic inputs)");
    return 0;
}
