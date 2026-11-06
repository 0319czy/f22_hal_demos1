/* PMW3901 Arduino driver
 * Copyright (c) 2017 Bitcraze AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Initialization sequence adapted from Bitcraze's MIT Arduino driver.
 * Register I/O is injected; F22 adapter uses SPI2 mode3 at 1.125MHz. */
#include "flow.h"
#include <string.h>
static const uint8_t init_sequence[][2] = {
    {0x7F, 0x00},
    {0x61, 0xAD},
    {0x7F, 0x03},
    {0x40, 0x00},
    {0x7F, 0x05},
    {0x41, 0xB3},
    {0x43, 0xF1},
    {0x45, 0x14},
    {0x5B, 0x32},
    {0x5F, 0x34},
    {0x7B, 0x08},
    {0x7F, 0x06},
    {0x44, 0x1B},
    {0x40, 0xBF},
    {0x4E, 0x3F},
    {0x7F, 0x08},
    {0x65, 0x20},
    {0x6A, 0x18},
    {0x7F, 0x09},
    {0x4F, 0xAF},
    {0x5F, 0x40},
    {0x48, 0x80},
    {0x49, 0x80},
    {0x57, 0x77},
    {0x60, 0x78},
    {0x61, 0x78},
    {0x62, 0x08},
    {0x63, 0x50},
    {0x7F, 0x0A},
    {0x45, 0x60},
    {0x7F, 0x00},
    {0x4D, 0x11},
    {0x55, 0x80},
    {0x74, 0x1F},
    {0x75, 0x1F},
    {0x4A, 0x78},
    {0x4B, 0x78},
    {0x44, 0x08},
    {0x45, 0x50},
    {0x64, 0xFF},
    {0x65, 0x1F},
    {0x7F, 0x14},
    {0x65, 0x60},
    {0x66, 0x08},
    {0x63, 0x78},
    {0x7F, 0x15},
    {0x48, 0x58},
    {0x7F, 0x07},
    {0x41, 0x0D},
    {0x43, 0x14},
    {0x4B, 0x0E},
    {0x45, 0x0F},
    {0x44, 0x42},
    {0x4C, 0x80},
    {0x7F, 0x10},
    {0x5B, 0x02},
    {0x7F, 0x07},
    {0x40, 0x41},
    {0x70, 0x00},
    {0xff, 100},
    {0x32, 0x44},
    {0x7F, 0x07},
    {0x40, 0x40},
    {0x7F, 0x06},
    {0x62, 0xf0},
    {0x63, 0x00},
    {0x7F, 0x0D},
    {0x48, 0xC0},
    {0x6F, 0xd5},
    {0x7F, 0x00},
    {0x5B, 0xa0},
    {0x4E, 0xA8},
    {0x5A, 0x50},
    {0x40, 0x80},
};
static int W(FlowSensor *s, uint8_t reg, uint8_t value)
{
    s->last_reg = reg;
    s->io_status = s->io.write(s->io.context, reg, &value, 1U);
    return s->io_status;
}
static int R(FlowSensor *s, uint8_t reg, uint8_t *data, size_t count)
{
    s->last_reg = reg;
    s->io_status = s->io.read(s->io.context, reg, data, count);
    return s->io_status;
}
static int Failed(FlowSensor *s)
{
    s->fault = s->stage; s->present = s->valid = s->initializing = 0U;
    s->probe_only = 0U;
    s->errors++; return -1;
}
static void Begin(FlowSensor *s, SensorIo io, unsigned errors, unsigned attempts)
{
    memset(s, 0, sizeof(*s)); s->io = io;
    s->errors = errors; s->attempts = attempts;
    s->initializing = 1U; s->stage = FLOW_STAGE_POWER_WAIT;
    if (!io.read || !io.write || !io.now_ms || !io.delay_ms) {
        s->io_status = -1; (void)Failed(s); return;
    }
    s->next_ms = io.now_ms(io.context) + 40U;
}
void Flow_BeginRetry(FlowSensor *s)
{ Begin(s, s->io, s->errors, s->attempts + 1U); }
int Flow_InitStep(FlowSensor *s)
{
    uint8_t dummy;
    if (!s->initializing) return s->present ? 1 : -1;
    uint32_t now = s->io.now_ms(s->io.context);
    if ((int32_t)(now - s->next_ms) < 0) return 0;
    switch (s->stage) {
    case FLOW_STAGE_POWER_WAIT: s->stage = FLOW_STAGE_BANK_SELECT; break;
    case FLOW_STAGE_BANK_SELECT:
        /* Retry may follow an interrupted banked configuration sequence. */
        if (W(s, 0x7f, 0x00)) return Failed(s);
        s->stage = FLOW_STAGE_RESET_WRITE; break;
    case FLOW_STAGE_RESET_WRITE:
        if (W(s, 0x3a, 0x5a)) return Failed(s);
        s->stage = FLOW_STAGE_RESET_WAIT; s->next_ms = now + 5U; break;
    case FLOW_STAGE_RESET_WAIT: s->stage = FLOW_STAGE_ID_READ; break;
    case FLOW_STAGE_ID_READ:
        if (R(s, 0x00, &s->id, 1U)) return Failed(s);
        s->id_read_mask |= 1U; s->stage = FLOW_STAGE_INVERSE_READ; break;
    case FLOW_STAGE_INVERSE_READ:
        if (R(s, 0x5f, &s->inverse_id, 1U)) return Failed(s);
        s->id_read_mask |= 2U; s->stage = FLOW_STAGE_ID_CHECK; break;
    case FLOW_STAGE_ID_CHECK:
        if (s->id != 0x49) return Failed(s);
        if (s->inverse_id != 0xb6) {
#ifdef FLOW_ID_PROBE
            /* Diagnostic only: inspect configuration/raw data without claiming
             * presence, valid velocity, or permitting any motor output. */
            s->probe_only = 1U; s->fault = FLOW_STAGE_ID_CHECK; s->errors++;
#else
            return Failed(s);
#endif
        }
        s->config_index = 2U; s->stage = FLOW_STAGE_CLEAR_READ; break;
    case FLOW_STAGE_CLEAR_READ:
        if (R(s, (uint8_t)s->config_index, &dummy, 1U)) return Failed(s);
        if (++s->config_index > 6U) {
            s->config_index = 0U; s->stage = FLOW_STAGE_CLEAR_WAIT;
            s->next_ms = now + 1U;
        }
        break;
    case FLOW_STAGE_CLEAR_WAIT: s->stage = FLOW_STAGE_CONFIG_WRITE; break;
    case FLOW_STAGE_CONFIG_WRITE:
        if (s->config_index == sizeof(init_sequence)/sizeof(init_sequence[0])) {
            if (s->probe_only) { s->stage = FLOW_STAGE_PROBE_REVISION; break; }
            s->initializing = 0U; s->present = 1U; s->fault = FLOW_STAGE_NONE;
            s->stage = FLOW_STAGE_READY; return 1;
        }
        if (init_sequence[s->config_index][0] == 0xff)
            s->next_ms = now + init_sequence[s->config_index][1];
        else if (W(s, init_sequence[s->config_index][0], init_sequence[s->config_index][1]))
            return Failed(s);
        s->config_index++; break;
    case FLOW_STAGE_PROBE_REVISION:
        if (R(s, 0x01, &s->revision, 1U)) return Failed(s);
        s->stage = FLOW_STAGE_PROBE_CONFIG_X; break;
    case FLOW_STAGE_PROBE_CONFIG_X:
        if (R(s, 0x4e, &s->config_4e, 1U)) return Failed(s);
        s->stage = FLOW_STAGE_PROBE_CONFIG_Y; break;
    case FLOW_STAGE_PROBE_CONFIG_Y:
        if (R(s, 0x5b, &s->config_5b, 1U)) return Failed(s);
        s->initializing = 0U; s->stage = FLOW_STAGE_READY; return 1;
    default: return Failed(s);
    }
    return 0;
}
int Flow_Init(FlowSensor *s, SensorIo io)
{
    Begin(s, io, 0U, 1U);
    int result;
    while ((result = Flow_InitStep(s)) == 0) io.delay_ms(io.context, 1U);
    return result == 1 ? 0 : -1;
}
int Flow_CheckIdentity(FlowSensor *s)
{
    if (!s->present) return -1;
    s->id_read_mask = 0U;
    s->stage = FLOW_STAGE_RECHECK_ID;
    if (R(s, 0x00, &s->id, 1U)) return Failed(s);
    s->id_read_mask |= 1U;
    s->stage = FLOW_STAGE_RECHECK_INVERSE;
    if (R(s, 0x5f, &s->inverse_id, 1U)) return Failed(s);
    s->id_read_mask |= 2U;
    s->stage = FLOW_STAGE_ID_CHECK;
    if (s->id != 0x49 || s->inverse_id != 0xb6) return Failed(s);
    s->fault = FLOW_STAGE_NONE; s->stage = FLOW_STAGE_READY; return 0;
}
int Flow_Poll(FlowSensor *s)
{
    uint8_t b[12];
    s->valid = 0U; s->dx = s->dy = 0;
    s->motion = s->observation = s->quality = 0U;
    s->raw_sum = s->raw_max = s->raw_min = 0U; s->shutter = 0U;
    if (!s->present && !s->probe_only) return -1;
    s->stage = FLOW_STAGE_BURST_READ;
    if (R(s, 0x16, b, sizeof(b))) return Failed(s);
    s->motion = b[0];
    s->observation = b[1];
    s->dx = (int16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8));
    s->dy = (int16_t)((uint16_t)b[4] | ((uint16_t)b[5] << 8));
    s->quality = b[6]; s->raw_sum = b[7]; s->raw_max = b[8]; s->raw_min = b[9];
    s->shutter = (uint16_t)(((uint16_t)b[10] << 8) | b[11]);
    s->last_sample_ms = s->io.now_ms(s->io.context);
    /* Quality threshold is a starting point, subject to the actual floor test.
     * A stationary frame may have motion bit7 clear and still be healthy. */
    s->valid = s->present && s->quality >= 15U; /* motion bit4 is rawFrom0, NOT overflow */
    if (!s->probe_only) s->fault = FLOW_STAGE_NONE;
    s->stage = FLOW_STAGE_READY;
    return 0;
}
