#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "sensors.h"
#include "system_clock.h"
#include "usart.h"

#include <math.h>
#include <string.h>

#define VOFA_CHANNEL_COUNT 20U
#define VOFA_PERIOD_MS 20U
#define ALTITUDE_FILTER_ALPHA 0.10f
#define RAD_TO_DEG 57.2957795f
#define DEG_TO_RAD 0.0174532925f

/* Only finite values or NaN are sent: +Infinity is JustFloat's delimiter. */
static HAL_StatusTypeDef SendFrame(const float channels[VOFA_CHANNEL_COUNT])
{
    uint8_t frame[VOFA_CHANNEL_COUNT * sizeof(float) + 4U];
    static const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7fU};

    _Static_assert(sizeof(float) == 4U, "JustFloat requires float32");
    /* STM32F103 is little endian; memcpy avoids alignment/aliasing issues. */
    memcpy(frame, channels, VOFA_CHANNEL_COUNT * sizeof(float));
    memcpy(frame + VOFA_CHANNEL_COUNT * sizeof(float), tail, sizeof(tail));
    return HAL_UART_Transmit(&huart1, frame, sizeof(frame), 20U);
}

static uint8_t InitImu(float gyro_bias[3])
{
    uint8_t id = 0U;
    uint8_t i;
    uint8_t sample;
    float sum[3] = {0.0f, 0.0f, 0.0f};
    ImuSample imu;

    if (Sensors_ImuReadId(&id) != HAL_OK || id != MPU6050_ID_MPU6050 ||
        Sensors_ImuInit() != HAL_OK) {
        return 0U;
    }
    /* Estimate gyro zero-rate bias at boot. Keep the board still briefly. */
    for (sample = 0U; sample < 100U; ++sample) {
        if (Sensors_ImuRead(&imu) != HAL_OK) {
            return 0U;
        }
        for (i = 0U; i < 3U; ++i) {
            sum[i] += (float)imu.gyro[i] / 131.0f;
        }
        HAL_Delay(5U);
    }
    for (i = 0U; i < 3U; ++i) {
        gyro_bias[i] = sum[i] / 100.0f;
    }
    return 1U;
}

static uint8_t InitBaro(void)
{
    uint8_t id = 0U;
    uint32_t started;
    if (Sensors_BaroReadId(&id) != HAL_OK || (id & 0xf0U) != 0x10U ||
        Sensors_BaroInit() != HAL_OK) {
        return 0U;
    }
    /* Wait once for both first conversions; temperature runs at only 4 Hz. */
    started = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - started) < 500U) {
        uint8_t status = 0U;
        if (Sensors_BaroReadStatus(&status) != HAL_OK) {
            return 0U;
        }
        if ((status & 0x30U) == 0x30U) {
            return 1U;
        }
        HAL_Delay(5U);
    }
    return 0U;
}

