/* TLE100 AT89S52, 11.0592MHz, SDCC mcs51. Teaching transmitter, no actuators.
 * New protocol: A5 5A 01 sequence command CRC8(poly07,initial0), heartbeat every20ms.
 * Original character receiver firmware is NOT compatible with these packets. */
#include <8052.h>
typedef unsigned char u8;
__sbit __at(0x91) RADIO_MODE1; /* P1.1; manual WL_M1 */
__sbit __at(0x92) RADIO_MODE0; /* P1.2; manual WL_M0 */
static void Wait20ms(void)
{
    /* Timer0: 11.0592MHz /12 ->921600Hz;18432counts=20ms. */
    TR0=0; TH0=0xb8; TL0=0; TF0=0; TR0=1;
    while(!TF0) { }
    TR0=0;TF0=0;
}
static void Send(u8 b){TI=0;SBUF=b;while(!TI){}TI=0;}
static u8 Crc(const u8 *p)
{
    u8 crc=0,i,bit;
    for(i=0;i<5;i++){crc^=p[i];for(bit=0;bit<8;bit++)crc=(crc&0x80)?(crc<<1)^7:crc<<1;}
    return crc;
}
static u8 Keys(void)
{
    u8 result=0;
    if(!(P0&0x40))result|=1; /* F1 calibrate */
    if(!(P0&0x80))result|=2; /* F2 one-key takeoff */
    if(!(P1&0x08))result|=4; /* F3 one-key landing */
    if(!(P1&0x10))result|=8; /* F4 emergency stop */
    return result;
}
void main(void)
{
    u8 p[6]={0xa5,0x5a,1,0,0,0},i,previous,stable,candidate,last_raw;
    u8 pending=0,repeats=0;
    EA=0; P0=0xff;P1=0xff;RADIO_MODE0=0;RADIO_MODE1=0;
    PCON&=0x7f;TMOD=0x21;TH1=TL1=0xfd;SCON=0x50;TR1=1;
    previous=stable=candidate=last_raw=Keys(); /* held-at-boot keys cannot arm */
    while(1) {
        u8 raw=Keys(),edges,cmd=0;
        if(raw==last_raw)candidate=raw;else candidate=stable; /* two scans debounce */
        last_raw=raw;stable=candidate;edges=stable&~previous;previous=stable;
        /* Stop remains repeated while held, so one lost event cannot suppress it.
         * START/LAND edge lasts three packets (~79ms) to tolerate transient loss. */
        if(stable&8){pending=0;repeats=0;cmd=3;}
        else {
            if(edges&4){pending=2;repeats=3;}
            else if(edges&2){pending=1;repeats=3;}
            else if(edges&1){pending=4;repeats=3;}
            if(repeats){cmd=pending;repeats--;}
        }
        p[3]++;p[4]=cmd;p[5]=Crc(p);
        for(i=0;i<6;i++)Send(p[i]);
        Wait20ms(); /* actual period adds UART airtime ~6.25ms ->~26ms */
    }
}
