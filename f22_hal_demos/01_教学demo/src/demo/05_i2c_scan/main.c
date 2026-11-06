#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "system_clock.h"
#include "usart.h"

/*
 * 验板主力例程：同时扫描 I2C1 和 I2C2，并点名手册/原理图上板载的器件。
 *
 * 结果通过 USART1 输出，也就是板载 CH340 背后的串口，用 USB 线直接能看到。
 * 输出分成两部分：
 *   1) 扫到的所有 7 位地址
 *   2) 板载器件逐个点名，缺哪个就报哪个 MISSING
 * 第 2 部分才是判断板子好坏的关键：总线通但器件不应答，说明那颗芯片坏了。
 */

/* 板载器件清单：7 位地址 + 名字。 */
typedef struct {
    uint8_t address7;
    const char *name;
} BoardDevice;

/*
 * 板载传感器挂在 I2C2（PB10/PB11）上；US22310S 如果装了超声波模块也在这条总线，
 * 地址 7 位是 0x39（官方旧例程里的 0x70 是 US22309S，不能照抄）。
 * ST24C02 的 A0/A1/A2 焊盘决定地址，按 0x50~0x57 全范围检查。
 */
static const BoardDevice i2c2_devices[] = {
    {BOARD_I2C_ADDR_MPU6050,    "MPU6050 (attitude)"},
    {BOARD_I2C_ADDR_QMC5883L,  "QMC5883L (magnetometer)"},
    {BOARD_I2C_ADDR_SPL06,     "SPL06-001 (barometer)"},
    {BOARD_I2C_ADDR_US22310S,  "US22310S (ultrasonic)"},
};

#define I2C2_DEVICE_COUNT (sizeof(i2c2_devices) / sizeof(i2c2_devices[0]))

/* I2C1（PB6/PB7）出厂不接任何器件，扫到东西反而要留意。 */
static void ScanBus(I2C_HandleTypeDef *bus, const char *label)
{
    uint8_t found = 0U;

    Board_Log("=== ");
    Board_Log(label);
    Board_Log(" scan (7-bit) ===\r\n");

    /* 0x08~0x77 是常用 7 位地址范围，跳过保留地址。 */
    for (uint16_t address7 = 0x08U; address7 <= 0x77U; ++address7) {
        /* HAL 要求传入左移一位后的 8 位形式，所以这里传 address7 << 1。 */
        if (HAL_I2C_IsDeviceReady(bus,
                                  (uint16_t)(address7 << 1U),
                                  3U,
                                  20U) == HAL_OK) {
            Board_Log("  found 0x");
            Board_LogHexByte((uint8_t)address7);
            Board_Log("\r\n");
            found = 1U;
        }
    }

    if (!found) {
        Board_Log("  (nothing on this bus)\r\n");
    }
}

/* 逐个点名板载器件，应答的报 OK，不应答的报 MISSING。 */
static void ReportI2c2Devices(void)
{
    Board_Log("=== I2C2 onboard device check ===\r\n");

    for (uint32_t i = 0U; i < I2C2_DEVICE_COUNT; ++i) {
        Board_Log("  ");
        Board_Log(i2c2_devices[i].name);
        Board_Log(" @0x");
        Board_LogHexByte(i2c2_devices[i].address7);
        Board_Log(" : ");

        if (HAL_I2C_IsDeviceReady(&hi2c2,
                                  (uint16_t)(i2c2_devices[i].address7 << 1U),
                                  3U,
                                  20U) == HAL_OK) {
            Board_Log("OK\r\n");
        } else {
            Board_Log("MISSING\r\n");
        }
    }

    /* EEPROM 地址由 A0/A1/A2 硬件配置决定，只做地址应答检测，不读写内容。 */
    uint8_t eeprom_found = 0U;
    for (uint16_t address7 = BOARD_I2C_ADDR_EEPROM24C02_BASE;
         address7 <= BOARD_I2C_ADDR_EEPROM24C02_TOP;
         ++address7) {
        if (HAL_I2C_IsDeviceReady(&hi2c2,
                                  (uint16_t)(address7 << 1U),
                                  3U,
                                  20U) == HAL_OK) {
            Board_Log("  ST24C02 (EEPROM 2K) @0x");
            Board_LogHexByte((uint8_t)address7);
            Board_Log(" : OK\r\n");
            eeprom_found = 1U;
            break;
        }
    }
    if (!eeprom_found) {
        Board_Log("  ST24C02 (EEPROM 2K) @0x50-0x57 : MISSING\r\n");
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    /* 输出走 USART1（板载 CH340），必须先初始化再打印。 */
    MX_USART1_UART_Init();
    MX_GPIO_Init();
    /* LED1 常亮表示程序已经跑到 main 并正常循环。 */
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_ON_LEVEL);

    Board_Log("\r\nF22 I2C scan\r\n");

    /*
     * I2C1 和 I2C2 分开初始化、分开扫描：PB6/PB7 是 I2C1 的脚，
     * 也是 M6/M7 的 T4_CH1/T4_CH2，两个外设不能同时用。
     */
    MX_I2C1_Init();
    ScanBus(&hi2c1, "I2C1 (PB6/PB7, JP6)");

    MX_I2C2_Init();
    ScanBus(&hi2c2, "I2C2 (PB10/PB11, JP7)");
    ReportI2c2Devices();

    Board_Log("scan done\r\n");

    while (1) {
        /* 只扫一次，避免一直占用总线影响后续判断；结果保持在上方输出里。 */
        HAL_Delay(2000U);
    }
}
