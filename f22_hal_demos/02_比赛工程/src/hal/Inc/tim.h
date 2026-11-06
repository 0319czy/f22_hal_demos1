#ifndef F22_HAL_DEMOS_TIM_H
#define F22_HAL_DEMOS_TIM_H

#include "board_f22.h"
#include "stm32f1xx_hal.h"

/*
 * 定时器计数频率统一为 1 MHz，所以 CCR 的数值可以直接当作微秒读。
 *
 * 三个电机定时器的周期不同，沿用厂家例程的取值：
 *   M1~M4  TIM3 约 12 kHz（厂家 GENERAL_TIM3_Period=1000, Prescaler=5）
 *   M6/M7  TIM4 约 1 kHz （厂家 GENERAL_TIM4_Period=1999, Prescaler=35）
 *   M5     TIM1 约 1 kHz，直流减速电机用低频避免啸叫
 */
#define BOARD_TIM_MOTOR_STOP_US 0U

#define BOARD_TIM1_PERIOD_US 1000U
#ifndef BOARD_TIM3_PERIOD_US
#define BOARD_TIM3_PERIOD_US 83U   /* ≈12 kHz */
#endif
#ifndef BOARD_TIM4_PERIOD_US
#define BOARD_TIM4_PERIOD_US 1000U /* ≈1 kHz */
#endif

/* 电机编号，和手册接口定义的 M1~M7 一致。 */
#define BOARD_MOTOR_1 1U /* TIM3_CH1 -> PC6 */
#define BOARD_MOTOR_2 2U /* TIM3_CH2 -> PC7 */
#define BOARD_MOTOR_3 3U /* TIM3_CH3 -> PC8 */
#define BOARD_MOTOR_4 4U /* TIM3_CH4 -> PC9 */
#define BOARD_MOTOR_5 5U /* TIM1_CH1(正) / TIM1_CH4(反) -> PA8/PA11 */
#define BOARD_MOTOR_6 6U /* TIM4_CH1 -> PB6 */
#define BOARD_MOTOR_7 7U /* TIM4_CH2 -> PB7 */

/* M5 双向直流减速电机的转向。 */
#define BOARD_M5_STOP 0U
#define BOARD_M5_FORWARD 1U
#define BOARD_M5_REVERSE 2U

/*
 * PWM 输出极性。厂家例程和参考工程都用低有效。
 * 接执行器之前务必先用示波器确认：低有效时 CCR 越大高电平越短。
 */
#define BOARD_PWM_ACTIVE_LEVEL GPIO_PIN_RESET

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

/* 初始化三个电机 PWM 定时器及对应 GPIO，输出停在"停止"占空比。 */
void MX_TIM1_Init(void);
void MX_TIM3_Init(void);
void MX_TIM4_Init(void);

/* 在 HAL_TIM_PWM_Init 之后配置该定时器对应的复用输出引脚。 */
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *tim);

/* 启动三个定时器的 PWM 输出。上电默认停在 BOARD_TIM_MOTOR_STOP_US（不转）。 */
HAL_StatusTypeDef Board_MotorPwm_StartAll(void);

/*
 * 按百分号设定占空比，motor_index 取 BOARD_MOTOR_1..BOARD_MOTOR_7，
 * percent 取 0~100。这样调用方不需要知道各定时器周期不同（M1~M4 是 83 us，
 * M5/M6/M7 是 1000 us），避免手写 compare 时超过周期而永久高电平。
 */
HAL_StatusTypeDef Board_MotorPwm_SetDuty(uint8_t motor_index, uint8_t percent);

/*
 * M5 是唯一的双向直流减速电机，必须单独切方向。
 * 会把两个方向的 CCR 都按 duty 设置，direction 为 BOARD_M5_STOP 时全停。
 * 注意 M6/M7 与 I2C1 共用 PB6/PB7，两者不能同时用。
 */
HAL_StatusTypeDef Board_MotorPwm_SetM5(uint8_t direction, uint8_t percent);

/* 把某个电机的占空比设成 0（停转）。 */
HAL_StatusTypeDef Board_MotorPwm_Stop(uint8_t motor_index);

#endif /* F22_HAL_DEMOS_TIM_H */
