#include "motor_radio.h"
#include <assert.h>
#include <stdio.h>
static void Packet(MotorRadio *c, uint8_t seq, RadioCommand command, uint32_t now)
{
    uint8_t p[6]; Radio_Encode(seq, command, p);
    for (unsigned i = 0; i < 6; ++i) MotorRadio_Feed(c, p[i], now);
}
static void Expect(const float motor[4], float value)
{ for (unsigned i = 0; i < 4; ++i) assert(motor[i] == value); }
int main(void)
{
    MotorRadio c; float motor[4]; MotorRadio_Init(&c);
    MotorRadio_Step(&c, 0, 0, 0, 0, motor); Expect(motor, 0);
    Packet(&c, 1, RADIO_CALIBRATE, 10); /* Startup command without prior heartbeat. */
    MotorRadio_Step(&c, 0, 0, 0, 10, motor); Expect(motor, 0);
    Packet(&c, 2, RADIO_HEARTBEAT, 36);
    Packet(&c, 3, RADIO_START, 62); /* F2 is intentionally not a start key here. */
    MotorRadio_Step(&c, 0, 0, 0, 62, motor); Expect(motor, 0);
    Packet(&c, 4, RADIO_CALIBRATE, 88);
    Packet(&c, 5, RADIO_CALIBRATE, 114);
    Packet(&c, 6, RADIO_HEARTBEAT, 140);
    MotorRadio_Step(&c, 0, 0, 0, 140, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    assert(c.starts == 1);
    Packet(&c, 7, RADIO_CALIBRATE, 166);
    MotorRadio_Step(&c, 0, 0, 0, 166, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    assert(c.starts == 1); /* Repeated F1 packets never toggle or restart. */
    uint8_t seq = 7;
    for (uint32_t now = 192; now < 60192; now += 26) {
        Packet(&c, ++seq, RADIO_HEARTBEAT, now);
        MotorRadio_Step(&c, 0, 0, 0, now, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    }
    uint32_t last = c.receiver.last_packet_ms;
    MotorRadio_Step(&c, 0, 0, 0, last + 299, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    MotorRadio_Step(&c, 0, 0, 0, last + 300, motor); Expect(motor, 0);
    assert(c.disconnects == 1 && c.stop_reason == 3 && c.clear_output);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 301);
    MotorRadio_Step(&c, 0, 0, 0, last + 301, motor); Expect(motor, 0);
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 327);
    MotorRadio_Step(&c, 0, 0, 0, last + 327, motor); Expect(motor, 0);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 353);
    MotorRadio_Step(&c, 0, 0, 0, last + 353, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    Packet(&c, ++seq, RADIO_STOP, last + 379);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 405);
    MotorRadio_Step(&c, 0, 0, 0, last + 405, motor); Expect(motor, 0);
    assert(c.stop_reason == 2 && c.clear_output);
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 431);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 457);
    MotorRadio_Step(&c, 1, 1, 0, last + 457, motor); Expect(motor, 0);
    assert(c.stop_reason == 1);
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 483);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 509);
    MotorRadio_Step(&c, 0, 0, 0, last + 509, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    MotorRadio_Step(&c, 0, 1, 1, last + 510, motor); Expect(motor, 0);
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 535);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 561);
    MotorRadio_Step(&c, 0, 0, 0, last + 561, motor); Expect(motor, 0);
    Packet(&c, ++seq, RADIO_LAND, last + 587);
    MotorRadio_Step(&c, 0, 1, 0, last + 587, motor); Expect(motor, 0);
    assert(c.clear_output && c.motor.state == CL_IDLE);
    /* Fresh heartbeat cannot revive an old buffered start. */
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 613);
    Packet(&c, ++seq, RADIO_CALIBRATE, last + 639);
    Packet(&c, ++seq, RADIO_HEARTBEAT, last + 940);
    MotorRadio_Step(&c, 0, 0, 0, last + 940, motor); Expect(motor, 0);
    MotorRadio_Init(&c);
    Packet(&c, 254, RADIO_HEARTBEAT, UINT32_MAX - 50);
    Packet(&c, 255, RADIO_CALIBRATE, UINT32_MAX - 24);
    Packet(&c, 0, RADIO_HEARTBEAT, 1);
    MotorRadio_Step(&c, 0, 0, 0, 1, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    Packet(&c, 3, RADIO_HEARTBEAT, 27); assert(c.sequence_missing == 2);
    unsigned accepted = c.receiver.accepted;
    Packet(&c, 3, RADIO_STOP, 28); assert(c.receiver.accepted == accepted);
    MotorRadio_Step(&c, 0, 0, 0, 28, motor); Expect(motor, MOTOR_CONTINUOUS_DUTY);
    uint8_t bad[6]; Radio_Encode(4, RADIO_STOP, bad); bad[5] ^= 1;
    for (unsigned i = 0; i < 6; ++i) MotorRadio_Feed(&c, bad[i], 30);
    assert(c.receiver.accepted == accepted);
    Packet(&c, 4, RADIO_STOP, 53);
    MotorRadio_Step(&c, 0, 0, 0, 53, motor); Expect(motor, 0);
    puts("Radio motor bench passed: F1 start, repeats, 60s demands, stop priority, 300ms link loss, no reconnect restart, UART fault latch, stale commands, wrap, CRC/sequence checks (synthetic inputs)");
    return 0;
}
