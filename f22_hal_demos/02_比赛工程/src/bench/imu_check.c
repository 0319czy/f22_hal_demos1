/* Bench-only entry: production gyro calibration, sensor axes, NO motor output. */
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include "i2c.h"
#include "sensors.h"
#include "imu_calibration.h"
#include <math.h>
#include <string.h>
static unsigned Calibrate(ImuCalibration *window, float bias[3], unsigned *errors)
{
    uint8_t id = 0; ImuSample sample;
    ImuCalibration_Init(window);
    if (Sensors_ImuReadId(&id) != HAL_OK || id != 0x68U || Sensors_ImuInit() != HAL_OK)
        return 0U;
    for (unsigned n = 0; n < IMU_CALIBRATION_SAMPLES; ++n) {
        if (Sensors_ImuRead(&sample) != HAL_OK) { ++*errors; return 0U; }
        float acc[3], gyro[3];
        for (unsigned i = 0; i < 3; ++i) {
            acc[i] = sample.accel[i] / 16384.0f;
            gyro[i] = sample.gyro[i] / 131.0f;
        }
        if (!ImuCalibration_Add(window, acc, gyro)) return 0U;
        HAL_Delay(5U);
    }
    return ImuCalibration_Finish(window, bias);
}
static void Send(const float channels[16])
{
    uint8_t frame[68]; const uint8_t tail[] = {0, 0, 0x80, 0x7f};
    _Static_assert(sizeof(float) == 4, "JustFloat needs float32");
    memcpy(frame, channels, 64); memcpy(frame + 64, tail, 4);
    (void)HAL_UART_Transmit(&huart1, frame, sizeof(frame), 20U);
}
int main(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_USART1_UART_Init(); MX_I2C2_Init();
    ImuCalibration window; float bias[3] = {0}; unsigned errors = 0, failures = 0;
    unsigned ready = Calibrate(&window, bias, &errors);
    if (!ready) ++failures;
    uint32_t retry = HAL_GetTick(), previous = retry;
    while (1) {
        uint32_t now = HAL_GetTick();
        if (!ready && (uint32_t)(now - retry) >= 2000U) {
            ready = Calibrate(&window, bias, &errors);
            if (!ready) ++failures;
            retry = previous = HAL_GetTick();
        }
        now = HAL_GetTick();
        if ((uint32_t)(now - previous) >= 20U) {
            float c[16] = {0}; ImuSample sample;
            for (unsigned i = 1; i <= 8; ++i) c[i] = NAN;
            if (ready) {
                if (Sensors_ImuRead(&sample) == HAL_OK) {
                    c[0] = 1; float norm = 0;
                    for (unsigned i = 0; i < 3; ++i) {
                        c[i + 1] = sample.accel[i] / 16384.0f;
                        c[i + 4] = sample.gyro[i] / 131.0f - bias[i];
                        norm += c[i + 1] * c[i + 1];
                    }
                    c[7] = sample.temp_raw / 340.0f + 36.53f; c[8] = sqrtf(norm);
                } else { ready = 0U; ++errors; retry = now; }
            }
            for (unsigned i = 0; i < 3; ++i) c[i + 9] = bias[i];
            c[12] = window.count; c[13] = errors; c[14] = now - previous; c[15] = failures;
            previous = now; Send(c);
        }
        HAL_Delay(1U);
    }
}
