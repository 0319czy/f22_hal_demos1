#include "flow_capture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void Near(float actual, float expected)
{ assert(fabsf(actual - expected) < .0001f * (1.f + fabsf(expected))); }
int main(void)
{
    Near(FlowCapture_ProjectHeight(.2f,0,0),.2f);
    Near(FlowCapture_ProjectHeight(.2f,20,10),
         .2f*cosf(20*.0174532925199433f)*cosf(10*.0174532925199433f));
    assert(isnan(FlowCapture_ProjectHeight(.039f,0,0)));
    assert(isnan(FlowCapture_ProjectHeight(4.001f,0,0)));
    assert(isnan(FlowCapture_ProjectHeight(.2f,31,0)));
    assert(isnan(FlowCapture_ProjectHeight(.2f,0,-31)));
    assert(isnan(FlowCapture_ProjectHeight(.2f,NAN,0)));
    FlowCapture c; FlowCapture_Init(&c);
    FlowCaptureSample s = {.dx=1,.dy=2,.dt=.02f,.height_m=.2f,
        .gyro_dps={10,-5,0},.flow_ok=1,.range_ok=1,.imu_ok=1,.same_window=1};
    float hx=0,hy=0,angle=0,hangle=0;
    /* 100 sensor windows, no telemetry snapshots required. Varying height
     * catches the incorrect mean-height or double-height approximations. */
    for (unsigned i=0;i<100;++i) {
        s.ms=i*20; s.height_m=i%2 ? .4f : .2f;
        hx+=s.height_m; hy+=2*s.height_m;
        angle+=10*.0174532925199433f*.02f;
        hangle+=s.height_m*10*.0174532925199433f*.02f;
        FlowCapture_Add(&c,&s);
    }
    assert(c.raw_samples==100 && c.accepted==100 && c.dx==100 && c.dy==200);
    Near(c.hx,hx); Near(c.hy,hy); Near(c.angle[0],angle); Near(c.hangle[0],hangle);
    Near(c.seconds,2.f);
    /* A discarded interval must omit BOTH optical and gyro weighted data. */
    FlowCapture before=c; s.range_ok=0; FlowCapture_Add(&c,&s);
    assert(c.rejected==1 && c.raw_samples==101 && c.raw_dx==101 && c.dx==before.dx);
    Near(c.angle[0],before.angle[0]); Near(c.hangle[0],before.hangle[0]);
    s.range_ok=1; s.io_error=1; FlowCapture_Add(&c,&s);
    assert(c.io_errors==1 && c.raw_samples==101);
    s.io_error=0; s.same_window=0; FlowCapture_Add(&c,&s);
    s.same_window=1; s.dt=NAN; FlowCapture_Add(&c,&s);
    s.dt=.02f; s.gyro_dps[0]=INFINITY; FlowCapture_Add(&c,&s);
    assert(c.accepted==100 && c.rejected==5);
    FlowCapture_Break(&c); assert(c.epoch==1 && c.dx==100);
    /* Long-running diagnostic capture starts a fresh epoch before its
     * int32 sample sums saturate; it must not classify saturation as a bad
     * optical sample forever. */
    FlowCapture_Init(&c); s.gyro_dps[0]=0; s.dx=-32768; s.dy=32767;
    for(unsigned i=0;i<65536;++i) FlowCapture_Add(&c,&s);
    assert(c.epoch==1 && c.raw_samples==5536 && c.accepted==5536);
    assert(c.rejected==0 && !c.saturated);
    assert(c.dx==-32768*5536 && c.dy==32767*5536);
    puts("Full-sample capture: cumulative windows, matched count/gyro rejection, height weighting, and safe diagnostic epoch rollover passed (host synthetic samples)");
}
