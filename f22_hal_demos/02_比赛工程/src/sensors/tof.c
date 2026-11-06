/* Port of STMicroelectronics stm32duino/VL53L1X default configuration and ULD
 * initialization/long-mode settings. COPYRIGHT(c) 2018 STMicroelectronics.
 * Redistribution conditions and disclaimer retained below.
 * COPYRIGHT(c) 2018 STMicroelectronics
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "tof.h"
#include <string.h>
static const uint8_t defaults[] =
{
   0x00, /* 0x2d : set bit 2 and 5 to 1 for fast plus mode (1MHz I2C), else don't touch */
   0x00, /* 0x2e : bit 0 if I2C pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
   0x00, /* 0x2f : bit 0 if GPIO pulled up at 1.8V, else set bit 0 to 1 (pull up at AVDD) */
   0x01, /* 0x30 : set bit 4 to 0 for active high interrupt and 1 for active low (bits 3:0 must be 0x1), use SetInterruptPolarity() */
   0x02, /* 0x31 : bit 1 = interrupt depending on the polarity, use CheckForDataReady() */
   0x00, /* 0x32 : not user-modifiable */
   0x02, /* 0x33 : not user-modifiable */
   0x08, /* 0x34 : not user-modifiable */
   0x00, /* 0x35 : not user-modifiable */
   0x08, /* 0x36 : not user-modifiable */
   0x10, /* 0x37 : not user-modifiable */
   0x01, /* 0x38 : not user-modifiable */
   0x01, /* 0x39 : not user-modifiable */
   0x00, /* 0x3a : not user-modifiable */
   0x00, /* 0x3b : not user-modifiable */
   0x00, /* 0x3c : not user-modifiable */
   0x00, /* 0x3d : not user-modifiable */
   0xff, /* 0x3e : not user-modifiable */
   0x00, /* 0x3f : not user-modifiable */
   0x0F, /* 0x40 : not user-modifiable */
   0x00, /* 0x41 : not user-modifiable */
   0x00, /* 0x42 : not user-modifiable */
   0x00, /* 0x43 : not user-modifiable */
   0x00, /* 0x44 : not user-modifiable */
   0x00, /* 0x45 : not user-modifiable */
   0x20, /* 0x46 : interrupt configuration 0->level low detection, 1-> level high, 2-> Out of window, 3->In window, 0x20-> New sample ready , TBC */
   0x0b, /* 0x47 : not user-modifiable */
   0x00, /* 0x48 : not user-modifiable */
   0x00, /* 0x49 : not user-modifiable */
   0x02, /* 0x4a : not user-modifiable */
   0x0a, /* 0x4b : not user-modifiable */
   0x21, /* 0x4c : not user-modifiable */
   0x00, /* 0x4d : not user-modifiable */
   0x00, /* 0x4e : not user-modifiable */
   0x05, /* 0x4f : not user-modifiable */
   0x00, /* 0x50 : not user-modifiable */
   0x00, /* 0x51 : not user-modifiable */
   0x00, /* 0x52 : not user-modifiable */
   0x00, /* 0x53 : not user-modifiable */
   0xc8, /* 0x54 : not user-modifiable */
   0x00, /* 0x55 : not user-modifiable */
   0x00, /* 0x56 : not user-modifiable */
   0x38, /* 0x57 : not user-modifiable */
   0xff, /* 0x58 : not user-modifiable */
   0x01, /* 0x59 : not user-modifiable */
   0x00, /* 0x5a : not user-modifiable */
   0x08, /* 0x5b : not user-modifiable */
   0x00, /* 0x5c : not user-modifiable */
   0x00, /* 0x5d : not user-modifiable */
   0x01, /* 0x5e : not user-modifiable */
   0xcc, /* 0x5f : not user-modifiable */
   0x0f, /* 0x60 : not user-modifiable */
   0x01, /* 0x61 : not user-modifiable */
   0xf1, /* 0x62 : not user-modifiable */
   0x0d, /* 0x63 : not user-modifiable */
   0x01, /* 0x64 : Sigma threshold MSB (mm in 14.2 format for MSB+LSB), use SetSigmaThreshold(), default value 90 mm  */
   0x68, /* 0x65 : Sigma threshold LSB */
   0x00, /* 0x66 : Min count Rate MSB (MCPS in 9.7 format for MSB+LSB), use SetSignalThreshold() */
   0x80, /* 0x67 : Min count Rate LSB */
   0x08, /* 0x68 : not user-modifiable */
   0xb8, /* 0x69 : not user-modifiable */
   0x00, /* 0x6a : not user-modifiable */
   0x00, /* 0x6b : not user-modifiable */
   0x00, /* 0x6c : Intermeasurement period MSB, 32 bits register, use SetIntermeasurementInMs() */
   0x00, /* 0x6d : Intermeasurement period */
   0x0f, /* 0x6e : Intermeasurement period */
   0x89, /* 0x6f : Intermeasurement period LSB */
   0x00, /* 0x70 : not user-modifiable */
   0x00, /* 0x71 : not user-modifiable */
   0x00, /* 0x72 : distance threshold high MSB (in mm, MSB+LSB), use SetD:tanceThreshold() */
   0x00, /* 0x73 : distance threshold high LSB */
   0x00, /* 0x74 : distance threshold low MSB ( in mm, MSB+LSB), use SetD:tanceThreshold() */
   0x00, /* 0x75 : distance threshold low LSB */
   0x00, /* 0x76 : not user-modifiable */
   0x01, /* 0x77 : not user-modifiable */
   0x0f, /* 0x78 : not user-modifiable */
   0x0d, /* 0x79 : not user-modifiable */
   0x0e, /* 0x7a : not user-modifiable */
   0x0e, /* 0x7b : not user-modifiable */
   0x00, /* 0x7c : not user-modifiable */
   0x00, /* 0x7d : not user-modifiable */
   0x02, /* 0x7e : not user-modifiable */
   0xc7, /* 0x7f : ROI center, use SetROI() */
   0xff, /* 0x80 : XY ROI (X=Width, Y=Height), use SetROI() */
   0x9B, /* 0x81 : not user-modifiable */
   0x00, /* 0x82 : not user-modifiable */
   0x00, /* 0x83 : not user-modifiable */
   0x00, /* 0x84 : not user-modifiable */
   0x01, /* 0x85 : not user-modifiable */
   0x00, /* 0x86 : clear interrupt, use ClearInterrupt() */
   0x00  /* 0x87 : start ranging, use StartRanging() or StopRanging(), If you want an automatic start after VL53L1X_init() call, put 0x40 in location 0x87 */
};




