#include "closed_loop_radio.h"
#include "closed_loop.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static void Packet(ClosedLoopRadio *in, uint16_t seq, RadioCommand cmd, uint8_t keys,
                   uint32_t tx, uint32_t arrival)
{
    uint8_t p[12] = {0xa5,0x5a,2,(uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)cmd,keys,
                    (uint8_t)tx,(uint8_t)(tx>>8),(uint8_t)(tx>>16),(uint8_t)(tx>>24),0};
    p[11] = Radio_Crc(p,11);
    for (unsigned i=0;i<12;i++) ClosedLoopRadio_Feed(in,p[i],arrival);
}
static void Released(ClosedLoopRadio *in, uint16_t seq, uint32_t t)
{ Packet(in,seq,RADIO_HEARTBEAT,0,t,t); }
static void Packet3(ClosedLoopRadio *in, uint16_t seq, RadioCommand cmd,
                    uint8_t keys, uint8_t token, uint32_t t)
{
    uint8_t p[12]={0xa5,0x5a,3,(uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)cmd,
                  (uint8_t)(keys|((token&15U)<<4)),(uint8_t)t,(uint8_t)(t>>8),
                  (uint8_t)(t>>16),(uint8_t)(t>>24),0};
    p[11]=Radio_Crc(p,11);
    for(unsigned i=0;i<12;i++)ClosedLoopRadio_Feed(in,p[i],t);
}

