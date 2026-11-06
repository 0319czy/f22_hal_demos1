#include "board_f22.h"
#include "common.h"
#include "tim.h"

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/*
 * 初始化结束后清零状态：先关输出比较、关 MOE、清计数、全部 CCR 置 0。
 * 这样即使 demo 忘了启动 PWM，电机驱动也不会被一个上电残留脉冲带动。
 */
static void Timer_ClearToStop(TIM_HandleTypeDef *tim)
{
    /*
     * 没调过 MX_TIMx_Init() 时 tim->Instance 还是 0，
     * 下面 __HAL_TIM_DISABLE 和 tim->Instance->CCER 都会解空指针进 HardFault。
     * 这个函数是"让电机停住"的兜底路径，绝不能自己变成崩溃源。
     */
    if (tim == NULL || tim->Instance == (TIM_TypeDef *)0) {
        return;
    }

    __HAL_TIM_DISABLE(tim);
    /* TIM_CCER 的四个使能位是 CC1E/CC2E/CC3E/CC4E，对应位 0/2/4/6。 */
    tim->Instance->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E |
                             TIM_CCER_CC3E | TIM_CCER_CC4E);
    __HAL_TIM_SET_COUNTER(tim, 0U);
    __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_1, BOARD_TIM_MOTOR_STOP_US);
    __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_2, BOARD_TIM_MOTOR_STOP_US);
    __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_3, BOARD_TIM_MOTOR_STOP_US);
    __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_4, BOARD_TIM_MOTOR_STOP_US);
    /* TIM1 是高级定时器，还必须单独关掉主输出使能位 MOE。 */
    if (tim->Instance == TIM1) {
        __HAL_TIM_MOE_DISABLE(tim);
    }
}

static void Timer_InitPwm(TIM_HandleTypeDef *tim,
                          TIM_TypeDef *instance,
                          uint32_t period_us)
{
    TIM_ClockConfigTypeDef clock_source = {0};
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef channel = {0};

    tim->Instance = instance;
    /* APB 定时器时钟 72 MHz / (71+1) = 1 MHz，CCR 数值可直接当微秒。 */
    tim->Init.Prescaler = 71U;
    tim->Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->Init.Period = period_us;
    tim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(tim) != HAL_OK) {
        Error_Handler();
    }

    clock_source.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(tim, &clock_source) != HAL_OK ||
        HAL_TIM_PWM_Init(tim) != HAL_OK) {
        Error_Handler();
    }

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(tim, &master) != HAL_OK) {
        Error_Handler();
    }

    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = BOARD_TIM_MOTOR_STOP_US;
    channel.OCPolarity = TIM_OCPOLARITY_LOW;
    channel.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(tim, &channel, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(tim, &channel, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(tim, &channel, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(tim, &channel, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(tim);
    Timer_ClearToStop(tim);
}

/* M1~M4：TIM3 完全重映射到 PC6~PC9。 */
void MX_TIM3_Init(void)
{
    Timer_InitPwm(&htim3, TIM3, BOARD_TIM3_PERIOD_US);
}

/* M6/M7：TIM4 的 PB6/PB7，手册只用 CH1/CH2。 */
void MX_TIM4_Init(void)
{
    Timer_InitPwm(&htim4, TIM4, BOARD_TIM4_PERIOD_US);
}

/* M5：TIM1 的 PA8(OUTA1) 与 PA11(OUTB1)，两路配合实现双向。 */
void MX_TIM1_Init(void)
{
    Timer_InitPwm(&htim1, TIM1, BOARD_TIM1_PERIOD_US);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *tim)
{
    /* 只开时钟，GPIO 复用放到 HAL_TIM_MspPostInit 里统一处理。 */
    if (tim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    } else if (tim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    } else if (tim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *tim)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    if (tim->Instance == TIM1) {
        /*
         * 只用 PA8=T1_CH1(OUTA1)。绝对不能碰 PA11：它是 USB_DM，
         * 配成 AF_PP 会让 MCU 彻底不再是 USB 设备，表现为"按完 KEY1 串口再无输出"。
         * TIM1_CH4 没有其他可用脚：PA11 归 USB，PB14 需开 TIM1 全重映射，
         * 而全重映射会把 CH1 从 PA8 挪到 PE9。
         */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin = BOARD_M5_Pin;
        HAL_GPIO_Init(GPIOA, &gpio);
    } else if (tim->Instance == TIM3) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        /* PC6~PC9 只有开启 TIM3 完全重映射后才由 TIM3 驱动。 */
        __HAL_AFIO_REMAP_TIM3_ENABLE();
        gpio.Pin = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
        HAL_GPIO_Init(GPIOC, &gpio);
    } else if (tim->Instance == TIM4) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
        HAL_GPIO_Init(GPIOB, &gpio);
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *tim)
{
    if (tim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, BOARD_M5_Pin);
    } else if (tim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, BOARD_M1_Pin | BOARD_M2_Pin |
                               BOARD_M3_Pin | BOARD_M4_Pin);
    } else if (tim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, BOARD_M6_Pin | BOARD_M7_Pin);
    }
}

