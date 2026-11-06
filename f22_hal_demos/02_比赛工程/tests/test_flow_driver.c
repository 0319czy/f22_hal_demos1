#include "flow.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint32_t now;
    int fail_read, fail_write;
    uint8_t id, inverse, quality;
    unsigned reads, writes, delays, bank_selected, reset_done;
} Bus;
static int Read(void *ctx, uint16_t reg, uint8_t *data, size_t n)
{
    Bus *b = ctx; b->reads++;
    if (reg == b->fail_read) return -7;
    memset(data, 0, n);
    if (reg == 0 || reg == 0x5f) assert(b->bank_selected && b->reset_done);
    if (reg == 0) *data = b->id;
    if (reg == 0x5f) *data = b->inverse;
    if (reg == 0x16) { assert(n == 12); data[2] = 3; data[4] = 0xfe; data[5] = 0xff; data[6] = b->quality; }
    return 0;
}
static int Write(void *ctx, uint16_t reg, const uint8_t *data, size_t n)
{
    Bus *b = ctx; b->writes++; assert(n == 1);
    if (reg == b->fail_write) return -9;
    if (reg == 0x7f && *data == 0) b->bank_selected = 1;
    if (reg == 0x3a) { assert(b->bank_selected && *data == 0x5a); b->reset_done = 1; }
    return 0;
}
static uint32_t Now(void *ctx) { return ((Bus *)ctx)->now; }
static void Delay(void *ctx, uint32_t ms)
{ Bus *b = ctx; b->delays++; b->now += ms; }
static SensorIo Io(Bus *b) { SensorIo io = {b,Read,Write,Now,Delay}; return io; }
static Bus Healthy(void) { Bus b = {0}; b.id = 0x49; b.inverse = 0xb6; b.quality = 120; b.fail_read = b.fail_write = -1; return b; }
static int Run(FlowSensor *s, Bus *b)
{
    int result = 0; unsigned delays = b->delays;
    for (unsigned i = 0; i < 1000 && !result; ++i) {
        unsigned operations = b->reads + b->writes;
        result = Flow_InitStep(s);
        assert(b->reads+b->writes-operations <= 1); /* bounded, no table loop */
        assert(b->delays == delays); /* every wait is timestamp-based */
        b->now += 5;
    }
    assert(result); return result;
}
int main(void)
{
    const struct { int reg; unsigned write; FlowStage stage; unsigned mask; } faults[] = {
        {0x7f,1,FLOW_STAGE_BANK_SELECT,0},
        {0x3a,1,FLOW_STAGE_RESET_WRITE,0}, {0,0,FLOW_STAGE_ID_READ,0},
        {0x5f,0,FLOW_STAGE_INVERSE_READ,1}, {4,0,FLOW_STAGE_CLEAR_READ,3},
        {0x61,1,FLOW_STAGE_CONFIG_WRITE,3}
    };
    for (unsigned i = 0; i < sizeof(faults)/sizeof(faults[0]); ++i) {
        Bus b = Healthy(); FlowSensor s;
        if (faults[i].write) b.fail_write = faults[i].reg; else b.fail_read = faults[i].reg;
        assert(Flow_Init(&s, Io(&b)) == -1);
        assert(!s.present && !s.valid && s.errors == 1 && s.attempts == 1);
        assert(s.fault == faults[i].stage && s.id_read_mask == faults[i].mask);
        assert(s.last_reg == faults[i].reg && s.io_status == (faults[i].write ? -9 : -7));
        Flow_BeginRetry(&s); assert(Run(&s,&b) == -1);
        assert(s.errors == 2 && s.attempts == 2);
        b.fail_read = b.fail_write = -1; Flow_BeginRetry(&s);
        assert(Run(&s,&b) == 1 && s.present && !s.fault && s.errors == 2 && s.attempts == 3);
    }
    for (unsigned id = 0; id < 2; ++id) {
        Bus b = Healthy(); FlowSensor s; b.id = id ? 0xff : 0;
        assert(Flow_Init(&s,Io(&b)) == -1 && s.fault == FLOW_STAGE_ID_CHECK);
        assert(s.id_read_mask == 3 && s.io_status == 0 && s.id == b.id);
    }
    /* Reproduce the operator's healthy transport but wrong inverse value.
     * It must remain unrecognized, not be accepted from product ID alone. */
    Bus wrong = Healthy(); FlowSensor absent; wrong.inverse = 0xff;
#ifdef FLOW_ID_PROBE
    assert(Flow_Init(&absent,Io(&wrong)) == 0 && absent.probe_only);
    assert(!absent.present && absent.fault == FLOW_STAGE_ID_CHECK && absent.inverse_id == 0xff);
    assert(Flow_Poll(&absent) == 0 && !absent.valid && absent.quality == 120);
    assert(!absent.present && absent.dx == 3 && absent.dy == -2);
    wrong.fail_read = 0x16;
    assert(Flow_Poll(&absent) == -1 && !absent.probe_only && !absent.present);
#else
    assert(Flow_Init(&absent,Io(&wrong)) == -1);
    assert(!absent.present && absent.id == 0x49 && absent.inverse_id == 0xff);
    assert(absent.fault == FLOW_STAGE_ID_CHECK && absent.io_status == 0);
#endif
    Bus b = Healthy(); FlowSensor s;
    assert(Flow_Init(&s,Io(&b)) == 0 && s.present);
    /* Raw SPI capture is independent of height; poor texture is not absence. */
    assert(Flow_Poll(&s) == 0 && s.valid && s.dx == 3 && s.dy == -2);
    b.quality = 0; assert(Flow_Poll(&s) == 0 && !s.valid && s.present && !s.fault);
    b.fail_read = 0x16;
    assert(Flow_Poll(&s) == -1 && !s.present && !s.valid && s.fault == FLOW_STAGE_BURST_READ);
    assert(s.dx == 0 && s.dy == 0 && s.io_status == -7);
    b.fail_read = -1; Flow_BeginRetry(&s); assert(Run(&s,&b) == 1);
    b.fail_read = 0x5f;
    assert(Flow_CheckIdentity(&s) == -1 && !s.present && s.id_read_mask == 1);
    assert(s.fault == FLOW_STAGE_RECHECK_INVERSE && s.errors == 2);
    b.fail_read = -1; Flow_BeginRetry(&s); assert(Run(&s,&b) == 1);
    b.id = 0xff;
    assert(Flow_CheckIdentity(&s) == -1 && s.fault == FLOW_STAGE_ID_CHECK && s.errors == 3);
    b.id = 0x49; b.now = UINT32_MAX - 20U;
    Flow_BeginRetry(&s); assert(Run(&s,&b) == 1); /* wait deadlines wrap */
    puts("Flow driver: stage/IO/ID masks, accumulated retries, nonblocking steps, height-independent raw capture, quality vs absence, periodic ID and wrap passed (synthetic I/O)");
    return 0;
}