int main(void)
{
    ClosedLoopRadio in; ClosedLoopRadio_Init(&in);
    Packet(&in,1,RADIO_START,2,10,10);
    assert(ClosedLoopRadio_Take(&in,10)==HOVER_NONE); /* held at startup */
    Packet(&in,2,RADIO_HEARTBEAT,2,20,20); assert(!in.ready);
    Released(&in,3,30); assert(in.ready);
    Packet(&in,4,RADIO_START,2,40,40);
    assert(ClosedLoopRadio_Take(&in,40)==HOVER_START);
    Packet(&in,5,RADIO_START,2,90,90);
    Packet(&in,6,RADIO_START,0,140,140);
    assert(ClosedLoopRadio_Take(&in,140)==HOVER_NONE); /* retries not re-arm */
    Released(&in,7,150);
    Packet(&in,8,RADIO_CALIBRATE,1,160,160);
    assert(ClosedLoopRadio_TakeCalibration(&in,1,160));
    Packet(&in,9,RADIO_CALIBRATE,0,210,210);
    assert(!ClosedLoopRadio_TakeCalibration(&in,1,210));
    Released(&in,10,220);
    Packet(&in,11,RADIO_CALIBRATE,1,230,230);
    assert(!ClosedLoopRadio_TakeCalibration(&in,0,230)); /* active/land/CCR nonzero */
    assert(!ClosedLoopRadio_TakeCalibration(&in,1,230));
    Packet(&in,12,RADIO_LAND,4,240,240);
    assert(ClosedLoopRadio_Take(&in,240)==HOVER_REQUEST_LAND);
    Packet(&in,13,RADIO_LAND,4,290,290);
    assert(ClosedLoopRadio_Take(&in,290)==HOVER_NONE);

    /* Same receive batch: STOP dominates START and a released heartbeat. */
    Released(&in,14,300); Packet(&in,15,RADIO_START,2,310,310);
    Packet(&in,16,RADIO_STOP,8,320,320); Released(&in,17,330);
    Packet(&in,18,RADIO_START,2,340,340);
    assert(ClosedLoopRadio_Take(&in,340)==HOVER_STOP); assert(!in.ready);
    Packet(&in,19,RADIO_START,2,350,350);
    assert(ClosedLoopRadio_Take(&in,350)==HOVER_NONE);
    Released(&in,20,360); Packet(&in,21,RADIO_START,2,370,370);
    assert(ClosedLoopRadio_Take(&in,370)==HOVER_START);
    Packet(&in,21,RADIO_STOP,8,370,380); /* duplicate seq must not refresh */
    assert(ClosedLoopRadio_Take(&in,380)==HOVER_NONE);
    assert(in.radio.last_packet_ms==370);
    assert(ClosedLoopRadio_LinkValid(&in,669)); assert(!ClosedLoopRadio_LinkValid(&in,670));
    Packet(&in,22,RADIO_START,2,680,680);
    assert(in.fault_latched && ClosedLoopRadio_Take(&in,680)==HOVER_NONE);
    Released(&in,23,690); assert(!in.ready && !ClosedLoopRadio_LinkValid(&in,690));
    Packet(&in,24,RADIO_STOP,8,700,700);
    assert(ClosedLoopRadio_Take(&in,700)==HOVER_STOP);
    Released(&in,25,710); Packet(&in,26,RADIO_START,2,720,720);
    assert(ClosedLoopRadio_Take(&in,720)==HOVER_START);
    /* A sender reboot faults the control link; heartbeat alone cannot clear. */
    Packet(&in,1,RADIO_HEARTBEAT,0,1,730);
    assert(in.restarts==1 && in.fault_latched && !in.ready);
    Packet(&in,2,RADIO_START,2,51,780);
    assert(ClosedLoopRadio_Take(&in,780)==HOVER_NONE);

    /* Old STOP + released HB + START cannot clear a live session's fault. */
    ClosedLoopRadio_Init(&in);
    Packet(&in,100,RADIO_HEARTBEAT,0,10000,100);
    Packet(&in,1,RADIO_STOP,8,50,110);
    assert(in.sequence==100 && in.radio.last_packet_ms==100);
    assert(in.fault_latched && ClosedLoopRadio_Take(&in,110)==HOVER_NONE);
    Packet(&in,2,RADIO_HEARTBEAT,0,100,120);
    Packet(&in,3,RADIO_START,2,150,130);
    assert(!in.ready && in.fault_latched && in.sequence==100);
    assert(ClosedLoopRadio_Take(&in,130)==HOVER_NONE);
    Packet(&in,101,RADIO_STOP,8,10050,150);
    assert(ClosedLoopRadio_Take(&in,150)==HOVER_STOP && !in.fault_latched);
    Packet(&in,102,RADIO_HEARTBEAT,0,10060,160);
    Packet(&in,103,RADIO_START,2,10070,170);
    assert(ClosedLoopRadio_Take(&in,170)==HOVER_START);

    /* Preserve stale event arrival: a fresh HB never revives old START. */
    ClosedLoopRadio_Init(&in); Released(&in,1,10);
    Packet(&in,2,RADIO_START,2,20,20); Released(&in,3,310);
    assert(ClosedLoopRadio_Take(&in,320)==HOVER_NONE);
    /* Sequence16 and both device/receiver uint32 clock wraps. */
    ClosedLoopRadio_Init(&in);
    Packet(&in,65535,RADIO_HEARTBEAT,0,UINT32_MAX-10,UINT32_MAX-10);
    Packet(&in,0,RADIO_START,2,10,10);
    assert(ClosedLoopRadio_Take(&in,10)==HOVER_START && !in.restarts);
    Packet(&in,65535,RADIO_START,2,UINT32_MAX-10,11);
    assert(ClosedLoopRadio_Take(&in,11)==HOVER_NONE && in.fault_latched);

    /* Split a valid frame over an RF boundary beyond the old 30ms timeout. */
    ClosedLoopRadio_Init(&in); Released(&in,1,1);
    uint8_t p[12]={0xa5,0x5a,2,2,0,RADIO_START,2,2,0,0,0,0};p[11]=Radio_Crc(p,11);
    for(unsigned i=0;i<12;i++)ClosedLoopRadio_Feed(&in,p[i],i<5?2:42);
    assert(ClosedLoopRadio_Take(&in,42)==HOVER_START);
    Released(&in,3,50);p[3]=4;p[5]=RADIO_STOP;p[6]=8;p[7]=60;p[11]=Radio_Crc(p,11)^1U;
    for(unsigned i=0;i<12;i++)ClosedLoopRadio_Feed(&in,p[i],60);
    assert(ClosedLoopRadio_Take(&in,60)==HOVER_NONE); /* bad STOP CRC rejected */
    assert(in.trace.crc_failures);
    /* Lost first key frame: its next distinct-seq retry yields one event. */
    Packet(&in,5,RADIO_START,2,70,70);
    assert(ClosedLoopRadio_Take(&in,70)==HOVER_START);
    Packet(&in,6,RADIO_START,0,120,120);
    assert(ClosedLoopRadio_Take(&in,120)==HOVER_NONE);

    /* Bridge and real closed-loop state machine: observable-height link loss
     * requests descent; reconnect HB/START do not resume, STOP plus release/
     * new START is required. */
    ClosedLoop closed; ClosedLoopOutput out;
    ClosedLoopConfig cfg={.15f,.20f,.20f,1,0};ClosedLoop_Init(&closed,cfg);
    ClosedLoopSample sample={.imu_valid=1,.link_valid=1,.tof_present=1,.tof_fresh=1,
                             .range_valid=1,.range_m=.10f};
    ClosedLoopRadio_Init(&in);Released(&in,1,10);Packet(&in,2,RADIO_START,2,20,20);
    ClosedLoop_Step(&closed,&sample,ClosedLoopRadio_Take(&in,20),.005f,&out);
    assert(out.state==CL_ASCEND && out.motor[0]>0);
    sample.link_valid=ClosedLoopRadio_LinkValid(&in,320);
    ClosedLoop_Step(&closed,&sample,HOVER_NONE,.005f,&out);
    assert(out.state==CL_LAND && out.fault==CL_OK && out.link_lost_landing);
    assert(out.collective>0 && out.motor[0]>0);
    Released(&in,3,330);Packet(&in,4,RADIO_START,2,340,340);
    sample.link_valid=ClosedLoopRadio_LinkValid(&in,340);
    ClosedLoop_Step(&closed,&sample,ClosedLoopRadio_Take(&in,340),.005f,&out);
    assert(out.state==CL_LAND && out.link_lost_landing && out.motor[0]>0);
    Packet(&in,5,RADIO_STOP,8,350,350);
    ClosedLoop_Step(&closed,&sample,ClosedLoopRadio_Take(&in,350),.005f,&out);
    assert(out.state==CL_IDLE && out.motor[0]==0);
    Released(&in,6,360);Packet(&in,7,RADIO_START,2,370,370);
    sample.link_valid=ClosedLoopRadio_LinkValid(&in,370);
    ClosedLoop_Step(&closed,&sample,ClosedLoopRadio_Take(&in,370),.005f,&out);
    assert(out.state==CL_ASCEND);
    /* V3 distinguishes two real same-key presses even without a transmitted
     * release between them; distinct transport-seq retries keep one token. */
    ClosedLoopRadio_Init(&in);
    Packet3(&in,1,RADIO_HEARTBEAT,0,0,10);
    Packet3(&in,2,RADIO_START,2,14,20);
    assert(ClosedLoopRadio_Take(&in,20)==HOVER_START && in.event_id==1);
    Packet3(&in,3,RADIO_START,2,14,70);
    assert(ClosedLoopRadio_Take(&in,70)==HOVER_NONE && in.event_id==1);
    Packet3(&in,4,RADIO_START,2,15,120);
    assert(ClosedLoopRadio_Take(&in,120)==HOVER_START && in.event_id==2);
    Packet3(&in,5,RADIO_START,2,0,170);
    assert(ClosedLoopRadio_Take(&in,170)==HOVER_START && in.event_id==3);
    Packet3(&in,6,RADIO_STOP,8,1,220);
    assert(ClosedLoopRadio_Take(&in,220)==HOVER_STOP && in.event_id==4);
    Packet3(&in,7,RADIO_STOP,8,1,260);
    assert(ClosedLoopRadio_Take(&in,260)==HOVER_STOP && in.event_id==4);
    Packet3(&in,8,RADIO_START,2,2,300);
    assert(ClosedLoopRadio_Take(&in,300)==HOVER_NONE && !in.ready);
    Packet3(&in,9,RADIO_HEARTBEAT,0,2,340);
    Packet3(&in,10,RADIO_START,2,3,380);
    assert(ClosedLoopRadio_Take(&in,380)==HOVER_START);
    puts("v2/v3 task bridge: per-press token, retries, release gate, stop priority, freshness, restart, wraps, CRC and controller recovery PASS (host)");
}
