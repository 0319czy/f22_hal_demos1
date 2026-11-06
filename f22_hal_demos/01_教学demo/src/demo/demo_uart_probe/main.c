#include "stm32f1xx.h"

/* Temporary hardware diagnosis: HSI, direct USART1 registers, no HAL tick,
 * HSE, I2C or floating point. Never enable motor outputs. */
int main(void)
{
    SCB->VTOR = FLASH_BASE;
    __disable_irq();
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U) {}
    RCC->CFGR = 0U;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0U) {}
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xfU << 4U)) | (0xbU << 4U);
    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->BRR = 69U; /* 8 MHz / 115200 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
    while (1) {
        const char *p = "F22 UART ALIVE\r\n";
        while (*p) {
            while ((USART1->SR & USART_SR_TXE) == 0U) {}
            USART1->DR = (uint8_t)*p++;
        }
        for (volatile uint32_t i = 0U; i < 400000U; ++i) {}
    }
}
