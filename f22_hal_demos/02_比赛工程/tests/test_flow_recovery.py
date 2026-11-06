#!/usr/bin/env python3
"""Exercise the actual main.c flow-only recovery helper with injected I/O."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.c").read_text()
start = source.index("static void ServiceFlow(")
opening = source.index("{", start)
depth = 1
end = opening + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
helper = source[start:end]
test = r'''
#include "flow.h"
#include "closed_loop_radio.h"
#include "observation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t clock_ms;
static unsigned operations, sleeps;
static int io_bad = 1;
static int Read(void *c, uint16_t r, uint8_t *b, size_t n)
{ (void)c; operations++; memset(b,0,n); if(io_bad) return -1;
  if(r==0) *b=0x49; if(r==0x5f) *b=0xb6; return 0; }
static int Write(void *c,uint16_t r,const uint8_t*b,size_t n)
{ (void)c;(void)r;(void)b;(void)n;operations++;return io_bad ? -1 : 0; }
static uint32_t Now(void*c) { (void)c;return clock_ms; }
static void Delay(void*c,uint32_t n) { (void)c;sleeps++;clock_ms+=n; }
static ClosedLoopRadio input;
static HeightEstimate height;
static uint32_t attitude_started=10;
/* Accidental recovery resets would be observable, not silently stubbed away. */
void ReceiveReset(ClosedLoopRadio *in) { memset(in,0,sizeof(*in)); }
void Height_Reset(HeightEstimate *h) { memset(h,0,sizeof(*h)); }
'''
test += helper
test += r'''
int main(void) {
    FlowSensor flow; SensorIo io={0,Read,Write,Now,Delay};
    assert(Flow_Init(&flow,io)==-1);
    uint32_t retry=clock_ms; sleeps=0;
    input.radio.accepted=1234; input.sequence=432; input.ready=1;
    height.valid=height.initialized=1; height.height=.2f;
    for(unsigned i=0;i<1800;i++) {
        ClosedLoopRadio before=input; HeightEstimate h=height;
        unsigned n=operations;clock_ms+=5;
        ServiceFlow(&flow,1,clock_ms,&retry);
        assert(operations-n<=1 && !sleeps);
        assert(!memcmp(&input,&before,sizeof(input)) && !memcmp(&height,&h,sizeof(h)));
        assert(attitude_started==10);
    }
    assert(flow.attempts>=4 && flow.errors>=4);
    io_bad=0; Flow_BeginRetry(&flow);
    unsigned n=operations;
    for(unsigned i=0;i<200;i++) { clock_ms+=5;ServiceFlow(&flow,0,clock_ms,&retry); }
    assert(operations==n && flow.initializing); /* active flight cannot reconfigure */
    for(unsigned i=0;i<400 && !flow.present;i++) {clock_ms+=5;ServiceFlow(&flow,1,clock_ms,&retry);}
    assert(flow.present && !sleeps && input.ready && input.radio.accepted==1234);
    puts("Actual main recovery: cumulative failures, no radio/height/settling reset, one SPI op/cycle, active pause and resumed success passed (synthetic I/O)");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / "flow_recovery.c"
    c.write_text(test)
    binary = Path(directory) / "flow_recovery"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation",
                    "-fsanitize=address,undefined", "-g", "-Isrc/sensors", "-Isrc/flight",
                    str(c), "src/sensors/flow.c", "-o", str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], cwd=root, check=True)
