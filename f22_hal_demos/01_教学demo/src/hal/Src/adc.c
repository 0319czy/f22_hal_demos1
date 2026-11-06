#include "board_f22.h"
#include "common.h"
#include "adc.h"

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/*
 * JP8 上 8 路模拟输入的顺序：ADC1~ADC8。
 * 对应 PB0(IN0)、PB1(IN1)、PC0~PC5(IN10~IN15)。
 * 厂家例程把这张表拆散在 bsp_adc.h 里，这里集中定义，避免顺序记错。
 *
 * 注意 STM32F1 的 ADC1 通道号不是 0~7：PC0~PC5 对应 IN10~IN15。
 */
const uint32_t board_adc_channel[BOARD_ADC_PORT_COUNT] = {
    ADC_CHANNEL_0,  /* ADC1 -> PB0 */
    ADC_CHANNEL_1,  /* ADC2 -> PB1 */
    ADC_CHANNEL_10, /* ADC3 -> PC0 */
    ADC_CHANNEL_11, /* ADC4 -> PC1 */
    ADC_CHANNEL_12, /* ADC5 -> PC2 */
    ADC_CHANNEL_13, /* ADC6 -> PC3 */
    ADC_CHANNEL_14, /* ADC7 -> PC4 */
    ADC_CHANNEL_15, /* ADC8 -> PC5 */
};

const uint8_t board_adc_pin_index[BOARD_ADC_PORT_COUNT] = {
    0U, /* PB0 -> GPIO_PIN_0 */
    1U, /* PB1 -> GPIO_PIN_1 */
    2U, /* PC0 -> GPIO_PIN_2 */
    3U, /* PC1 -> GPIO_PIN_3 */
    4U, /* PC2 -> GPIO_PIN_4 */
    5U, /* PC3 -> GPIO_PIN_5 */
    6U, /* PC4 -> GPIO_PIN_6 */
    7U, /* PC5 -> GPIO_PIN_5 -> GPIO_PIN_7 */
};

void MX_ADC1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 8 路模拟输入分布在 PB0/PB1 和 PC0~PC5，两组端口时钟都要开。 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* 模拟输入必须配置成模拟模式，数字输入缓冲会干扰采样。 */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
               GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);

    /*
     * F1 的 ADC 固定 12 位，没有 Resolution/Overrun 字段；
     * 也没有 DMAContinuousRequests，DMA 循环模式由 DMA 自己的 Mode 决定。
     */
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.NbrOfConversion = BOARD_ADC_PORT_COUNT;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion = 0U;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /*
     * 规则组 rank 顺序必须与 board_adc_channel[] 一致，
     * 轮询取第 N 个 rank 的结果才对应 JP8 的第 N 路输入。
     * 55.5 周期采样时间约 14 us，8 路扫描总耗时约 110 us。
     */
    for (uint32_t rank = 0U; rank < BOARD_ADC_PORT_COUNT; ++rank) {
        ADC_ChannelConfTypeDef channel = {0};

        channel.Channel = board_adc_channel[rank];
        channel.Rank = rank + 1U; /* HAL 的 rank 从 1 开始 */
        channel.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
        if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) {
            Error_Handler();
        }
    }

    /* F1 的 ADC1 DMA 请求固定走 DMA1 通道 1。 */
    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }
    /* F1 句柄里的字段名是 DMA_Handle，不是新系列的 DMA_Channel。 */
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);
}

uint32_t Board_AdcCodeToPortMv(uint16_t code)
{
    /*
     * 12 位量程对应 MCU 的 3.3 V 参考，板载二分压再乘 2。
     * 手册原话："采集到的信号值应乘以 2 才是端口上的实际信号值"。
     * 用 64 位中间值避免 4095 * 7200 溢出 32 位乘法路径。
     */
    uint64_t mv = ((uint64_t)code * BOARD_ADC_FULL_SCALE_MV) / 4095U;
    return (uint32_t)mv;
}

/* --- 运行时读取接口 -------------------------------------------------------- */

/*
 * 做一次单通道的单次转换。
 * F1 的 ADC1 只有一个数据寄存器 DR，转换结束后 EOC 置位，
 * 所以这里用轮询等 EOC，再读 DR。
 */
