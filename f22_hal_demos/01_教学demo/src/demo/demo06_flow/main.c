#include "../lesson_support.h"
#include "i2c.h"
#include "f22_sensor_bus.h"
#include "tof.h"
#include "flow.h"
#include <math.h>
int main(void)
{
    TofSensor tof; FlowSensor flow;
    Lesson_Init(); MX_I2C2_Init(); F22_ModuleBusInit();
    (void)Tof_Init(&tof,F22_TofIo()); (void)Flow_Init(&flow,F22_FlowIo());
    uint32_t retry=HAL_GetTick(); float counts_x=0,counts_y=0;
    while (1) {
        uint32_t now=HAL_GetTick();
        if((uint32_t)(now-retry)>=2000U) {
            if(!tof.present) (void)Tof_Init(&tof,F22_TofIo());
            if(!flow.present) (void)Flow_Init(&flow,F22_FlowIo());
            retry=HAL_GetTick();
        }
        (void)Tof_Poll(&tof); (void)Flow_Poll(&flow);
        if(flow.valid) {counts_x+=flow.dx; counts_y+=flow.dy;}
        if(Lesson_Key1()) counts_x=counts_y=0;
        float ch[]={flow.present,flow.id,flow.inverse_id,flow.dx,flow.dy,flow.quality,
            flow.motion,flow.valid,counts_x,counts_y,tof.valid?tof.distance_mm*.001f:NAN,
            tof.valid,flow.errors,tof.errors,0,0};
        (void)Lesson_SendFloats(ch,16U);
        uint32_t elapsed=HAL_GetTick()-now; if(elapsed<20U) HAL_Delay(20U-elapsed);
    }
}
