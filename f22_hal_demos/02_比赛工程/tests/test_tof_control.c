#include "tof_control.h"
#include "closed_loop.h"
#include "closed_loop_config.h"
#include "observation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint32_t Now(void *context) { return *(uint32_t *)context; }
static void SetRange(TofSensor *tof, uint32_t now, unsigned mm)
{
    tof->last_sample_ms = now - 4U;
    tof->distance_mm = (uint16_t)mm;
    tof->valid = tof->raw_status == 9U && mm >= 40U && mm <= 4000U;
}
static void Tick(ClosedLoop *c, TofSensor *tof, ClosedLoopSample *s,
                 HoverCommand cmd, ClosedLoopOutput *out)
{
    s->tof_present = tof->present;
    s->tof_fresh = Tof_ControlFresh(tof, c->state != CL_IDLE && s->range_valid);
    s->tof_near_ground = Tof_ControlNearGround(tof);
    ClosedLoop_Step(c, s, cmd, .006f, out);
    for (unsigned i = 0; i < 4; ++i)
        assert(isfinite(out->motor[i]) && out->motor[i] >= 0 && out->motor[i] <= 1);
}
static void Stopped(const ClosedLoopOutput *out)
{ for (unsigned i = 0; i < 4; ++i) assert(out->motor[i] == 0); }

int main(void)
{
    uint32_t now = 426477U;
    TofSensor tof = {.present=1, .seen=1, .raw_status=9,
                     .io={.context=&now, .now_ms=Now}};
    /* Recorded fault: status9, 2mm, age4ms. Synthetic boundaries also cover
     * every under-range value; none becomes valid height or touchdown proof. */
    for (unsigned mm = 0; mm <= 40; ++mm) {
        SetRange(&tof, now, mm);
        assert(Tof_ControlFresh(&tof, 0U));
        assert(tof.valid == (mm >= 40U));
        assert(Tof_ControlNearGround(&tof) == (mm >= 5U && mm < 40U));
    }
    tof.seen = 0; assert(!Tof_ControlFresh(&tof, 1U)); tof.seen = 1;
    tof.present = 0; assert(!Tof_ControlFresh(&tof, 1U)); tof.present = 1;
    tof.last_sample_ms = now - TOF_MAX_AGE_MS;
    assert(!Tof_ControlFresh(&tof, 1U));
    SetRange(&tof, now, 2);
    tof.raw_status = 4;
    assert(!Tof_ControlFresh(&tof, 0U));
    assert(Tof_ControlFresh(&tof, 1U)); /* only an already-valid prediction */
    assert(!Tof_ControlNearGround(&tof));
    tof.raw_status = 9;
    SetRange(&tof, now, 4001);
    assert(!Tof_ControlFresh(&tof, 0U));

    ClosedLoop c; ClosedLoopOutput out;
    ClosedLoopConfig cfg = {1, 1, .20f, 1, 1};
    ClosedLoopSample s = {.imu_valid=1, .link_valid=1,
                         .flow_present=1, .flow_control_provisional=1};
    HeightEstimate height; Height_Reset(&height);
    ClosedLoop_Init(&c, cfg);
    /* The same adapter used by main must allow bounded ground launch across
     * fresh 0..4mm readings without manufacturing an inertial height anchor. */
    const unsigned ranges[] = {0, 2, 4, 9, 39};
    for (unsigned i = 0; i < 370; ++i) {
        now += 6U; SetRange(&tof, now, ranges[i % 5]);
        if (tof.valid) (void)Height_Correct(&height, tof.distance_mm*.001f, 0, 0, now);
        assert(!height.valid);
        s.range_m = c.state == CL_IDLE ? tof.distance_mm*.001f : height.height;
        Tick(&c, &tof, &s, i ? HOVER_NONE : HOVER_START, &out);
        assert(c.state == CL_LAUNCH && c.fault == CL_OK);
        assert(out.flow_gain == 0 && !out.flow_position_hold);
    }
    assert(c.collective > .99f); /* 100% ceiling, not an early fault6 */
    /* F4 remains immediate, independent of under-range measurement. */
    Tick(&c, &tof, &s, HOVER_STOP, &out); assert(c.state == CL_IDLE); Stopped(&out);
    Tick(&c, &tof, &s, HOVER_START, &out); assert(c.state == CL_LAUNCH);
    for (unsigned i = 0; i < 510; ++i) {
        now += 6U; SetRange(&tof, now, 2);
        Tick(&c, &tof, &s, HOVER_NONE, &out);
    }
    assert(c.state == CL_FAULT && c.fault == CL_LAUNCH_TIMEOUT); Stopped(&out);
    /* True stale data and missing hardware must still stop the launch: hardware
     * absence is immediate, while a fresh-sample gap is tolerated only inside
     * the CL_TOF_GAP_GRACE_SEC window (added 2026-10-27 after one bad
     * conversion killed a real launch at 581 permille). */
    for (unsigned missing = 0; missing < 2; ++missing) {
        ClosedLoop_Init(&c, cfg); SetRange(&tof, now, 2);
        Tick(&c, &tof, &s, HOVER_START, &out); assert(c.state == CL_LAUNCH);
        if (missing) {
            tof.present = 0;
            Tick(&c, &tof, &s, HOVER_NONE, &out);
            assert(c.state == CL_FAULT && c.fault == CL_TOF_LOST); Stopped(&out);
        } else {
            tof.last_sample_ms = now - TOF_MAX_AGE_MS;
            Tick(&c, &tof, &s, HOVER_NONE, &out);
            assert(c.state == CL_LAUNCH && c.fault == CL_OK);
            for (unsigned i = 0; i < 100 && c.state == CL_LAUNCH; ++i)
                Tick(&c, &tof, &s, HOVER_NONE, &out);
            assert(c.state == CL_FAULT && c.fault == CL_TOF_LOST); Stopped(&out);
        }
        tof.present = 1;
    }
    /* Gain a REAL height anchor: the corrected adapter must not prevent
     * height/flow handover or relabel zero as a valid airborne measurement. */
    ClosedLoop_Init(&c, cfg); SetRange(&tof, now, 9);
    Tick(&c, &tof, &s, HOVER_START, &out);
    now += 6; SetRange(&tof, now, 100);
    assert(Height_Correct(&height, .10f, 0, 0, now));
    s.range_m = height.height; s.range_valid = height.valid; s.flow_valid = 1;
    Tick(&c, &tof, &s, HOVER_NONE, &out);
    assert(c.state == CL_ASCEND && out.flow_gain > 0);
    SetRange(&tof, now, 0); s.range_valid = 0;
    Tick(&c, &tof, &s, HOVER_NONE, &out);
    assert(c.state == CL_FAULT && c.fault == CL_TOF_LOST); Stopped(&out);
    puts("PASS: recorded ToF 2mm/age4ms regression, blind-zone boundaries, no fabricated height/touchdown, bounded launch, real handover, stale/missing/stop guards (host only)");
    return 0;
}
