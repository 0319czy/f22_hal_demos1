#ifndef STM32F1XX_HAL_CONF_H
#define STM32F1XX_HAL_CONF_H

#define HAL_MODULE_ENABLED

#define HAL_ADC_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED

#ifndef HSE_VALUE
#define HSE_VALUE 8000000U
#endif
#ifndef HSI_VALUE
#define HSI_VALUE 8000000U
#endif
#ifndef LSI_VALUE
#define LSI_VALUE 40000U
#endif
#ifndef LSE_VALUE
#define LSE_VALUE 32768U
#endif

#define HSE_STARTUP_TIMEOUT 100U
#define LSE_STARTUP_TIMEOUT 5000U
#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY 15U
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U

#define USE_HAL_ADC_REGISTER_CALLBACKS 0U
#define USE_HAL_DAC_REGISTER_CALLBACKS 0U
#define USE_HAL_I2C_REGISTER_CALLBACKS 0U
#define USE_HAL_TIM_REGISTER_CALLBACKS 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f1xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f1xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f1xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f1xx_hal_cortex.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f1xx_hal_adc.h"
#endif
#ifdef HAL_DAC_MODULE_ENABLED
#include "stm32f1xx_hal_dac.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f1xx_hal_flash.h"
#endif
#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32f1xx_hal_i2c.h"
#endif
#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32f1xx_hal_spi.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f1xx_hal_pwr.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f1xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f1xx_hal_uart.h"
#endif

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

#endif
