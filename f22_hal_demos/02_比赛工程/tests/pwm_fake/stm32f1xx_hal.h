#ifndef PWM_FAKE_HAL_H
#define PWM_FAKE_HAL_H
/* Register/IO double for host tests only; cannot prove electrical waveforms. */
#include <stdint.h>
typedef struct { volatile uint32_t CR1, CCER, PSC, ARR, CCMR1, CCMR2, CCR1, CCR2, CCR3, CCR4, EGR; } TIM_TypeDef;
typedef struct { unsigned id; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Speed, Pull; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
extern TIM_TypeDef fake_tim3, fake_tim4;
extern GPIO_TypeDef fake_gpio_a, fake_gpio_b, fake_gpio_c;
#define TIM3 (&fake_tim3)
#define TIM4 (&fake_tim4)
#define GPIOA (&fake_gpio_a)
#define GPIOB (&fake_gpio_b)
#define GPIOC (&fake_gpio_c)
#define GPIO_PIN_6 (1U<<6)
#define GPIO_PIN_7 (1U<<7)
#define GPIO_PIN_8 (1U<<8)
#define GPIO_PIN_12 (1U<<12)
#define GPIO_PIN_13 (1U<<13)
#define GPIO_MODE_AF_PP 2U
#define GPIO_SPEED_FREQ_HIGH 3U
#define TIM_EGR_UG 1U
#define TIM_CR1_CEN 1U
#define TIM_CR1_ARPE (1U<<7)
#define TIM_CCMR1_OC1PE (1U<<3)
#define TIM_CCMR1_OC2PE (1U<<11)
#define TIM_CCMR2_OC3PE (1U<<3)
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC2E (1U<<4)
#define TIM_CCER_CC3E (1U<<8)
#define __HAL_RCC_TIM3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM4_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_AFIO_REMAP_TIM3_ENABLE() ((void)0)
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
uint32_t HAL_GetTick(void);
void HAL_SYSTICK_Callback(void);
#endif
