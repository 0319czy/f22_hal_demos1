#include "radio.h"
#include <string.h>
uint8_t Radio_Crc(const uint8_t *data,unsigned n)
{
    uint8_t crc=0;
    for(unsigned i=0;i<n;i++) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;bit++)crc=(uint8_t)((crc&0x80U)?((unsigned)crc<<1)^0x07U:(unsigned)crc<<1);
    }
    return crc;
}
void Radio_Init(RadioReceiver *r){memset(r,0,sizeof(*r));}
void Radio_Encode(uint8_t seq,RadioCommand cmd,uint8_t p[6])
{p[0]=0xa5;p[1]=0x5a;p[2]=1;p[3]=seq;p[4]=(uint8_t)cmd;p[5]=Radio_Crc(p,5);}
unsigned Radio_LinkValid(const RadioReceiver *r,uint32_t now)
{return r->seen && (uint32_t)(now-r->last_packet_ms)<RADIO_LINK_TIMEOUT_MS;}
unsigned Radio_Feed(RadioReceiver *r,uint8_t byte,uint32_t now)
{
    if((uint32_t)(now-r->last_byte_ms)>30U)r->used=0;
    r->last_byte_ms=now;
    r->packet[r->used++]=byte;
    if(r->used<6)return 0;
    uint8_t *p=r->packet;
    unsigned valid=p[0]==0xa5 && p[1]==0x5a && p[2]==1 && p[4]<=RADIO_CALIBRATE && p[5]==Radio_Crc(p,5);
    if(valid) {
        uint8_t advance=(uint8_t)(p[3]-r->sequence);
        if(Radio_LinkValid(r,now) && (advance==0 || advance>=128U)) {
            r->rejected++;r->used=0;return 0; /* duplicates do not extend freshness */
        }
        r->seen=1;r->sequence=p[3];r->command=p[4];r->last_packet_ms=now;r->accepted++;r->used=0;return 1;
    }
    r->rejected++;
    memmove(p,p+1,5);r->used=5; /* sliding window resynchronizes after corruption */
    return 0;
}