static uint16_t Adc_ReadChannelOnce(uint32_t channel, uint8_t verbose)
{
    ADC_ChannelConfTypeDef conf = {0};

    /*
     * 没调 MX_ADC1_Init() 时 hadc1.Instance 还是 0。
     * 直接往下走 HAL_ADC_Init 会解空指针进 HardFault，现象是"标题打印出来了
     * 但后面没结果"，很难查。这里直接返回 0 并让上层报 FAIL。
     */
    if (hadc1.Instance == (ADC_TypeDef *)0) {
        if (verbose != 0U) {
            Board_Log("adc: not initialised\r\n");
        }
        return 0U;
    }

    /*
     * 必须重新配置成"只转一个通道、转一次就停"。
     *
     * MX_ADC1_Init() 是按 DMA 循环采集配的：ContinuousConvMode = ENABLE、
     * 规则组 8 个通道。这种状态下 CONT 一置位，转换序列结束后会立刻开始
     * 下一轮，我们等 EOC 去读 DR 时，DR 里已经是序列里别的通道了 ——
     * 表现就是同一个通道两次读出完全不同的值，或者读出来恒为 0。
     *
     * 注意 F1 的 rank 与寄存器是反的：rank 1~6 对应 SQR3 而不是 SQR1，
     * 所以这里 rank 传 1（也就是 SQR3 SQ1）就能把通道放到序列第一位。
     */
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1U;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        if (verbose != 0U) {
            Board_Log("adc: HAL_ADC_Init failed\r\n");
        }
        return 0U;
    }

    conf.Channel = channel;
    conf.Rank = 1U;
    /*
     * 内部基准电压通道要求采样时间至少 17.5 us，取最长的 55.5 周期。
     * 另外注意 HAL 是按 Channel >= 10 来选 SMPR1 还是 SMPR2 的，
     * ADC_CHANNEL_17 的位域与 ADC_CHANNEL_10~15 不同，
     * 所以内部通道的采样时间实际由 SMPR2 的 SMP1 位决定，
     * 这里沿用 55.5 周期以留足建立时间。
     */
    conf.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &conf) != HAL_OK) {
        if (verbose != 0U) {
            Board_Log("adc: ConfigChannel failed\r\n");
        }
        return 0U;
    }

    /*
     * HAL 没法给内部通道设置采样时间，必须在这里补。
     * HAL_ADC_ConfigChannel 用 ADC_SMPR1(_TIME_, _CH_) = _TIME_ << (3*(_CH_-10))
     * 定位字段，而 ADC_CHANNEL_17 的数值就是 17，代进去是 3*(17-10)=21 位移量，
     * 掩码落到 SMPR1 的保留位上（SMPR1 只有 bit 0~17），等于什么都没写 ——
     * 外部通道 0~15 不受影响，唯独内部通道 16/17 一直是默认的最短采样时间。
     *
     * 放在 ConfigChannel 之后，直接把所有字段写满 55.5 周期：
     * SMPR2 = SMP0~SMP9（bit 0~29），SMPR1 = SMP10~SMP15（bit 0~17）。
     */
    if ((channel == ADC_CHANNEL_16) || (channel == ADC_CHANNEL_17)) {
        hadc1.Instance->SMPR2 = 0x3FFFFFFFUL;
        hadc1.Instance->SMPR1 = 0x0003FFFFUL;
    }

    /* 必须在 Start 之前清：Start 之后转换已经开始了，那时清已经太晚。 */
    CLEAR_BIT(hadc1.Instance->CR2, ADC_CR2_CONT);

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        if (verbose != 0U) {
            Board_Log("adc: Start failed\r\n");
        }
        return 0U;
    }

    if (verbose != 0U) {
        Board_Log("adc: started, waiting EOC\r\n");
    }

    /* HAL_ADC_Start 内部已经把 ADC 使能，这里只需要等 EOC。 */
    {
        uint32_t timeout = 1000U;
        while ((hadc1.Instance->SR & ADC_SR_EOC) == 0U) {
            if (--timeout == 0U) {
                if (verbose != 0U) {
                    Board_Log("adc: EOC timeout\r\n");
                }
                (void)HAL_ADC_Stop(&hadc1);
                return 0U;
            }
        }
    }

    if (verbose != 0U) {
        Board_Log("adc: EOC ok\r\n");
    }
    return (uint16_t)hadc1.Instance->DR;
}

static uint16_t Adc_ReadOneChannel(uint32_t channel)
{
    return Adc_ReadChannelOnce(channel, 1U);
}

uint16_t Board_AdcReadCode(uint8_t port_index)
{
    if (port_index >= BOARD_ADC_PORT_COUNT) {
        return 0U;
    }
    return Adc_ReadOneChannel(board_adc_channel[port_index]);
}

/*
 * VREFINT 的标称输出电压。数据手册给的是 1.204 V @ VDDA=3.3V、TA=25C。
 * 注意这不是 3.0 V —— 之前误用 3000 当分子，算出来的 VDDA 是真实值的
 * 2.5 倍（1159 码时报 10599 mV，荒谬）。
 * 正确关系：raw = 1204 mV / VDDA * 4095，反解 VDDA = 1204 * 4095 / raw。
 */
#define BOARD_VREFINT_NOMINAL_MV 1204UL
#define BOARD_ADC_FULL_SCALE 4095UL
/* 一次转换的采样时间远短于 VREFINT 的建立时间，靠多次平均压制残差。 */
#define BOARD_VREFINT_SAMPLES 16U

/* Board_AdcReadVrefRaw() 采到的原始码，供上层判决复用。 */
static uint16_t g_vrefint_raw;

