#include "board_f22.h"
#include "common.h"
#include "usart.h"

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart4;

static void Uart_Config8N1(UART_HandleTypeDef *uart,
                           uint32_t baudrate)
{
    uart->Init.BaudRate = baudrate;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
    /* 三个接口都没有接硬件流控线，所以一律关闭。 */
    uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->Init.OverSampling = UART_OVERSAMPLING_16;
}

/* USART1：JP12 的 PA9=TXD1 / PA10=RXD1，与板载 CH340 及 E49 模组共用。 */
void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    Uart_Config8N1(&huart1, USART1_DEMO_BAUDRATE);
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* USART2：JP13 的 PA2=TXD2 / PA3=RXD2，常用调试口。 */
void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    Uart_Config8N1(&huart2, USART2_DEMO_BAUDRATE);
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/* UART4：JP14 的 PC10=USART4_TX / PC11=USART4_RX，3.3 V 电平。 */
void MX_USART4_UART_Init(void)
{
    huart4.Instance = UART4;
    Uart_Config8N1(&huart4, USART4_DEMO_BAUDRATE);
    if (HAL_UART_Init(&huart4) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *uart)
{
    GPIO_InitTypeDef gpio = {0};

    /* TX 用复用推挽输出，RX 用浮空输入；两者都不需要内部上下拉。 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    if (uart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio.Pin = BOARD_USART1_TX_PIN;
        HAL_GPIO_Init(BOARD_USART1_TX_PORT, &gpio);

        gpio.Pin = BOARD_USART1_RX_PIN;
        gpio.Mode = GPIO_MODE_INPUT;
        HAL_GPIO_Init(BOARD_USART1_RX_PORT, &gpio);
    } else if (uart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio.Pin = BOARD_USART2_TX_PIN;
        HAL_GPIO_Init(BOARD_USART2_TX_PORT, &gpio);

        gpio.Pin = BOARD_USART2_RX_PIN;
        gpio.Mode = GPIO_MODE_INPUT;
        HAL_GPIO_Init(BOARD_USART2_RX_PORT, &gpio);
    } else if (uart->Instance == UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        gpio.Pin = BOARD_USART4_TX_PIN;
        HAL_GPIO_Init(BOARD_USART4_TX_PORT, &gpio);

        gpio.Pin = BOARD_USART4_RX_PIN;
        gpio.Mode = GPIO_MODE_INPUT;
        HAL_GPIO_Init(BOARD_USART4_RX_PORT, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(BOARD_USART1_TX_PORT, BOARD_USART1_TX_PIN);
        HAL_GPIO_DeInit(BOARD_USART1_RX_PORT, BOARD_USART1_RX_PIN);
    } else if (uart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(BOARD_USART2_TX_PORT, BOARD_USART2_TX_PIN);
        HAL_GPIO_DeInit(BOARD_USART2_RX_PORT, BOARD_USART2_RX_PIN);
    } else if (uart->Instance == UART4) {
        __HAL_RCC_UART4_CLK_DISABLE();
        HAL_GPIO_DeInit(BOARD_USART4_TX_PORT, BOARD_USART4_TX_PIN);
        HAL_GPIO_DeInit(BOARD_USART4_RX_PORT, BOARD_USART4_RX_PIN);
    }
}
