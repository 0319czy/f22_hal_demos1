#include "../lesson_support.h"
#include "i2c.h"
#include "f22_sensor_bus.h"
#include "tof.h"
#include "observation.h"
#include <math.h>
int main(void)
{
    TofSensor sensor; HeightEstimate height;
    Lesson_Init(); MX_I2C2_Init(); F22_ModuleBusInit(); Height_Reset(&height);
    (void)Tof_Init(&sensor, F22_TofIo()); uint32_t retry=HAL_GetTick();
    while (1) {
        uint32_t now=HAL_GetTick();
        if (!sensor.present && (uint32_t)(now-retry)>=2000U) {
            (void)Tof_Init(&sensor,F22_TofIo()); retry=HAL_GetTick(); Height_Reset(&height);
        }
        int fresh=Tof_Poll(&sensor);
        /* This lesson assumes a level bench; no IMU tilt correction is available. */
        if (fresh==1 && sensor.valid) (void)Height_Update(&height,sensor.distance_mm*.001f,0,0,sensor.last_sample_ms);
        if (!sensor.valid) height.valid=0U;
        float ch[] = {sensor.seen ? sensor.distance_mm : NAN,
            sensor.valid ? sensor.distance_mm*.001f : NAN,
            height.valid ? height.height : NAN, height.valid ? height.vz : NAN,
            sensor.present, sensor.valid, sensor.seen ? sensor.raw_status : NAN,
            sensor.id, sensor.seen ? (float)Tof_AgeMs(&sensor) : -1.0f,
            fresh==1, sensor.errors, 0};
        (void)Lesson_SendFloats(ch,12U);
        uint32_t elapsed=HAL_GetTick()-now; if(elapsed<40U) HAL_Delay(40U-elapsed);
    }
}
