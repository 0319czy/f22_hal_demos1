#include "common.h"
#include "usart.h"

/*
 * Board_Log 走 USART1，因为 F22 的板载 CH340 接在 USART1 上
 * （原理图 USART1_TX/RXD1 与 CH340 相连），这是唯一能直接用 USB 线
 * 看到输出的串口。USART2/4 只引到 JP13/JP14 排针，必须外接串口模块。
 *
 * 它可能被任何例程在还没初始化串口之前调用，所以这里检查句柄是否已配置；
 * huart1 是文件级零初始化变量，未初始化时 Instance 为 0，直接跳过即可。
 */
static uint8_t Board_LogReady(void)
{
    return (huart1.Instance != (USART_TypeDef *)0) ? 1U : 0U;
}

void Board_Log(const char *text)
{
    if (!Board_LogReady()) {
        return;
    }
    /* 逐字节发送，省掉 strlen 依赖，也方便复用短字符串。 */
    while (*text != '\0') {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, 1U, 10U);
        ++text;
    }
}

void Board_LogHexByte(uint8_t value)
{
    static const char digit[] = "0123456789ABCDEF";
    char pair[3];

    pair[0] = digit[(value >> 4) & 0x0FU];
    pair[1] = digit[value & 0x0FU];
    pair[2] = '\0';
    Board_Log(pair);
}

/* 所有 HAL 初始化失败都会进入这里；裸机示例选择停机，便于调试器定位。 */
void Error_Handler(void)
{
    /* 关闭中断，避免错误状态下仍有后台中断继续修改外设。 */
    __disable_irq();
    while (1) {
    }
}

void Board_LogU32(uint32_t value)
{
    /* 先逆序取出数字，再用第二个缓冲正序拼出来。 */
    char reverse[10];
    char text[11];
    uint8_t count = 0U;
    uint8_t i;

    if (value == 0U) {
        Board_Log("0");
        return;
    }

    while ((value > 0U) && (count < sizeof(reverse))) {
        reverse[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count++;
    }

    /*
     * 必须用独立的 text 缓冲：直接在 reverse 上倒着写会覆盖还没读的字符
     *（1234 会变成 1221），而只靠尾部对齐又会漏掉结尾的 '\0'，
     * 导致把栈上的垃圾字节一起发出去。
     */
    for (i = 0U; i < count; ++i) {
        text[i] = reverse[count - 1U - i];
    }
    text[count] = '\0';

    Board_Log(text);
}

/* RCC->CSR 各复位标志位的名字，写清顺序与 RM0008 表 8-4 一致。 */
static const struct {
    uint32_t flag;
    const char *name;
} reset_flags[] = {
    {RCC_CSR_LPWRRSTF, "LPWR  低功耗"},
    {RCC_CSR_WWDGRSTF, "WWDG  看门狗"},
    {RCC_CSR_IWDGRSTF, "IWDG  看门狗"},
    {RCC_CSR_SFTRSTF, "SFTR  软件(程序调用复位)"},
    {RCC_CSR_PINRSTF, "PIN   按键/NRST"},
    {RCC_CSR_PORRSTF, "POR   上电/掉电"},
};

uint8_t Board_PrintResetCause(void)
{
    uint32_t csr = RCC->CSR;
    uint8_t found = 0U;
    uint8_t i;

    if ((csr & RCC_CSR_RMVF) != 0U) {
        Board_Log("  reset flags: (cleared)\r\n");
        RCC->CSR |= RCC_CSR_RMVF; /* 写 1 清除全部标志 */
        return 0U;
    }

    for (i = 0U; i < (sizeof(reset_flags) / sizeof(reset_flags[0])); ++i) {
        if ((csr & reset_flags[i].flag) != 0U) {
            Board_Log("  reset flag: ");
            Board_Log(reset_flags[i].name);
            Board_Log("\r\n");
            found = 1U;
        }
    }

    RCC->CSR |= RCC_CSR_RMVF;
    return found;
}
