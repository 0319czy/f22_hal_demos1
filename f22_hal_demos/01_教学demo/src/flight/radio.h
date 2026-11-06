#ifndef F22_RADIO_H
#define F22_RADIO_H
#include <stdint.h>
#define RADIO_LINK_TIMEOUT_MS 300U
#define RADIO_PACKET_SIZE 6U
typedef enum { RADIO_HEARTBEAT=0, RADIO_START=1, RADIO_LAND=2, RADIO_STOP=3, RADIO_CALIBRATE=4 } RadioCommand;
typedef struct {
    uint8_t packet[6], used, seen, sequence, command;
    uint32_t last_byte_ms,last_packet_ms;
    unsigned accepted,rejected;
} RadioReceiver;
uint8_t Radio_Crc(const uint8_t *data,unsigned n);
void Radio_Init(RadioReceiver *r);
unsigned Radio_Feed(RadioReceiver *r,uint8_t byte,uint32_t now);
unsigned Radio_LinkValid(const RadioReceiver *r,uint32_t now);
void Radio_Encode(uint8_t sequence,RadioCommand command,uint8_t packet[6]);
#endif
