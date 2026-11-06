#include "../lesson_support.h"
#include "i2c.h"
#include "sensors.h"
#include "f22_sensor_bus.h"
#include "tof.h"
#include "flow.h"
#include "observation.h"
#include "hover.h"
#include "config.h"
#include <math.h>
static unsigned Calibrate(float bias[3])
{
    ImuSample sample; float sum[3]={0}; uint8_t id=0;
    if(Sensors_ImuReadId(&id)!=HAL_OK || id!=0x68U || Sensors_ImuInit()!=HAL_OK) return 0;
    for(unsigned n=0;n<100;n++) {
        if(Sensors_ImuRead(&sample)!=HAL_OK) return 0;
        float norm=0;
        for(unsigned k=0;k<3;k++) {
            float gyro=sample.gyro[k]/131.0f, acc=sample.accel[k]/16384.0f;
            if(fabsf(gyro)>2.0f) return 0;
            sum[k]+=gyro; norm+=acc*acc;
        }
        if(norm<.81f || norm>1.21f) return 0;
        HAL_Delay(5U);
    }
    for(unsigned k=0;k<3;k++) bias[k]=sum[k]/100;
    return 1;
}
static void Rotate(const float in[3],float out[3])
{for(unsigned i=0;i<3;i++)out[i]=imu_mount[3*i]*in[0]+imu_mount[3*i+1]*in[1]+imu_mount[3*i+2]*in[2];}
int main(void)
{
    TofSensor tof; FlowSensor flow; Attitude attitude; HeightEstimate height;
    FlowEstimate position; HoverController controller; HoverOutput output;
    float bias[3]={0},gyro[3]={0};
    Lesson_Init(); MX_I2C2_Init(); F22_ModuleBusInit();
    unsigned imu_ready=Calibrate(bias);
    Attitude_Init(&attitude); Height_Reset(&height); FlowEstimate_Reset(&position); Hover_Init(&controller);
    (void)Tof_Init(&tof,F22_TofIo()); (void)Flow_Init(&flow,F22_FlowIo());
    uint32_t previous=HAL_GetTick(),retry=previous; uint8_t previous_key=Lesson_Key1();
    while(1) {
        uint32_t now=HAL_GetTick(); float dt=(now-previous)*.001f; previous=now;
        if(dt==0)dt=.02f;
        if((uint32_t)(now-retry)>=2000U) {
            if(!imu_ready) {imu_ready=Calibrate(bias); Attitude_Init(&attitude);}
            if(!tof.present)(void)Tof_Init(&tof,F22_TofIo());
            if(!flow.present)(void)Flow_Init(&flow,F22_FlowIo());
            retry=HAL_GetTick();
        }
        ImuSample imu; float acc_sensor[3],gyro_sensor[3],acc[3];
        if(imu_ready && Sensors_ImuRead(&imu)==HAL_OK) {
            for(unsigned k=0;k<3;k++){acc_sensor[k]=imu.accel[k]/16384.0f;gyro_sensor[k]=imu.gyro[k]/131.0f-bias[k];}
            Rotate(acc_sensor,acc); Rotate(gyro_sensor,gyro);
            (void)Attitude_Update(&attitude,acc,gyro,dt);
        } else {imu_ready=0; attitude.valid=0;}
        int fresh=Tof_Poll(&tof); (void)Flow_Poll(&flow);
        if(fresh==1 && tof.valid && attitude.valid)
            (void)Height_Update(&height,tof.distance_mm*.001f,attitude.roll,attitude.pitch,tof.last_sample_ms);
        if(!tof.valid || !attitude.valid) Height_Reset(&height);
        position.valid=0;
        if(flow.valid && height.valid && attitude.valid)
            (void)FlowEstimate_Update(&position,&flow_calibration,flow.dx,flow.dy,height.height,gyro,attitude.yaw,dt);
        HoverSample sample={.roll_deg=attitude.roll,.pitch_deg=attitude.pitch,.yaw_deg=attitude.yaw,
            .gyro_dps={gyro[0],gyro[1],gyro[2]},.height_m=height.height,.vertical_speed_mps=height.vz,
            .x_m=position.x,.y_m=position.y,.vx_mps=position.vx,.vy_mps=position.vy,
            .imu_valid=attitude.valid && IMU_MOUNT_VERIFIED,.height_valid=height.valid,
            .position_valid=position.valid,.command_link_valid=1}; /* local buttons, not RF heartbeat */
        uint8_t key=Lesson_Key1();
        HoverCommand command=Lesson_Key2()?HOVER_STOP:(key && !previous_key?
            (controller.state==HOVER_IDLE?HOVER_START:HOVER_REQUEST_LAND):HOVER_NONE);
        previous_key=key;
        Hover_Step(&controller,&sample,command,dt,&output);
        float ch[]={output.state,attitude.valid?attitude.roll:NAN,attitude.valid?attitude.pitch:NAN,
            attitude.valid?attitude.yaw:NAN,height.valid?height.height:NAN,height.valid?height.vz:NAN,
            position.valid?position.x:NAN,position.valid?position.y:NAN,
            position.valid?position.vx:NAN,position.valid?position.vy:NAN,flow.dx,flow.dy,flow.quality,
            imu_ready,tof.valid,flow.valid,IMU_MOUNT_VERIFIED,FLOW_CALIBRATED,
            output.height_target_m,output.collective,output.virtual_motor[0],output.virtual_motor[1],
            output.virtual_motor[2],output.virtual_motor[3],0,controller.stable_seconds,
            controller.stable_seconds>5.0f,1};
        (void)Lesson_SendFloats(ch,28U);
        uint32_t elapsed=HAL_GetTick()-now; if(elapsed<20U) HAL_Delay(20U-elapsed);
    }
}
