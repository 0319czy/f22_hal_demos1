#include "f22_sensor_bus.h"
#include "common.h"
#include "i2c.h"
#include "board_f22.h"
static SPI_HandleTypeDef module_spi;
#if defined(FLOW_SPI_DUPLEX_PROBE) && !defined(FLOW_ID_PROBE)
#error "Alternative SPI transport is restricted to the motor-locked diagnostic"
#endif
static uint32_t Now(void *ctx) { (void)ctx; return HAL_GetTick(); }
static void Delay(void *ctx, uint32_t ms) { (void)ctx; HAL_Delay(ms); }
static void DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < us * (SystemCoreClock / 1000000U)) { }
}
static int TofRead(void *ctx, uint16_t reg, uint8_t *b, size_t n)
{
    (void)ctx;
    return HAL_I2C_Mem_Read(&hi2c2, 0x29U << 1, reg, I2C_MEMADD_SIZE_16BIT,
                            b, (uint16_t)n, 20U) == HAL_OK ? 0 : -1;
}
static int TofWrite(void *ctx, uint16_t reg, const uint8_t *b, size_t n)
{
    (void)ctx;
    return HAL_I2C_Mem_Write(&hi2c2, 0x29U << 1, reg, I2C_MEMADD_SIZE_16BIT,
                            (uint8_t *)b, (uint16_t)n, 20U) == HAL_OK ? 0 : -1;
}
static int SpiTransfer(uint8_t address, uint8_t *rx, const uint8_t *tx, size_t n)
{
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t dummy[16] = {0};
    if (n > sizeof(dummy)) return -1;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    DelayUs(50U);
#ifdef FLOW_SPI_DUPLEX_PROBE
    /* A/B diagnostic: drain RX during the command byte as well. Writes send
     * command and payload together; read address-to-data timing is unchanged.
     * This is an experiment, not an accepted production transport fix. */
    if (!rx) {
        uint8_t frame[17] = {address};
        for (size_t i = 0; i < n; ++i) frame[i + 1U] = tx[i];
        status = HAL_SPI_Transmit(&module_spi, frame, (uint16_t)(n + 1U), 10U);
        goto finish;
    }
    uint8_t ignored;
    status = HAL_SPI_TransmitReceive(&module_spi, &address, &ignored, 1U, 10U);
#else
    status = HAL_SPI_Transmit(&module_spi, &address, 1U, 10U);
#endif
    if (status != HAL_OK) goto finish;
    /* Conservative single-register read delay used by the Bitcraze firmware.
     * Motion burst keeps its separate 50us address-to-data interval. */
    DelayUs(rx && n == 1U ? 500U : 50U);
    if (rx) {
        status = HAL_SPI_TransmitReceive(&module_spi, dummy, rx, (uint16_t)n, 10U);
    } else status = HAL_SPI_Transmit(&module_spi, (uint8_t *)tx, (uint16_t)n, 10U);
finish:
    DelayUs(50U);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); /* also deassert on timeout */
    DelayUs(200U);
    return -(int)status; /* preserve HAL_ERROR/BUSY/TIMEOUT as -1/-2/-3 */
}
static int FlowRead(void *ctx, uint16_t reg, uint8_t *b, size_t n)
{ (void)ctx; return SpiTransfer((uint8_t)reg & 0x7fU, b, 0, n); }
static int FlowWrite(void *ctx, uint16_t reg, const uint8_t *b, size_t n)
{ (void)ctx; return SpiTransfer((uint8_t)reg | 0x80U, 0, b, n); }
void F22_ModuleBusInit(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_SPI2_CLK_ENABLE();
    /* Module HEAD2 pin3 E_CS1 controls its ground switch. PC14 is NOT I2C SCL. */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_14; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_12; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_13 | GPIO_PIN_15; gpio.Mode = GPIO_MODE_AF_PP; HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_14; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_NOPULL; HAL_GPIO_Init(GPIOB, &gpio);
    module_spi.Instance = SPI2;
    module_spi.Init.Mode = SPI_MODE_MASTER; module_spi.Init.Direction = SPI_DIRECTION_2LINES;
    module_spi.Init.DataSize = SPI_DATASIZE_8BIT; module_spi.Init.CLKPolarity = SPI_POLARITY_HIGH;
    module_spi.Init.CLKPhase = SPI_PHASE_2EDGE; module_spi.Init.NSS = SPI_NSS_SOFT;
    module_spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; /* 36MHz/32=1.125MHz <=2MHz */
    module_spi.Init.FirstBit = SPI_FIRSTBIT_MSB; module_spi.Init.TIMode = SPI_TIMODE_DISABLE;
    module_spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE; module_spi.Init.CRCPolynomial = 7U;
    if (HAL_SPI_Init(&module_spi) != HAL_OK) Error_Handler();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    HAL_Delay(50U);
    /* Boot-only serial-port reset. Module may remain powered across MCU reset.
     * Motors are zero during Init(); runtime retries remain timestamp based. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_Delay(2U);
}
SensorIo F22_TofIo(void) { SensorIo io = {0, TofRead, TofWrite, Now, Delay}; return io; }
SensorIo F22_FlowIo(void) { SensorIo io = {0, FlowRead, FlowWrite, Now, Delay}; return io; }