uint16_t Board_AdcReadVrefRaw(void)
{
    /*
     * STM32F103 没有出厂 VREFINT 校准值 —— F1 的信息块里只有唯一 ID
     * (0x1FFFF7E8) 和 Flash 容量 (0x1FFFF7F0)，没有 VREFINT_CAL。
     * 0x1FFFFE10 那个地址是 F411/F401 用的，在 F103 上读它会触发
     * 精确数据总线错误（未映射地址），表现为直接 HardFault。
     * 所以只能按标称值反推，精度受 VREFINT 自身容差限制（百分之几），
     * 适合判断供电是否明显异常，不能当精密电压表。
     */
    uint32_t sum = 0UL;
    uint32_t i;
    uint16_t raw;

    /*
     * 关键：数据手册规定 VREFINT 上电稳定时间最长 50 us。
     * ADC 时钟 PCLK2/6 = 12 MHz，55.5 周期采样只有 4.6 us，
     * 单靠采样时间不够，还要等外部建立时间。
     *
     * 60 ms 是故意给得很大：F103 没有 VREFINT 工厂校准值，
     * 内部基准本身的容差就有百分之几。这里求的是"供电是否明显异常"，
     * 不是精密测量，所以宁可等久一点换稳定。
     *
     * 另外不要在这里直接写 SMPR1/SMPR2：Adc_ReadChannelOnce() 每采一个
     * 样都会调 HAL_ADC_Init()，而它会按 Init.SampleTime 把这两个寄存器
     * 整体重刷一遍，直接赋值会被当场冲掉。真正的修法是设
     * Init.SampleTime（见 MX_ADC1_Init），让 HAL 自己把 55.5 周期
     * 写进包括内部通道在内的每一个字段。
     */
    HAL_Delay(60U);

    for (i = 0U; i < BOARD_VREFINT_SAMPLES; ++i) {
        sum += Adc_ReadChannelOnce(ADC_CHANNEL_17, 0U);
    }
    raw = (uint16_t)(sum / BOARD_VREFINT_SAMPLES);

    Board_Log("  VREFINT raw=");
    Board_LogU32(raw);
    Board_Log(" (");
    Board_LogU32(BOARD_VREFINT_SAMPLES);
    Board_Log(" samples averaged)\r\n");

    /*
     * 顺带读内部温度传感器（ch16）做交叉验证。
     * 内部通道能不能用、VREFINT 标称值对不对，靠这一个数就能判断：
     *   - 温度落在 15~40 C  -> 内部通道正常，问题只在 VREFINT 标称值
     *                          （可能不是 STM32 原厂料），供电本身没问题
     *   - 温度离谱（几百甚至负几十度）-> 通道映射不对，读到的根本不是
     *                          温度传感器，VDDA 的换算也就无从谈起
     */
    {
        uint16_t raw_temp = Adc_ReadChannelOnce(ADC_CHANNEL_16, 0U);
        /*
         * STM32F1 公式：T = (V25 - Vs)/AvgSlope + 25
         * V25 = 760 mV @ 25C，AvgSlope = 2.5 mV/C。
         * Vs = raw_temp * VDDA / 4095，用 VREFINT 反推出的 VDDA 代入；
         * 这里只是为了看数量级，所以用 3.3 V 假定值即可。
         *
         * 注意结果可能是负数（实测确实如此），而 Board_LogU32 只吃无符号，
         * 直接把负的 int32_t 丢进去会变成 4294967294 这种下溢值，
         * 必须自己处理符号。
         */
        int32_t temp_c_x10 =
            (int32_t)((760.0f - (raw_temp * 3.3f / 4095.0f) * 1000.0f) / 2.5f) + 250;

        Board_Log("  TEMPSENS raw=");
        Board_LogU32(raw_temp);
        Board_Log(" -> ");
        if (temp_c_x10 < 0) {
            Board_Log("-");
            temp_c_x10 = -temp_c_x10;
        }
        Board_LogU32((uint32_t)(temp_c_x10 / 10));
        Board_Log(".");
        Board_LogU32((uint32_t)(temp_c_x10 % 10));
        Board_Log(" C (sanity check only)\r\n");
    }

    /* 存下来给上层做判决用，避免为了拿原始码再重复采样一遍。 */
    g_vrefint_raw = raw;
    return raw;
}

/* 取上一次 Board_AdcReadVrefRaw() 的原始码。 */
uint16_t Board_AdcLastVrefRaw(void)
{
    return g_vrefint_raw;
}
uint32_t Board_AdcReadVrefMv(void)
{
    uint16_t raw = Board_AdcReadVrefRaw();

    /*
     * 派生电压仅供参考，不能当结论。
     * 手册 2.2 节写明 DAC 输出范围 0~3.6 V，暗示 VDDA = 3.6 V；
     * 但代入 VREFINT 标称 1.204 V 反推却得到 2659 mV，两者对不上。
     * F103 没有 VREFINT 工厂校准值（百分之几容差），这颗片子也未必是
     * STM32 原厂，所以这个数只能当"大致量级"看。
     * 需要真实电压就拿万用表量 JP10 或 3.6V 轨，不要信这里。
     */
    if (raw < 100U) {
        return 0U;
    }
    return (BOARD_VREFINT_NOMINAL_MV * BOARD_ADC_FULL_SCALE) / raw;
}