/* --- 电机 PWM 运行时接口 ---------------------------------------------------- */

/*
 * 找到 motor_index 对应的 (定时器, 通道, 周期)。
 * 返回 NULL 表示编号越界（只有 M1~M7 七个口）。
 */
typedef struct {
    TIM_HandleTypeDef *tim;
    uint32_t channel;
    uint32_t period_us;
} MotorMapping;

static const MotorMapping *Motor_Lookup(uint8_t motor_index)
{
    /* 与手册接口定义顺序一致：M1~M4=TIM3, M5=TIM1, M6/M7=TIM4。 */
    static const MotorMapping table[] = {
        {&htim3, TIM_CHANNEL_1, BOARD_TIM3_PERIOD_US}, /* M1 */
        {&htim3, TIM_CHANNEL_2, BOARD_TIM3_PERIOD_US}, /* M2 */
        {&htim3, TIM_CHANNEL_3, BOARD_TIM3_PERIOD_US}, /* M3 */
        {&htim3, TIM_CHANNEL_4, BOARD_TIM3_PERIOD_US}, /* M4 */
        {&htim1, TIM_CHANNEL_1, BOARD_TIM1_PERIOD_US}, /* M5 正转 */
        {&htim4, TIM_CHANNEL_1, BOARD_TIM4_PERIOD_US}, /* M6 */
        {&htim4, TIM_CHANNEL_2, BOARD_TIM4_PERIOD_US}, /* M7 */
    };

    if (motor_index < 1U || motor_index > BOARD_MOTOR_7) {
        return NULL;
    }
    return &table[motor_index - 1U];
}

HAL_StatusTypeDef Board_MotorPwm_StartAll(void)
{
    /*
     * 句柄没初始化时直接失败。Timer_ClearToStop() 虽然加了空指针保护，
     * 但后面的 HAL_TIM_PWM_Start() 一样会解空指针，所以在这里挡掉，
     * 让调用方拿到明确的 HAL_ERROR 而不是 HardFault。
     */
    if (htim1.Instance == (TIM_TypeDef *)0 ||
        htim3.Instance == (TIM_TypeDef *)0 ||
        htim4.Instance == (TIM_TypeDef *)0) {
        return HAL_ERROR;
    }

    /*
     * 先把所有 CCR 归零再启动：PWM1 模式下 CCR=0 输出恒为低（不转），
     * 避免 Start 之前引脚停在高电平让电机突然转起来。
     */
    Timer_ClearToStop(&htim3);
    Timer_ClearToStop(&htim4);
    Timer_ClearToStop(&htim1);

    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef Board_MotorPwm_SetDuty(uint8_t motor_index, uint8_t percent)
{
    const MotorMapping *motor = Motor_Lookup(motor_index);

    if (motor == NULL) {
        return HAL_ERROR;
    }
    if (percent > 100U) {
        percent = 100U;
    }

    /*
     * CCR 上限取 period_us - 1。用 period_us 本身的话在 PWM1 下
     * 输出会一直是高（100% 反而可能因为超过 ARR 而不翻转），
     * 留一个计数值更符合定时器语义。
     */
    {
        uint32_t compare = (motor->period_us * percent) / 100U;
        if (compare >= motor->period_us) {
            compare = motor->period_us - 1U;
        }
        __HAL_TIM_SET_COMPARE(motor->tim, motor->channel, compare);
    }
    return HAL_OK;
}

HAL_StatusTypeDef Board_MotorPwm_SetM5(uint8_t direction, uint8_t percent)
{
    if (direction > BOARD_M5_REVERSE || percent > 100U) {
        return HAL_ERROR;
    }
    if (direction == BOARD_M5_STOP) {
        percent = 0U;
    }

    /*
     * 正反两路必须互斥。
     * 之前这里给 CH1 和 CH4 写了同一个 compare，正转和反转输出一模一样，
     * 双向接口等于没实现；而且两路同时有 PWM 输出会让 M5 的 H 桥上下桥臂
     * 同时导通，属于危险的直通工况。
     * 正确做法：先把两路都清零，再只驱动方向对应的那一路。
     */
    {
        uint32_t compare = (BOARD_TIM1_PERIOD_US * percent) / 100U;
        if (compare >= BOARD_TIM1_PERIOD_US) {
            compare = BOARD_TIM1_PERIOD_US - 1U;
        }
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0U);

        if (direction == BOARD_M5_FORWARD) {
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
        } else {
            /* M5 反转所需的第二个控制脚未确认，不驱动，避免破坏 USB。 */
            return HAL_ERROR;
        }
        /* BOARD_M5_STOP 时两路都保持 0，电机停转。 */
    }
    return HAL_OK;
}

HAL_StatusTypeDef Board_MotorPwm_Stop(uint8_t motor_index)
{
    return Board_MotorPwm_SetDuty(motor_index, 0U);
}
