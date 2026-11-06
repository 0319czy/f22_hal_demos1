#include "remote.h"
#include "radio.h"
#include "motor_guard.h"
#include "hover_sim.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void TestRemote(void)
{
    RemoteKeys keys;
    RemoteKeys_Init(&keys);
    assert(!RemoteKeys_Update(&keys, 0U));
    assert(RemoteKeys_Feed(&keys, 'F', UINT32_MAX - 100U));
    assert(keys.forward == 1 && keys.accepted == 1U);
    assert(RemoteKeys_Update(&keys, 100U));
    assert(!RemoteKeys_Feed(&keys, '?', 120U));
    assert(keys.rejected == 1U);
    assert(!RemoteKeys_Update(&keys, 149U));
    assert(keys.forward == 0);
    const char *commands = "FBLRUDWXYZ";
    for (unsigned i = 0U; commands[i]; ++i) {
        assert(RemoteKeys_Feed(&keys, (uint8_t)commands[i], 500U + i));
        assert(RemoteKeys_Update(&keys, 500U + i));
    }
    assert(keys.stop_requested && !keys.start_requested);
    assert(!RemoteKeys_Update(&keys, 1000U));
    assert(!keys.stop_requested && !keys.start_requested);
    assert(RemoteKeys_Feed(&keys, 'X', 2000U));
    assert(keys.start_requested);
    assert(!RemoteKeys_Feed(&keys, '\n', 2100U));
    assert(!RemoteKeys_Update(&keys, 2250U));
}
static void TestMotorGuard(void)
{
    MotorGuard g;
    MotorGuard_Init(&g);
    assert(!MotorGuard_Update(&g, 1U, 0U, 0U));
    assert(!MotorGuard_Update(&g, 1U, 0U, 3000U));
    assert(!MotorGuard_Update(&g, 0U, 0U, 4000U));
    assert(!MotorGuard_Update(&g, 1U, 0U, 5000U));
    assert(!MotorGuard_Update(&g, 1U, 0U, 6999U));
    assert(MotorGuard_Update(&g, 1U, 0U, 7000U) == 15U);
    assert(MotorGuard_Update(&g, 1U, 0U, 9999U) == 15U);
    assert(!MotorGuard_Update(&g, 1U, 0U, 10000U));
    assert(!MotorGuard_Update(&g, 1U, 0U, 15000U));
    assert(!MotorGuard_Update(&g, 0U, 0U, UINT32_MAX - 2500U));
    assert(!MotorGuard_Update(&g, 1U, 0U, UINT32_MAX - 1500U));
    assert(MotorGuard_Update(&g, 1U, 0U, 499U) == 15U);
    assert(!MotorGuard_Update(&g, 1U, 1U, 500U));
    assert(!MotorGuard_Update(&g, 1U, 0U, 3500U));
    (void)MotorGuard_Update(&g, 0U, 0U, 3501U);
    (void)MotorGuard_Update(&g, 1U, 0U, 3502U);
    assert(MotorGuard_Update(&g, 1U, 0U, 5502U) == 15U);
    assert(!MotorGuard_Update(&g, 0U, 0U, 5510U));
}
static void TestPid(void)
{
    FlightPid p = {.kp = 1.0f, .ki = 1.0f, .limit = 1.0f, .integral_limit = 0.1f};
    for (unsigned i = 0; i < 1000U; ++i)
        assert(FlightPid_Step(&p, 100.0f, 0.0f, 0.02f) == 1.0f);
    assert(p.integral == 0.0f);
    assert(FlightPid_Step(&p, -100.0f, 0.0f, 0.02f) == -1.0f);
    p.kp = 0.0f; p.ki = 0.0f; p.kd = 1.0f; p.limit = 100.0f;
    FlightPid_Reset(&p);
    assert(FlightPid_Step(&p, 0.0f, 0.0f, 0.02f) == 0.0f);
    assert(FlightPid_Step(&p, 10.0f, 0.0f, 0.02f) == 0.0f);
    assert(FlightPid_Step(&p, 0.0f, 1.0f, 0.02f) < 0.0f);
    assert(FlightPid_Step(&p, NAN, 0.0f, 0.02f) == 0.0f && !p.initialized);
    assert(FlightPid_Step(&p, 0.0f, 0.0f, 0.0f) == 0.0f);
}
static void AssertStopped(const HoverOutput *out)
{
    assert(out->collective == 0.0f);
    for (unsigned i = 0; i < 4U; ++i) assert(out->virtual_motor[i] == 0.0f);
    for (unsigned i = 0; i < 3U; ++i) assert(out->correction[i] == 0.0f);
}
static void TestHoverGates(void)
{
    HoverController c; HoverSample s; HoverOutput out;
    Hover_Init(&c); HoverSim_Init(&s);
    Hover_Step(&c, &s, HOVER_NONE, 0.02f, &out); AssertStopped(&out);
    for (unsigned fault = 0U; fault < 10U; ++fault) {
        Hover_Init(&c); HoverSim_Init(&s);
        Hover_Step(&c, &s, HOVER_START, 0.02f, &out);
        assert(out.state == HOVER_TAKEOFF);
        switch (fault) {
        case 0: s.imu_valid = 0U; break;
        case 1: s.height_valid = 0U; break;
        case 2: s.position_valid = 0U; break;
        case 3: s.command_link_valid = 0U; break;
        case 4: s.roll_deg = 35.0f; break;
        case 5: s.height_m = 1.01f; break;
        case 6: s.pitch_deg = NAN; break;
        case 7: s.gyro_dps[2] = INFINITY; break;
        case 8: s.vx_mps = NAN; break;
        default: s.height_m = -0.03f; break;
        }
        Hover_Step(&c, &s, HOVER_NONE, 0.02f, &out);
        assert(out.state == HOVER_FAULT); AssertStopped(&out);
        HoverSim_Init(&s);
        Hover_Step(&c, &s, HOVER_START, 0.02f, &out);
        assert(out.state == HOVER_FAULT); AssertStopped(&out);
        Hover_Step(&c, &s, HOVER_STOP, 0.02f, &out);
        assert(out.state == HOVER_IDLE); AssertStopped(&out);
    }
    Hover_Init(&c); HoverSim_Init(&s); s.height_m = 0.2f;
    Hover_Step(&c, &s, HOVER_START, 0.02f, &out);
    assert(out.state == HOVER_FAULT); AssertStopped(&out);
    Hover_Init(&c); HoverSim_Init(&s);
    Hover_Step(&c, &s, HOVER_START, 0.02f, &out);
    Hover_Step(&c, &s, HOVER_NONE, 0.1f, &out);
    assert(out.state == HOVER_FAULT); AssertStopped(&out);
    Hover_Init(&c); HoverSim_Init(&s);
    Hover_Step(&c, &s, HOVER_START, 0.02f, &out);
    Hover_Step(&c, &s, HOVER_REQUEST_LAND, 0.02f, &out);
    assert(out.state == HOVER_LAND);
    c.state = HOVER_HOLD; c.height_target_m = 0.5f; c.stable_seconds = 4.9f;
    s.height_m = 0.5f; s.x_m = 0.2f;
    Hover_Step(&c, &s, HOVER_NONE, 0.02f, &out);
    assert(c.stable_seconds == 0.0f && out.state == HOVER_HOLD);
}
static void TestMixer(void)
{
    float m[4];
    Hover_MixQuadX(0.45f, 0.05f, 0.0f, 0.0f, m);
    assert(fabsf(m[0] - 0.50f) < 1e-5f && fabsf(m[3] - 0.50f) < 1e-5f);
    assert(fabsf(m[1] - 0.40f) < 1e-5f && fabsf(m[2] - 0.40f) < 1e-5f);
    for (int r = -4; r <= 4; ++r) for (int p = -4; p <= 4; ++p)
        for (int y = -4; y <= 4; ++y) {
            Hover_MixQuadX(0.45f, (float)r, (float)p, (float)y, m);
            for (unsigned i = 0; i < 4U; ++i) assert(isfinite(m[i]) && m[i] >= 0 && m[i] <= 1);
        }
    Hover_MixQuadX(0.5f, FLT_MAX, FLT_MAX, FLT_MAX, m);
    for (unsigned i = 0; i < 4U; ++i) assert(m[i] == 0.0f);
    Hover_MixQuadX(NAN, 0, 0, 0, m);
    for (unsigned i = 0; i < 4U; ++i) assert(m[i] == 0.0f);
}
static void TestModelCycle(void)
{
    HoverController c; HoverSample s; HoverOutput out;
    unsigned held = 0U, landed = 0U, finished = 0U;
    float max_height = 0.0f;
    Hover_Init(&c); HoverSim_Init(&s);
    for (unsigned step = 0U; step < 1500U; ++step) {
        HoverCommand command = step == 0U ? HOVER_START :
            (step == 850U ? HOVER_REQUEST_LAND : HOVER_NONE);
        if (step == 849U) assert(c.state == HOVER_HOLD && c.stable_seconds > 5.0f);
        Hover_Step(&c, &s, command, 0.02f, &out);
        assert(out.state != HOVER_FAULT);
        held |= out.state == HOVER_HOLD; landed |= out.state == HOVER_LAND;
        for (unsigned i = 0; i < 4U; ++i)
            assert(isfinite(out.virtual_motor[i]) && out.virtual_motor[i] >= 0 && out.virtual_motor[i] <= 1);
        HoverSim_Step(&s, &out, 0.02f);
        max_height = fmaxf(max_height, s.height_m);
        if (landed && out.state == HOVER_IDLE) { finished = 1U; break; }
    }
    assert(held && landed && finished && max_height <= 1.0f);
    AssertStopped(&out);
    printf("software hover cycle passed; simulated max height %.3f m\n", (double)max_height);
}
static void TestRadio(void)
{
    RadioReceiver r; uint8_t p[6]; Radio_Init(&r);
    assert(!Radio_LinkValid(&r,0));
    Radio_Encode(254,RADIO_START,p);
    for(unsigned i=0;i<6;i++) assert(Radio_Feed(&r,p[i],UINT32_MAX-100)==(i==5));
    assert(r.command==RADIO_START && Radio_LinkValid(&r,100));
    assert(!Radio_LinkValid(&r,199));
    for(unsigned i=0;i<6;i++) assert(!Radio_Feed(&r,p[i],100)); /* duplicate */
    assert(!Radio_LinkValid(&r,199));
    Radio_Encode(255,RADIO_HEARTBEAT,p);
    for(unsigned i=0;i<6;i++) (void)Radio_Feed(&r,p[i],200);
    assert(Radio_LinkValid(&r,200));
    Radio_Encode(0,RADIO_LAND,p); /* uint8 sequence wrap */
    for(unsigned i=0;i<6;i++) (void)Radio_Feed(&r,p[i],225);
    assert(r.command==RADIO_LAND && r.sequence==0);
    p[4]=RADIO_START; /* corrupt CRC must not execute a different command */
    for(unsigned i=0;i<6;i++) assert(!Radio_Feed(&r,p[i],250));
    assert(r.command==RADIO_LAND && r.last_packet_ms==225);
    Radio_Encode(1,RADIO_STOP,p);
    (void)Radio_Feed(&r,0x12,260); /* noise */
    for(unsigned i=0;i<6;i++) (void)Radio_Feed(&r,p[i],260);
    assert(r.command==RADIO_STOP && r.accepted==4);
    Radio_Encode(2,RADIO_START,p);
    for(unsigned i=0;i<3;i++) (void)Radio_Feed(&r,p[i],280);
    for(unsigned i=3;i<6;i++) assert(!Radio_Feed(&r,p[i],320)); /* interbyte timeout */
    assert(r.command==RADIO_STOP);
    assert(!Radio_LinkValid(&r,560));
    Radio_Encode(3,RADIO_HEARTBEAT,p);
    for(unsigned i=0;i<6;i++) (void)Radio_Feed(&r,p[i],600);
    assert(Radio_LinkValid(&r,600) && r.command==RADIO_HEARTBEAT);
    Radio_Encode(4,(RadioCommand)99,p);
    for(unsigned i=0;i<6;i++) assert(!Radio_Feed(&r,p[i],620));
}
int main(void)
{
    TestRemote(); TestRadio(); TestMotorGuard(); TestPid(); TestHoverGates(); TestMixer(); TestModelCycle();
    puts("remote, motor guard, PID, safety gates and virtual mixer passed");
    return 0;
}
