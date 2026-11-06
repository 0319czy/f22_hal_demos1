#ifndef F22_TOF_CONTROL_H
#define F22_TOF_CONTROL_H
#include "tof.h"

/* A recent status-9 result is still a live sensor in its 0..39mm blind zone.
 * This is permission for the bounded LAUNCH ramp, NOT valid height, proof of
 * ground contact, or permission to extend an airborne height estimate.
 * A valid short-term height prediction may bridge a transient bad status;
 * the sensor must still be present and delivering recent results. */
static inline unsigned Tof_ControlFresh(const TofSensor *tof,
                                        unsigned predicted_height_valid)
{
    return tof->present && tof->seen && Tof_AgeMs(tof) < TOF_MAX_AGE_MS &&
           ((tof->raw_status == 9U && tof->distance_mm <= 4000U) ||
            predicted_height_valid);
}

/* Keep zero/under-range returns OUT of touchdown and radio-loss ground proof. */
static inline unsigned Tof_ControlNearGround(const TofSensor *tof)
{
    return Tof_ControlFresh(tof, 0U) && tof->distance_mm >= 5U &&
           tof->distance_mm < 40U;
}
#endif
