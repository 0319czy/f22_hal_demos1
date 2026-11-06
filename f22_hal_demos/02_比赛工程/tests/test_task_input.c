#include "task_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static void Packet(TaskInput *in, uint8_t sequence, RadioCommand command, uint32_t now)
{
    uint8_t packet[6]; Radio_Encode(sequence, command, packet);
    for (unsigned i = 0; i < 6; i++) TaskInput_Feed(in, packet[i], now);
}
static HoverSample ValidSample(void)
{
    HoverSample sample = {.height_m=.05f, .imu_valid=1, .height_valid=1,
                         .position_valid=1, .command_link_valid=1};
    return sample;
}
int main(void)
{
    TaskInput in; TaskInput_Init(&in);
    Packet(&in, 1, RADIO_START, 10); Packet(&in, 2, RADIO_HEARTBEAT, 20);
    assert(TaskInput_Take(&in, 20) == HOVER_START); /* heartbeat keeps the event */
    assert(TaskInput_Take(&in, 20) == HOVER_NONE);
    Packet(&in, 3, RADIO_START, 30); Packet(&in, 4, RADIO_LAND, 40);
    Packet(&in, 5, RADIO_START, 50); assert(TaskInput_Take(&in, 50) == HOVER_REQUEST_LAND);
    Packet(&in, 6, RADIO_STOP, 60); Packet(&in, 7, RADIO_LAND, 70);
    Packet(&in, 8, RADIO_START, 80); assert(TaskInput_Take(&in, 80) == HOVER_STOP);
    Packet(&in, 9, RADIO_START, 90); Packet(&in, 10, RADIO_HEARTBEAT, 400);
    assert(Radio_LinkValid(&in.radio, 400));
    assert(TaskInput_Take(&in, 400) == HOVER_NONE); /* fresh heartbeat cannot revive stale START */
    Packet(&in, 11, RADIO_CALIBRATE, 410); Packet(&in, 12, RADIO_HEARTBEAT, 420);
    assert(TaskInput_TakeCalibration(&in, HOVER_IDLE, 420));
    assert(!TaskInput_TakeCalibration(&in, HOVER_IDLE, 420));
    Packet(&in, 13, RADIO_CALIBRATE, 430);
    assert(!TaskInput_TakeCalibration(&in, HOVER_HOLD, 430));
    assert(!TaskInput_TakeCalibration(&in, HOVER_IDLE, 430));
    Packet(&in, 14, RADIO_CALIBRATE, 440); Packet(&in, 15, RADIO_STOP, 450);
    assert(!TaskInput_TakeCalibration(&in, HOVER_IDLE, 450));
    assert(TaskInput_Take(&in, 450) == HOVER_STOP);
    Packet(&in, 16, RADIO_CALIBRATE, 460); Packet(&in, 17, RADIO_HEARTBEAT, 770);
    assert(!TaskInput_TakeCalibration(&in, HOVER_IDLE, 770));
    TaskInput_Init(&in); Packet(&in, 255, RADIO_START, UINT32_MAX-20);
    Packet(&in, 0, RADIO_HEARTBEAT, 10); assert(TaskInput_Take(&in, 10) == HOVER_START);
    Packet(&in, 0, RADIO_START, 20); assert(TaskInput_Take(&in, 20) == HOVER_NONE); /* duplicate */
    /* Consumer time must be sampled AFTER draining ISR bytes (arrival can be later
     * than the loop-start timestamp); the production loop does exactly this. */
    TaskInput_Init(&in); Packet(&in, 1, RADIO_START, 201);
    assert(TaskInput_Take(&in, 201) == HOVER_START);
    Packet(&in, 2, RADIO_CALIBRATE, 202);
    assert(TaskInput_TakeCalibration(&in, HOVER_IDLE, 202));

    HoverController controller; HoverOutput output; HoverSample sample = ValidSample();
    Hover_Init(&controller); TaskInput_Init(&in);
    Packet(&in, 1, RADIO_START, 100); Packet(&in, 2, RADIO_HEARTBEAT, 110);
    Hover_Step(&controller, &sample, TaskInput_Take(&in, 110), .02f, &output);
    assert(output.state == HOVER_TAKEOFF);
    Packet(&in, 3, RADIO_LAND, 120);
    Hover_Step(&controller, &sample, TaskInput_Take(&in, 120), .02f, &output);
    assert(output.state == HOVER_LAND); /* early requested landing is permitted */
    sample.command_link_valid = Radio_LinkValid(&in.radio, 420);
    Hover_Step(&controller, &sample, HOVER_NONE, .02f, &output);
    assert(output.state == HOVER_FAULT);
    for (unsigned i = 0; i < 4; i++) assert(output.virtual_motor[i] == 0);
    Packet(&in, 4, RADIO_STOP, 430);
    Hover_Step(&controller, &sample, TaskInput_Take(&in, 430), .02f, &output);
    assert(output.state == HOVER_IDLE);
    sample.command_link_valid = 1; sample.imu_valid = 0;
    Packet(&in, 5, RADIO_START, 440);
    Hover_Step(&controller, &sample, TaskInput_Take(&in, 440), .02f, &output);
    assert(output.state == HOVER_FAULT);
    puts("competition task bridge passed: priorities, freshness, calibration, wrap, controller gates (host only)");
}
