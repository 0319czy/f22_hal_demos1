#!/usr/bin/env python3
"""Exercise the real main.c FCAL initializer across idle observer resets."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / "src/main.c").read_text()
begin = main.index("FlowCaptureSample capture_sample = {")
end = main.index("FlowCapture_Add(&flow_capture, &capture_sample);", begin)
fragment = main[begin:end] + "FlowCapture_Add(&flow_capture, &capture_sample);"
harness = r'''
#include "flow_capture.h"
#include <assert.h>
#include <stdio.h>
#define TOF_MAX_AGE_MS 150U
static FlowCapture flow_capture;
static struct { unsigned distance_mm, present, seen, valid; } tof;
static struct { float roll, pitch; unsigned valid; } attitude;
static struct { int dx,dy; unsigned present,valid; } flow;
static struct { float height; unsigned valid; } height;
static unsigned age,now,flow_error,flow_window_started=1;
static float flow_dt=.02f,average_gyro[3]={10,-5,0};
static unsigned Tof_AgeMs(const void *sensor) { (void)sensor; return age; }
static void Capture(void) {
'''
checks = r'''
}
int main(void) {
    FlowCapture_Init(&flow_capture);
    tof.distance_mm=200;tof.present=tof.seen=tof.valid=1;
    attitude.valid=1;flow.present=flow.valid=1;flow.dx=1;flow.dy=2;
    height.height=.2f;
    /* Flow samples at50Hz, ToF at25Hz; IDLE clears the flight observer
     * between fresh ToF conversions. Both windows need the same live range. */
    for (unsigned i=0;i<100;++i) {
        now=i*20;age=i%2?20:0;height.valid=i%2?0:1;Capture();
    }
    assert(flow_capture.accepted==100 && flow_capture.rejected==0);
    assert(flow_capture.dx==100 && flow_capture.dy==200);
    age=149;Capture();assert(flow_capture.accepted==101);
    age=150;Capture();assert(flow_capture.rejected==1);
    age=0;tof.valid=0;Capture();assert(flow_capture.rejected==2);
    tof.valid=1;tof.present=0;Capture();assert(flow_capture.rejected==3);
    tof.present=1;tof.seen=0;Capture();assert(flow_capture.rejected==4);
    tof.seen=1;tof.distance_mm=0;Capture();assert(flow_capture.rejected==5);
    tof.distance_mm=200;attitude.roll=31;Capture();assert(flow_capture.rejected==6);
    assert(flow_capture.accepted==101);
    puts("PASS: real main FCAL adapter retains every live ToF window despite idle observer invalidation; stale/missing/zero/tilted ranges still rejected (synthetic I/O)");
}
'''
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "capture_idle.c"
    binary = Path(directory) / "capture_idle"
    source.write_text(harness + fragment + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=address,undefined", "-g", "-I" + str(root / "src/bench"),
                    str(source), str(root / "src/bench/flow_capture.c"), "-lm",
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
