#include "board_f22.h"
#include "common.h"
#include "gpio.h"

/*
 * 只打开本函数真正要用到的端口时钟，避免无谓的时钟门控。
 * 单独打开的好处是：demo13 只要 PC14/PC15 就不会顺带占用 PB 端口。
 */
static void Gpio_EnableClocks(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* 初始化 LED1/LED2 输出（PA0/PA1）和两个板载按键输入（PC12/PC13）。 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    Gpio_EnableClocks();

    /*
     * 先写入"熄灭"电平再切换成输出，避免初始化瞬间出现一个亮灯脉冲。
     * 反相三极管使 LED 在 MCU 侧为低电平点亮，具体电平见 board_f22.h。
     */
    HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port,
                      BOARD_LED1_Pin,
                      BOARD_LED1_OFF_LEVEL);
    HAL_GPIO_WritePin(BOARD_LED2_GPIO_Port,
                      BOARD_LED2_Pin,
                      BOARD_LED2_OFF_LEVEL);

    gpio.Pin = BOARD_LED1_Pin | BOARD_LED2_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    /*
     * PA0 还可能是 USART2_CTS、PA1 还可能是 USART2_RTS，但本板两个串口
     * 都不启用硬件流控，因此推挽输出不会和串口配置冲突。
     */
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /*
     * 原理图上 S2/S3 一端直接接 GND 且没有外部上拉，
     * 所以必须打开内部上拉，按下读到低电平。
     */
    gpio.Pin = BOARD_KEY1_Pin | BOARD_KEY2_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);
}

/* 初始化 J2/J3 上的 5 个扩展通用 IO：PC14、PC15、PB3、PB4、PB5。 */
void MX_GPIO_InitGeneralIo(void)
{
    GPIO_InitTypeDef gpio = {0};

    Gpio_EnableClocks();

    /*
     * PC14/PC15 属于大电流/备份域引脚，PC13~PC15 的 GPIO 输出速率上限只有 2 MHz，
     * 驱动能力约 3 mA，只能做电平和小信号测试，不能直接带负载。
     */
    gpio.Pin = BOARD_IO_PC14_Pin | BOARD_IO_PC15_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    /*
     * PB3/PB4 复位后被 JTAG 占用，必须先调用 SystemClock_Config()
     * 里的 __HAL_AFIO_REMAP_SWJ_NOJTAG() 才能在这里配置成普通输出。
     */
    gpio.Pin = BOARD_IO_PB3_Pin | BOARD_IO_PB4_Pin | BOARD_IO_PB5_Pin;
    HAL_GPIO_Init(GPIOB, &gpio);
}

/* 把 PC12/PC13 改成下降沿外部中断输入，供 exti_key 例程使用。 */
void MX_GPIO_InitExtiKey(void)
{
    GPIO_InitTypeDef gpio = {0};

    Gpio_EnableClocks();

    gpio.Pin = BOARD_KEY1_Pin | BOARD_KEY2_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    /* 外部没有上拉，中断模式同样依赖内部上拉把空闲电平抬到高。 */
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* PC12/PC13 落在 EXTI15_10 这一条 IRQ 上，见 stm32f1xx_it.c。 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
