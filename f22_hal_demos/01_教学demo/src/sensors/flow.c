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
{ return s->io.write(s->io.context, reg, &value, 1U); }
int Flow_Init(FlowSensor *s, SensorIo io)
{
    uint8_t dummy;
    memset(s, 0, sizeof(*s)); s->io = io;
    if (!io.read || !io.write || !io.now_ms || !io.delay_ms) return -1;
    io.delay_ms(io.context, 40U);
    if (W(s, 0x3a, 0x5a)) goto fail;
    io.delay_ms(io.context, 5U);
    if (io.read(io.context, 0x00, &s->id, 1U) ||
        io.read(io.context, 0x5f, &s->inverse_id, 1U) || s->id != 0x49 || s->inverse_id != 0xb6) goto fail;
    for (uint8_t r = 2U; r <= 6U; ++r) if (io.read(io.context, r, &dummy, 1U)) goto fail;
    io.delay_ms(io.context, 1U);
    for (unsigned i = 0; i < sizeof(init_sequence)/sizeof(init_sequence[0]); ++i) {
        if (init_sequence[i][0] == 0xff) io.delay_ms(io.context, init_sequence[i][1]);
        else if (W(s, init_sequence[i][0], init_sequence[i][1])) goto fail;
    }
    s->present = 1U; return 0;
fail:
    s->errors++; return -1;
}
int Flow_Poll(FlowSensor *s)
{
    uint8_t b[12];
    s->valid = 0U; s->dx = s->dy = 0;
    if (!s->present) return -1;
    if (s->io.read(s->io.context, 0x16, b, sizeof(b))) {
        s->errors++; s->present = 0U; return -1;
    }
    s->motion = b[0];
    s->dx = (int16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8));
    s->dy = (int16_t)((uint16_t)b[4] | ((uint16_t)b[5] << 8));
    s->quality = b[6]; s->last_sample_ms = s->io.now_ms(s->io.context);
    /* Quality threshold is a starting point, subject to the actual floor test.
     * A stationary frame may have motion bit7 clear and still be healthy. */
    s->valid = s->quality >= 15U; /* motion bit4 is rawFrom0, NOT overflow */
    return 0;
}