static int Read(TofSensor *s, uint16_t reg, uint8_t *data, size_t n)
{ return s->io.read(s->io.context, reg, data, n); }
static int Write(TofSensor *s, uint16_t reg, const uint8_t *data, size_t n)
{ return s->io.write(s->io.context, reg, data, n); }
static int W8(TofSensor *s, uint16_t reg, uint8_t data)
{ return Write(s, reg, &data, 1U); }
static int W16(TofSensor *s, uint16_t reg, uint16_t data)
{ uint8_t b[] = {(uint8_t)(data >> 8), (uint8_t)data}; return Write(s, reg, b, 2U); }
static int Ready(TofSensor *s, uint8_t *ready)
{
    uint8_t polarity, status;
    if (Read(s, 0x0030, &polarity, 1U) || Read(s, 0x0031, &status, 1U)) return -1;
    *ready = ((status & 1U) == (((polarity >> 4) & 1U) ^ 1U));
    return 0;
}
unsigned Tof_AgeMs(const TofSensor *s)
{ return s->seen ? s->io.now_ms(s->io.context) - s->last_sample_ms : UINT32_MAX; }
int Tof_Init(TofSensor *s, SensorIo io)
{
    uint8_t b[4], ready = 0U;
    uint32_t start;
    memset(s, 0, sizeof(*s)); s->io = io;
    if (!io.read || !io.write || !io.now_ms || !io.delay_ms) return -1;
    start = io.now_ms(io.context);
    do {
        if (Read(s, 0x00e5, b, 1U)) goto fail;
        if (b[0] & 1U) break;
        io.delay_ms(io.context, 2U);
    } while ((uint32_t)(io.now_ms(io.context) - start) < 500U);
    if (!(b[0] & 1U) || Read(s, 0x010f, b, 2U)) goto fail;
    s->id = ((uint16_t)b[0] << 8) | b[1];
    if (s->id != 0xeaccU) goto fail; /* L0X is a different driver, despite same address. */
    if (Write(s, 0x002d, defaults, sizeof(defaults)) || W8(s, 0x0087, 0x40)) goto fail;
    start = io.now_ms(io.context);
    do {
        if (Ready(s, &ready)) goto fail;
        if (ready) break;
        io.delay_ms(io.context, 2U);
    } while ((uint32_t)(io.now_ms(io.context) - start) < 500U);
    if (!ready || W8(s, 0x0086, 1U) || W8(s, 0x0087, 0U) ||
        W8(s, 0x0008, 9U) || W8(s, 0x000b, 0U)) goto fail;
    /* ST long-mode register values; 33ms timing budget. */
    if (W8(s, 0x004b, 0x0a) || W8(s, 0x0060, 0x0f) || W8(s, 0x0063, 0x0d) ||
        W8(s, 0x0069, 0xb8) || W16(s, 0x0078, 0x0f0d) || W16(s, 0x007a, 0x0e0e) ||
        W16(s, 0x005e, 0x0060) || W16(s, 0x0061, 0x006e) || Read(s, 0x00de, b, 2U)) goto fail;
    uint16_t clock_pll = (((uint16_t)b[0] << 8) | b[1]) & 0x03ffU;
    if (!clock_pll) goto fail;
    uint32_t interval = (uint32_t)((float)clock_pll * 40.0f * 1.075f);
    b[0] = (uint8_t)(interval >> 24); b[1] = (uint8_t)(interval >> 16);
    b[2] = (uint8_t)(interval >> 8); b[3] = (uint8_t)interval;
    if (Write(s, 0x006c, b, 4U) || W8(s, 0x0086, 1U) || W8(s, 0x0087, 0x40)) goto fail;
    s->present = 1U; s->started_ms = io.now_ms(io.context); return 0;
fail:
    s->errors++; return -1;
}
int Tof_Poll(TofSensor *s)
{
    uint8_t ready, data[17];
    if (!s->present) { s->valid = 0U; return -1; }
    uint32_t now = s->io.now_ms(s->io.context);
    if (Tof_AgeMs(s) >= TOF_MAX_AGE_MS) s->valid = 0U;
    if (Ready(s, &ready)) goto fail;
    if (!ready) {
        if ((uint32_t)(now - (s->seen ? s->last_sample_ms : s->started_ms)) > 500U) goto fail;
        return 0;
    }
    /* Burst result 0x89..0x99: status and distance belong to same conversion. */
    if (Read(s, 0x0089, data, sizeof(data)) || W8(s, 0x0086, 1U)) goto fail;
    s->raw_status = data[0] & 0x1fU;
    s->distance_mm = ((uint16_t)data[13] << 8) | data[14]; /* 0x96/0x97 */
    s->last_sample_ms = now; s->seen = 1U;
    /* ST raw status 9 -> RangeValid(0). Other status codes are not accepted. */
    s->valid = s->raw_status == 9U && s->distance_mm >= 40U && s->distance_mm <= 4000U;
    return 1;
fail:
    s->errors++; s->present = s->valid = 0U; return -1;
}