int main(void)
{
    uint8_t imu_ready;
    uint8_t baro_ready;
    float gyro_bias[3] = {0.0f, 0.0f, 0.0f};
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float reference_pressure_pa = 0.0f;
    float relative_altitude_filtered_m = 0.0f;
    uint8_t altitude_filter_valid = 0U;
    uint8_t attitude_valid = 0U;
    uint32_t attitude_tick = 0U;
    uint32_t retry_at;
    uint32_t previous_frame;
    uint32_t heartbeat_at;

    /* Also work when entered by the ROM ISP GO command with BOOT0 high.
     * Address zero may still alias ROM; explicitly route SysTick to flash. */
    SCB->VTOR = FLASH_BASE;
    __DSB();
    __ISB();
    __enable_irq();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_I2C2_Init();
    /* This telemetry application does not start motor PWM or write EEPROM. */
    imu_ready = InitImu(gyro_bias);
    baro_ready = InitBaro();
    retry_at = HAL_GetTick();
    previous_frame = HAL_GetTick();
    heartbeat_at = previous_frame;

    while (1) {
        float ch[VOFA_CHANNEL_COUNT];
        ImuSample imu;
        BaroCompensatedSample baro;
        uint32_t started = HAL_GetTick();
        uint8_t imu_ok = 0U;
        uint8_t baro_ok = 0U;
        uint8_t i;

        /* Retry failed devices once a second; do not stall the whole stream. */
        if ((uint32_t)(started - retry_at) >= 1000U) {
            /* A reset during an I2C transaction can leave the peripheral
             * BUSY. Reset its state when both independent devices fail. */
            if (!imu_ready && !baro_ready) {
                (void)HAL_I2C_DeInit(&hi2c2);
                __HAL_RCC_I2C2_FORCE_RESET();
                __HAL_RCC_I2C2_RELEASE_RESET();
                MX_I2C2_Init();
            }
            if (!imu_ready) {
                imu_ready = InitImu(gyro_bias);
                if (imu_ready) {
                    attitude_valid = 0U;
                }
            }
            if (!baro_ready) {
                baro_ready = InitBaro();
            }
            retry_at = HAL_GetTick();
        }
        for (i = 0U; i < VOFA_CHANNEL_COUNT; ++i) {
            ch[i] = NAN;
        }
        if (imu_ready && Sensors_ImuRead(&imu) == HAL_OK) {
            float acc_roll;
            float acc_pitch;
            float acc_norm_sq;
            float dt;
            for (i = 0U; i < 3U; ++i) {
                ch[i] = (float)imu.accel[i] / 16384.0f;
                ch[i + 3U] = (float)imu.gyro[i] / 131.0f - gyro_bias[i];
            }
            ch[6] = (float)imu.temp_raw / 340.0f + 36.53f;
            acc_norm_sq = ch[0] * ch[0] + ch[1] * ch[1] + ch[2] * ch[2];
            acc_roll = atan2f(ch[1], ch[2]);
            acc_pitch = atan2f(-ch[0], sqrtf(ch[1] * ch[1] + ch[2] * ch[2]));
            if (!attitude_valid) {
                roll = acc_roll;
                pitch = acc_pitch;
                yaw = 0.0f;
                attitude_tick = started;
                attitude_valid = 1U;
            } else {
                dt = (float)(uint32_t)(started - attitude_tick) * 0.001f;
                if (dt > 0.1f) {
                    dt = 0.1f;
                }
                roll += ch[3] * DEG_TO_RAD * dt;
                pitch += ch[4] * DEG_TO_RAD * dt;
                yaw += ch[5] * DEG_TO_RAD * dt;
                /* Accelerometer corrects tilt only near 1 g; it has no yaw. */
                if (acc_norm_sq > 0.5625f && acc_norm_sq < 1.5625f) {
                    roll = 0.98f * roll + 0.02f * acc_roll;
                    pitch = 0.98f * pitch + 0.02f * acc_pitch;
                }
                attitude_tick = started;
            }
            if (yaw > 3.14159265f) yaw -= 6.28318531f;
            if (yaw < -3.14159265f) yaw += 6.28318531f;
            ch[7] = roll * RAD_TO_DEG;
            ch[8] = pitch * RAD_TO_DEG;
            ch[14] = yaw * RAD_TO_DEG;
            {
                float cr = cosf(roll * 0.5f), sr = sinf(roll * 0.5f);
                float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
                float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
                /* VOFA Cube quaternion bindings are x, y, z, scalar(w). */
                ch[15] = sr * cp * cy - cr * sp * sy;
                ch[16] = cr * sp * cy + sr * cp * sy;
                ch[17] = cr * cp * sy - sr * sp * cy;
                ch[18] = cr * cp * cy + sr * sp * sy;
            }
            imu_ok = 1U;
        } else {
            imu_ready = 0U;
            attitude_valid = 0U;
        }
        if (baro_ready && Sensors_BaroReadCompensated(&baro) == HAL_OK &&
            isfinite(baro.pressure_pa) && isfinite(baro.temperature_c)) {
            ch[9] = baro.pressure_pa / 100.0f;
            ch[10] = baro.temperature_c;
            /* Relative barometric altitude; zero at the first valid sample. */
            if (reference_pressure_pa <= 0.0f) {
                reference_pressure_pa = baro.pressure_pa;
            }
            if (reference_pressure_pa > 0.0f) {
                float relative_altitude_raw_m =
                    44330.0f *
                    (1.0f - powf(baro.pressure_pa / reference_pressure_pa,
                                 0.19029495f));
                if (!altitude_filter_valid) {
                    relative_altitude_filtered_m = relative_altitude_raw_m;
                    altitude_filter_valid = 1U;
                } else {
                    relative_altitude_filtered_m += ALTITUDE_FILTER_ALPHA *
                        (relative_altitude_raw_m - relative_altitude_filtered_m);
                }
                ch[19] = relative_altitude_filtered_m;
            }
            baro_ok = 1U;
        } else {
            baro_ready = 0U;
        }
        ch[11] = (float)imu_ok;
        ch[12] = (float)baro_ok;
        ch[13] = (float)(uint32_t)(started - previous_frame);
        previous_frame = started;

        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                          imu_ok && baro_ok ? BOARD_LED1_OFF_LEVEL :
                                             BOARD_LED1_ON_LEVEL);
        if (SendFrame(ch) != HAL_OK) {
            HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                              BOARD_LED1_ON_LEVEL);
        }
        if ((uint32_t)(HAL_GetTick() - heartbeat_at) >= 500U) {
            HAL_GPIO_TogglePin(BOARD_LED2_GPIO_Port, BOARD_LED2_Pin);
            heartbeat_at = HAL_GetTick();
        }
        /* Account for sampling and UART time instead of adding 20 ms to it. */
        {
            uint32_t elapsed = HAL_GetTick() - started;
            if (elapsed < VOFA_PERIOD_MS) {
                HAL_Delay(VOFA_PERIOD_MS - elapsed);
            }
        }
    }
}
