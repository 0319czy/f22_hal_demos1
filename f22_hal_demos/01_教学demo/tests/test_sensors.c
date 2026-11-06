#include "tof.h"
#include "flow.h"
#include "observation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t registers[65536], burst[12]; uint32_t now; unsigned fail_read,fail_write,flow_mode; } Fake;
static Fake fake;
static int Read(void *ctx,uint16_t reg,uint8_t *data,size_t n)
{
    Fake *f=ctx; if(f->fail_read)return -1;
    if(f->flow_mode && reg==0x16 && n==12)memcpy(data,f->burst,n);
    else memcpy(data,f->registers+reg,n);
    return 0;
}
static int Write(void *ctx,uint16_t reg,const uint8_t *data,size_t n)
{
    Fake *f=ctx; if(f->fail_write)return -1; memcpy(f->registers+reg,data,n);
    /* Hardware interrupt flag is read-only; writes to defaults must not clear ready. */
    if(!f->flow_mode && reg==0x2d)f->registers[0x31]=1;
    return 0;
}
static uint32_t Now(void *ctx) {return ((Fake *)ctx)->now;}
static void Delay(void *ctx,uint32_t ms) {((Fake *)ctx)->now+=ms;}
static SensorIo Io(void) {SensorIo io={&fake,Read,Write,Now,Delay};return io;}
static void SetupTof(void)
{
    memset(&fake,0,sizeof(fake));fake.registers[0xe5]=1;
    fake.registers[0x10f]=0xea;fake.registers[0x110]=0xcc;
    fake.registers[0x31]=1;fake.registers[0xde]=1;
}
static void TestTof(void)
{
    TofSensor s;
    SetupTof(); assert(!Tof_Init(&s,Io()));assert(s.present && !s.valid && s.id==0xeacc);
    assert(fake.registers[0x4b]==0x0a && fake.registers[0x5e]==0 && fake.registers[0x5f]==0x60);
    fake.registers[0x89]=9;fake.registers[0x96]=1;fake.registers[0x97]=0xf4;
    assert(Tof_Poll(&s)==1 && s.valid && s.distance_mm==500);
    fake.registers[0x31]=0; fake.now+=149;
    assert(Tof_Poll(&s)==0 && s.valid);
    fake.now++;assert(Tof_Poll(&s)==0 && !s.valid);
    fake.now+=351;assert(Tof_Poll(&s)==-1 && !s.present);
    SetupTof();assert(!Tof_Init(&s,Io()));
    fake.registers[0x89]=4;fake.registers[0x96]=0;fake.registers[0x97]=1;
    assert(Tof_Poll(&s)==1 && !s.valid); /* non-valid status must not become ground */
    fake.registers[0x89]=9;fake.registers[0x96]=0x10;fake.registers[0x97]=0;
    assert(Tof_Poll(&s)==1 && !s.valid); /* >4m */
    fake.fail_read=1;assert(Tof_Poll(&s)==-1 && !s.present && !s.valid);
    SetupTof();fake.registers[0x110]=0xab;assert(Tof_Init(&s,Io())==-1 && !s.present);
    SetupTof();fake.fail_write=1;assert(Tof_Init(&s,Io())==-1 && s.errors==1);
    SetupTof();fake.registers[0xe5]=0;fake.now=UINT32_MAX-100U;
    uint32_t start=fake.now;assert(Tof_Init(&s,Io())==-1 && (uint32_t)(fake.now-start)==500);
    SetupTof();assert(!Tof_Init(&s,Io()));fake.now=UINT32_MAX-20;
    fake.registers[0x89]=9;fake.registers[0x96]=1;fake.registers[0x97]=0;
    assert(Tof_Poll(&s)==1 && s.valid);fake.now=10;
    fake.registers[0x31]=0;assert(Tof_Poll(&s)==0 && s.valid && Tof_AgeMs(&s)==31);
}
static void SetupFlow(void)
{memset(&fake,0,sizeof(fake));fake.flow_mode=1;fake.registers[0]=0x49;fake.registers[0x5f]=0xb6;}
static void TestFlow(void)
{
    FlowSensor s; SetupFlow();assert(Flow_Init(&s,Io())==0 && s.present);
    fake.burst[0]=0x80;fake.burst[2]=0xfe;fake.burst[3]=0xff;
    fake.burst[4]=0x34;fake.burst[5]=0x12;fake.burst[6]=30;
    assert(!Flow_Poll(&s) && s.valid && s.dx==-2 && s.dy==0x1234);
    fake.burst[0]=0;fake.burst[2]=fake.burst[3]=fake.burst[4]=fake.burst[5]=0;
    assert(!Flow_Poll(&s) && s.valid); /* stationary is valid */
    fake.burst[6]=0;assert(!Flow_Poll(&s) && !s.valid);
    fake.burst[6]=30;fake.burst[0]=0x90;assert(!Flow_Poll(&s) && s.valid); /* rawFrom0 is not overflow */
    fake.fail_read=1;assert(Flow_Poll(&s)==-1 && !s.present && !s.valid && s.dx==0);
    SetupFlow();fake.registers[0x5f]=0;assert(Flow_Init(&s,Io())==-1);
    SetupFlow();fake.fail_write=1;assert(Flow_Init(&s,Io())==-1);
}
static void TestObservation(void)
{
    Attitude a;Attitude_Init(&a);float acc[3]={0,0,-1},gyro[3]={0,0,90};
    for(unsigned i=0;i<100;i++)assert(Attitude_Update(&a,acc,gyro,.01f));
    assert(fabsf(a.yaw-90)<.02f && fabsf(a.roll)<.001f);
    assert(!Attitude_Update(&a,acc,gyro,.1f));
    Attitude_Init(&a);gyro[2]=0;acc[1]=-.5f;acc[2]=-.8660254f;
    for(unsigned i=0;i<1000;i++)assert(Attitude_Update(&a,acc,gyro,.01f));
    assert(fabsf(a.roll-30)<.01f && fabsf(a.pitch)<.01f);
    acc[0]=NAN;assert(!Attitude_Update(&a,acc,gyro,.01f));
    HeightEstimate h;Height_Reset(&h);
    assert(Height_Update(&h,1,30,0,100));assert(fabsf(h.height-.8660254f)<1e-5);
    assert(Height_Update(&h,1,30,0,140) && h.vz==0);
    assert(!Height_Update(&h,1,31,0,180));assert(!Height_Update(&h,NAN,0,0,180));
    FlowEstimate e;FlowEstimate_Reset(&e);
    FlowCalibration c={.radians_per_count=.002f,.translation={1,0,0,1},.rotation={0,1,-1,0},.calibrated=0};
    assert(!FlowEstimate_Update(&e,&c,10,0,.5f,gyro,0,.02f));assert(!e.valid && e.x==0);
    c.calibrated=1;assert(FlowEstimate_Update(&e,&c,10,0,.5f,gyro,0,.02f));
    assert(fabsf(e.vx-.5f)<1e-5 && fabsf(e.x-.01f)<1e-5);
    assert(FlowEstimate_Update(&e,&c,10,0,.5f,gyro,90,.02f));assert(fabsf(e.vy-.5f)<1e-5);
    gyro[1]=57.2957795f;
    assert(FlowEstimate_Update(&e,&c,10,0,.5f,gyro,0,.02f));assert(fabsf(e.vx)<1e-5);
    assert(!FlowEstimate_Update(&e,&c,32767,0,.5f,gyro,0,.02f));
    assert(!FlowEstimate_Update(&e,&c,0,0,.04f,gyro,0,.02f));
}
int main(void){TestTof();TestFlow();TestObservation();puts("ToF/flow register transport fault tests and observation math passed (fake devices, no hardware)");}
