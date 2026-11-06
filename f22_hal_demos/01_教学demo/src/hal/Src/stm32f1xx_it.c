#include "stm32f1xx_hal.h"
#include "usart.h"

/* SysTick 是 HAL tick、HAL_Delay 和所有 HAL 超时的共同时间基准。 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}

/* PC12/PC13 落在 EXTI15_10 这一条共享中断入口上。 */
void EXTI15_10_IRQHandler(void)
{
    /* HAL 会在这里判断是 PC12 还是 PC13，并清除对应挂起位。 */
    HAL_GPIO_EXTI_IRQHandler(BOARD_KEY1_Pin);
    HAL_GPIO_EXTI_IRQHandler(BOARD_KEY2_Pin);
}

/* 只有启用 USART1 中断的例程才会真正进入这条入口。 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* ------------------------------------------------------------------------- */
/* HardFault 诊断                                                              */
/* ------------------------------------------------------------------------- */
/*
 * 目的：把"标题打印出来了但后面没输出"这种症状变成可定位的信息。
 * 遇到总线异常或非法指令时，Cortex-M3 会把出错时的 PC/LR/xPSR 压栈，
 * 这里把这三个值和 SCB 的 CFSR/HFSR 一起打出来，就能知道是访问了
 * 非法地址（比如空指针）还是执行到了错误的指令。
 *
 * 注意：裸机程序没有 RTOS，异常总是用 MSP，所以直接读当前栈顶就是
 * 出错时的压栈内容，不需要做 MSP/PSP 判别。
 */
extern void Board_Log(const char *text);
extern void Board_LogHexByte(uint8_t value);

static void Fault_PrintHex32(uint32_t value)
{
    static const char digit[] = "0123456789ABCDEF";
    uint8_t i;

    for (i = 0U; i < 8U; ++i) {
        char pair[3];

        pair[0] = digit[(value >> ((7U - i) * 4U)) & 0x0FU];
        pair[1] = '\0';
        Board_Log(pair);
    }
}

void HardFault_Handler(void)
{
    /*
     * 出错时硬件自动压栈的帧：[0]=PC [1]=LR [2]=xPSR ...
     *
     * 关键：如果 HFSR 的 FORCED(bit30) 置位，说明是 BusFault/UsageFault
     * "升级"成 HardFault 的。此时压了两层帧 —— BusFault 帧先压在高地址，
     * HardFault 帧后压在低地址，所以 MSP 指向的这层里 PC/LR 是无意义的
     * 中断返回地址（常见值 0x00000000 / 0x00000008），真正的出错指令地址
     * 在下面一帧，也就是 MSP+8。不区分就会把 0x800 这种垃圾值当成 PC，
     * addr2line 也定位不到。
     */
    const uint32_t *frame = (const uint32_t *)__get_MSP();

    if ((SCB->HFSR & (1UL << 30)) != 0U) {
        frame += 8; /* 跳过升级帧，取原始出错帧 */
    }

    {
        uint32_t pc = frame[0];
        uint32_t lr = frame[1];

        Board_Log("\r\n!!!!!!!! HARD FAULT !!!!!!!!\r\n");
        if ((SCB->HFSR & (1UL << 30)) != 0U) {
            Board_Log("  (escalated from BusFault, real frame used)\r\n");
        }
        Board_Log("  PC   = 0x");
        Fault_PrintHex32(pc);
        Board_Log("\r\n  LR   = 0x");
        Fault_PrintHex32(lr);
        Board_Log("\r\n  CFSR = 0x");
        Fault_PrintHex32(SCB->CFSR);
        Board_Log("\r\n  HFSR = 0x");
        Fault_PrintHex32(SCB->HFSR);
        Board_Log("\r\n");

        /* BFARVALID 置位时 BFAR 就是出错的数据/指令地址，最有用。 */
        if ((SCB->CFSR & (1UL << 15)) != 0U) {
            Board_Log("  BFAR = 0x");
            Fault_PrintHex32(SCB->BFAR);
            Board_Log("  (faulting address)\r\n");
        }
        if ((SCB->CFSR & (1UL << 7)) != 0U) {
            Board_Log("  BFAR = 0x");
            Fault_PrintHex32(SCB->BFAR);
            Board_Log("  (faulting address, imprecise)\r\n");
        }
    }

    /* CFSR 的低 16 位是 MemManage/BusFault 的细分原因，逐条说明最快定位。 */
    if ((SCB->CFSR & (1UL << 0)) != 0U) {
        Board_Log("  cause: instruction fetch from invalid address\r\n");
    }
    if ((SCB->CFSR & (1UL << 1)) != 0U) {
        Board_Log("  cause: data access to invalid address (null ptr?)\r\n");
    }
    if ((SCB->CFSR & (1UL << 3)) != 0U) {
        Board_Log("  cause: unstack error (corrupted stack)\r\n");
    }
    if ((SCB->CFSR & (1UL << 11)) != 0U) {
        Board_Log("  cause: unstacking error (bad exception entry)\r\n");
    }
    if ((SCB->CFSR & (1UL << 16)) != 0U) {
        Board_Log("  cause: undefined instruction\r\n");
    }
    if ((SCB->CFSR & (1UL << 17)) != 0U) {
        Board_Log("  cause: invalid EPSR state (corrupt stack?)\r\n");
    }
    if ((SCB->CFSR & (1UL << 19)) != 0U) {
        Board_Log("  cause: no coprocessor (FPU access on Cortex-M3!)\r\n");
    }

    /*
     * 打印完必须停住，不能返回：HardFault_Handler 返回会让 CPU
     * 跳到下一条指令，死循环会把这个位置反复打印。
     */
    __disable_irq();
    while (1) {
    }
}
