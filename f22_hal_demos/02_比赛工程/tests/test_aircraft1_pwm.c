#include "aircraft1_pwm.h"
#include "stm32f1xx_hal.h"
#include "closed_loop_profile.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#if defined(TEST_CLOSED_LOOP_PWM) || defined(TEST_DIRECTION_CHECK) || defined(TEST_ATTITUDE_CHECK)
#include "closed_loop.h"
#include "closed_loop_config.h"
#endif
#ifdef TEST_DIRECTION_CHECK
#include "motor_direction.h"
#endif
#ifdef TEST_ATTITUDE_CHECK
#include "attitude_check.h"
#endif
#ifndef AIRCRAFT1_PWM_MAX_DUTY
#define AIRCRAFT1_PWM_MAX_DUTY CL_MAX_DUTY
#endif
TIM_TypeDef fake_tim3, fake_tim4;
GPIO_TypeDef fake_gpio_a, fake_gpio_b, fake_gpio_c;
#if defined(TEST_CLOSED_LOOP_PWM) || defined(TEST_DIRECTION_CHECK) || defined(TEST_ATTITUDE_CHECK)
static void __attribute__((unused)) ZeroTrims(ClosedLoop *c)
{
    c->tuning.left_trim = c->tuning.front_trim = c->tuning.yaw_trim = 0.0f;
    c->tuning.collective_trim = 0.0f;
}
#endif
static uint32_t mask, tick, c_pins, b_pins;
static uint16_t pressed;
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1U; }
void __set_PRIMASK(uint32_t m) { mask = m; }
uint32_t HAL_GetTick(void) { return tick; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{ assert(port == GPIOC); return pressed & pin ? GPIO_PIN_RESET : GPIO_PIN_SET; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio)
{
    assert(gpio->Mode == GPIO_MODE_AF_PP);
    assert(TIM3->CCR1 == 0 && TIM3->CCR2 == 0 && TIM3->CCR3 == 0 && TIM4->CCR1 == 0);
    if (port == GPIOC) c_pins |= gpio->Pin;
    else { assert(port == GPIOB); b_pins |= gpio->Pin; }
}
static void Zero(void)
{ assert(TIM3->CCR1 == 0 && TIM3->CCR2 == 0 && TIM3->CCR3 == 0 && TIM4->CCR1 == 0); }
#ifdef TEST_CLOSED_LOOP_PWM
static void ControllerToPwm(void)
{
    ClosedLoop controller;
    ClosedLoopOutput output;
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, 1U, 0U};
    ClosedLoopSample sample = {0};
    sample.imu_valid = sample.link_valid = sample.tof_present = sample.tof_fresh = 1U;
#ifdef CL_FLOW_ASSIST
    cfg.use_flow = 1U;
    sample.flow_present = sample.flow_calibrated = 1U; /* synthetic acceptance */
    unsigned launch_steps = 360U; /* 2.16s reaches 100%, before 3s timeout */
#elif defined(CL_TETHERED_PWM100)
    unsigned launch_steps = 360U; /* 2.16s reaches 100%, before 3s timeout */
#else
    unsigned launch_steps = 310U;
#endif
    /* Near-ground invalid ToF is expected during the real launch ramp. */
    sample.range_m = .01f;
    mask = 0; tick = 1000; Aircraft1Pwm_Clear();
    ClosedLoop_Init(&controller, cfg);
    /* PWM register assertions isolate the driver from operator trim values. */
    controller.tuning.left_trim = controller.tuning.front_trim =
        controller.tuning.yaw_trim = controller.tuning.collective_trim = 0.0f;
    for (unsigned i = 0; i < launch_steps; ++i) {
        tick += 6;
        sample.output_fault = Aircraft1Pwm_Tripped();
        ClosedLoop_Step(&controller, &sample, i == 0 ? HOVER_START : HOVER_NONE,
                        .006f, &output);
        assert(output.state == CL_LAUNCH && output.fault == CL_OK);
        if (!Aircraft1Pwm_Apply(output.motor, tick)) {
            fprintf(stderr, "Controller/PWM mismatch at step %u: demand=%f ceiling=%f\n",
                    i, (double)output.motor[0], (double)AIRCRAFT1_PWM_MAX_DUTY);
            assert(!"controller launch rejected by PWM driver");
        }
        HAL_SYSTICK_Callback(); assert(!Aircraft1Pwm_Tripped());
    }
    /* 2026-10-28：起飞斜坡上限改为 max_duty（hover_duty 只作高度环前馈，
     * 不再给斜坡封顶）。斜坡终值 = min(斜率×时间, max_duty)，各 profile 不同。 */
    float ramp_end = CL_LAUNCH_RAMP_DUTY_PER_SEC * (float)launch_steps * .006f;
    if (ramp_end > CL_MAX_DUTY) ramp_end = CL_MAX_DUTY;
    unsigned expected = (unsigned)(ramp_end * 200.0f);
    assert(TIM3->CCR1 == expected && TIM3->CCR2 == expected &&
           TIM3->CCR3 == expected && TIM4->CCR1 == expected);
    /* Tilt corrections must also pass the same driver ceiling. */
    sample.roll = -1.8f; sample.pitch = -1.7f; tick += 6;
    ClosedLoop_Step(&controller, &sample, HOVER_NONE, .006f, &output);
    assert(Aircraft1Pwm_Apply(output.motor, tick));
    ClosedLoop_Step(&controller, &sample, HOVER_STOP, .006f, &output);
    assert(output.state == CL_IDLE && output.fault == CL_OK);
    assert(Aircraft1Pwm_Apply(output.motor, tick)); Zero();
    printf("Controller -> PWM register-double launch/tilt/stop passed: nominal=%.0f%% ceiling=%.0f%% (no hardware)\n",
           (double)(CL_HOVER_DUTY * 100.0f), (double)(AIRCRAFT1_PWM_MAX_DUTY * 100.0f));
}
#endif
#ifdef TEST_DIRECTION_CHECK
static void DirectionToPwm(void)
{
    ClosedLoop c; ClosedLoopOutput out; ClosedLoopSample sample = {0};
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, 1U, 0U};
    ClosedLoop_Init(&c, cfg); mask = 0; tick = 1000; Aircraft1Pwm_Clear();
    ZeroTrims(&c);
    MotorDirection_Step(&c, &sample, HOVER_NONE, .006f, &out);
    assert(out.state == CL_IDLE); assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    sample.link_valid = 1U;
    /* Six seconds with invalid sensors: diagnostic stays exactly5%, never ramps. */
    for (unsigned i = 0; i < 1000; ++i) {
        tick += 6;
        MotorDirection_Step(&c, &sample, i == 0 ? HOVER_START : HOVER_NONE, .006f, &out);
        assert(out.state == CL_HOLD && out.fault == CL_OK);
        for (unsigned k = 0; k < 4; ++k) assert(out.motor[k] == .05f);
        assert(Aircraft1Pwm_Apply(out.motor, tick));
        assert(TIM3->CCR1 == 10 && TIM3->CCR2 == 10 && TIM3->CCR3 == 10 && TIM4->CCR1 == 10);
        HAL_SYSTICK_Callback(); assert(!Aircraft1Pwm_Tripped());
    }
    MotorDirection_Step(&c, &sample, HOVER_REQUEST_LAND, .006f, &out);
    assert(out.state == CL_IDLE); assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    MotorDirection_Step(&c, &sample, HOVER_START, .006f, &out);
    sample.link_valid = 0;
    MotorDirection_Step(&c, &sample, HOVER_NONE, .006f, &out);
    assert(out.state == CL_FAULT && out.fault == CL_LINK_LOST);
    assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    sample.link_valid = 1;
    MotorDirection_Step(&c, &sample, HOVER_START, .006f, &out);
    assert(out.state == CL_FAULT); assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    MotorDirection_Step(&c, &sample, HOVER_STOP, NAN, &out);
    assert(out.state == CL_IDLE && out.fault == CL_OK);
    sample.output_fault = 1;
    MotorDirection_Step(&c, &sample, HOVER_START, .006f, &out);
    assert(out.state == CL_FAULT && out.fault == CL_OUTPUT_FAULT);
    MotorDirection_Step(&c, &sample, HOVER_STOP, .006f, &out); sample.output_fault = 0;
    MotorDirection_Step(&c, &sample, HOVER_START, .020f, &out);
    assert(out.state == CL_FAULT && out.fault == CL_TIMING);
    assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    puts("Direction diagnostic -> PWM: fixed5%,10/200 ticks, boot0, F3/F4, link/trip/timing faults passed (no hardware)");
}
#endif
#ifdef TEST_ATTITUDE_CHECK
static ClosedLoopSample BenchSample(void)
{
    ClosedLoopSample s = {0}; s.imu_valid = s.link_valid = 1U;
    /* Absent/invalid range and flow remain invalid, not replaced with height. */
    s.range_m = s.vz = s.vx = s.vy = NAN; return s;
}
static void BenchRamp(ClosedLoop *c, ClosedLoopSample *s, ClosedLoopOutput *out)
{
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, .20f, CL_IMU_AXES_VERIFIED, 0U};
    ClosedLoop_Init(c, cfg); ZeroTrims(c); Aircraft1Pwm_Clear(); mask = 0; tick = 1000;
    for (unsigned i = 0U; i < 600U; ++i) {
        tick += 6;
        AttitudeCheck_Step(c, s, i ? HOVER_NONE : HOVER_START, .006f, out);
        assert(out->state == CL_HOLD && out->fault == CL_OK);
        for (unsigned k = 0U; k < 4U; ++k)
            assert(isfinite(out->motor[k]) && out->motor[k] >= 0 && out->motor[k] <= .20f);
        assert(Aircraft1Pwm_Apply(out->motor, tick)); HAL_SYSTICK_Callback();
        assert(!Aircraft1Pwm_Tripped());
    }
    assert(!s->tof_present && !s->range_valid && isnan(s->range_m));
    assert(fabsf(c->collective - .15f) < .0001f);
    assert(TIM3->CCR1 == 30 && TIM3->CCR2 == 30 && TIM3->CCR3 == 30 && TIM4->CCR1 == 30);
}
static void AttitudeToPwm(void)
{
    assert(CL_HOVER_DUTY == .15f && CL_MAX_DUTY == .20f && CL_IMU_AXES_VERIFIED);
    ClosedLoop c; ClosedLoopOutput out; ClosedLoopSample s = BenchSample();
    ClosedLoopConfig cfg = {.15f, .20f, .20f, 1U, 0U};
    ClosedLoop_Init(&c, cfg); Aircraft1Pwm_Clear();
    ZeroTrims(&c);
    AttitudeCheck_Step(&c, &s, HOVER_NONE, .006f, &out);
    assert(out.state == CL_IDLE && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    /* Tilt error and angular-rate damping must drive the correct physical pair. */
    for (unsigned n = 0U; n < 8U; ++n) {
        s = BenchSample(); BenchRamp(&c, &s, &out);
        if (n < 2U) s.roll = n == 0U ? 10 : -10;
        else if (n < 4U) s.pitch = n == 2U ? -10 : 10;
        else if (n == 4U) s.gyro[0] = 15;
        else if (n == 5U) s.gyro[1] = -15;
        else s.gyro[2] = n == 6U ? 15 : -15;
        tick += 6; AttitudeCheck_Step(&c, &s, HOVER_NONE, .006f, &out);
        assert(Aircraft1Pwm_Apply(out.motor, tick));
        assert(TIM3->CCR1 <= 40 && TIM3->CCR2 <= 40 && TIM3->CCR3 <= 40 && TIM4->CCR1 <= 40);
        if (n == 0U || n == 4U) {
            assert(TIM3->CCR1 > TIM3->CCR2 && TIM3->CCR3 > TIM4->CCR1);
        } else if (n == 1U) {
            assert(TIM3->CCR1 < TIM3->CCR2 && TIM3->CCR3 < TIM4->CCR1);
        } else if (n == 2U || n == 5U) {
            assert(TIM3->CCR1 > TIM3->CCR3 && TIM3->CCR2 > TIM4->CCR1);
        } else if (n == 3U) {
            assert(TIM3->CCR1 < TIM3->CCR3 && TIM3->CCR2 < TIM4->CCR1);
        } else if (n == 6U) {
            assert(TIM3->CCR1 > TIM3->CCR2 && TIM4->CCR1 > TIM3->CCR3);
        } else {
            assert(TIM3->CCR1 < TIM3->CCR2 && TIM4->CCR1 < TIM3->CCR3);
        }
    }
    /* Missing IMU/link, timing, excessive tilt, trip and NaN latch zero output. */
    const ClosedLoopFault faults[] = {CL_IMU_INVALID,CL_LINK_LOST,CL_TIMING,
        CL_TILT,CL_OUTPUT_FAULT,CL_IMU_INVALID};
    for (unsigned n = 0U; n < sizeof(faults)/sizeof(faults[0]); ++n) {
        s = BenchSample(); BenchRamp(&c, &s, &out); float dt = .006f;
        if (n == 0U) s.imu_valid = 0;
        else if (n == 1U) s.link_valid = 0;
        else if (n == 2U) dt = .020f;
        else if (n == 3U) s.roll = 25;
        else if (n == 4U) s.output_fault = 1;
        else s.gyro[1] = NAN;
        AttitudeCheck_Step(&c, &s, HOVER_NONE, dt, &out);
        assert(out.state == CL_FAULT && out.fault == faults[n]);
        assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
        s = BenchSample(); AttitudeCheck_Step(&c, &s, HOVER_START, .006f, &out);
        assert(out.state == CL_FAULT && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
        AttitudeCheck_Step(&c, &s, HOVER_STOP, NAN, &out);
        assert(out.state == CL_IDLE && out.fault == CL_OK);
        assert(Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    }
    s = BenchSample(); BenchRamp(&c, &s, &out);
    AttitudeCheck_Step(&c, &s, HOVER_REQUEST_LAND, NAN, &out);
    assert(out.state == CL_IDLE && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    AttitudeCheck_Step(&c, &s, HOVER_NONE, .006f, &out);
    assert(out.state == CL_IDLE && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    s.roll = 9; AttitudeCheck_Step(&c, &s, HOVER_START, .006f, &out);
    assert(out.fault == CL_START_TOO_HIGH && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    ClosedLoop_Init(&c, cfg); ZeroTrims(&c); s = BenchSample(); c.config.max_duty = .90f;
    AttitudeCheck_Step(&c, &s, HOVER_START, .006f, &out);
    assert(out.fault == CL_INPUT_INVALID && Aircraft1Pwm_Apply(out.motor, tick)); Zero();
    puts("Attitude bench -> PWM: 15% base/20% ceiling, 8 tilt/rate directions, absent ToF beyond2s, stops/fault latch passed (no hardware)");
}
#endif
int main(void)
{
#ifdef CL_TETHERED_DEBUG_OUTPUT
    assert(CL_FLIGHT_OUTPUT_LOCKED == 0U);
    assert(CL_MAX_DUTY == 1.0f);
#endif
#ifdef FLOW_ID_PROBE
    float demand[4] = {0};
    Aircraft1Pwm_Init(); Zero();
    assert(Aircraft1Pwm_Apply(demand, 1));
    for (unsigned k=0;k<4;++k) {
        Aircraft1Pwm_Clear(); demand[k]=.000001f;
        assert(!Aircraft1Pwm_Apply(demand, 2));
        assert(Aircraft1Pwm_Tripped()); Zero(); demand[k]=0;
    }
    Aircraft1Pwm_Clear(); demand[0]=NAN;
    assert(!Aircraft1Pwm_Apply(demand, 3)); Zero();
    demand[0]=0; Aircraft1Pwm_Clear();
    assert(Aircraft1Pwm_Apply(demand, 4)); Zero();
    puts("Motor-locked diagnostic: actual PWM driver rejects every nonzero channel and NaN, accepts zero (register doubles)");
#else
    float m[4] = {AIRCRAFT1_PWM_MAX_DUTY * .50f, AIRCRAFT1_PWM_MAX_DUTY * .75f,
                  AIRCRAFT1_PWM_MAX_DUTY, AIRCRAFT1_PWM_MAX_DUTY * .25f};
    assert(!Aircraft1Pwm_Apply(m, 0));
    Aircraft1Pwm_Init(); Zero();
    assert(c_pins == (GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8));
    assert(b_pins == GPIO_PIN_6);
    assert(TIM3->PSC == 23 && TIM4->PSC == 23 && TIM3->ARR == 199 && TIM4->ARR == 199);
    assert(TIM3->CCER == (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E));
    assert(TIM4->CCER == TIM_CCER_CC1E);
    float full[4] = {AIRCRAFT1_PWM_MAX_DUTY, AIRCRAFT1_PWM_MAX_DUTY,
                     AIRCRAFT1_PWM_MAX_DUTY, AIRCRAFT1_PWM_MAX_DUTY};
    assert(Aircraft1Pwm_Apply(full, 90));
    unsigned full_ticks = (unsigned)(AIRCRAFT1_PWM_MAX_DUTY * 200.0f);
    assert(TIM3->CCR1 == full_ticks && TIM3->CCR2 == full_ticks &&
           TIM3->CCR3 == full_ticks && TIM4->CCR1 == full_ticks);
    assert(Aircraft1Pwm_Apply(m, 100));
    assert(TIM3->CCR1 == (unsigned)(m[0] * 200.0f) && TIM3->CCR2 == (unsigned)(m[1] * 200.0f) &&
           TIM3->CCR3 == (unsigned)(m[2] * 200.0f) && TIM4->CCR1 == (unsigned)(m[3] * 200.0f));
    /* 看门狗阈值 2026-10-28 由 20ms 放宽到 50ms（实飞偶发 I2C 超时误停机）。 */
    tick = 149; HAL_SYSTICK_Callback(); assert(!Aircraft1Pwm_Tripped());
    tick = 150; HAL_SYSTICK_Callback(); assert(Aircraft1Pwm_Tripped()); Zero();
    assert(!Aircraft1Pwm_Apply(m, 151)); Zero();
    Aircraft1Pwm_Clear(); assert(Aircraft1Pwm_Apply(m, 130));
    pressed = GPIO_PIN_12; tick = 131; HAL_SYSTICK_Callback();
#ifdef AIRCRAFT1_STANDALONE
    assert(!Aircraft1Pwm_Tripped()); /* KEY1 starts/lands in local mode. */
#else
    assert(Aircraft1Pwm_Tripped()); Zero();
#endif
    pressed = 0; Aircraft1Pwm_Clear(); assert(Aircraft1Pwm_Apply(m, 140));
    pressed = GPIO_PIN_13; tick = 141; HAL_SYSTICK_Callback();
    assert(Aircraft1Pwm_Tripped()); Zero(); pressed = 0;
    Aircraft1Pwm_Clear(); m[3] = NAN; assert(!Aircraft1Pwm_Apply(m, 150)); Zero();
    Aircraft1Pwm_Clear(); m[3] = AIRCRAFT1_PWM_MAX_DUTY + .01f;
    assert(!Aircraft1Pwm_Apply(m, 150)); Zero();
    Aircraft1Pwm_Clear(); m[3] = -.01f; assert(!Aircraft1Pwm_Apply(m, 150)); Zero();
    Aircraft1Pwm_Clear(); m[3] = AIRCRAFT1_PWM_MAX_DUTY * .25f;
    assert(Aircraft1Pwm_Apply(m, UINT32_MAX - 10U));
    tick = 39U; HAL_SYSTICK_Callback(); assert(Aircraft1Pwm_Tripped()); Zero(); /* 回绕 50ms */
    Aircraft1Pwm_Clear(); mask = 1; assert(Aircraft1Pwm_Apply(m, 10)); assert(mask == 1);
    Aircraft1Pwm_Clear(); assert(mask == 1); Zero();
#ifdef TEST_CLOSED_LOOP_PWM
    ControllerToPwm();
#endif
#ifdef TEST_DIRECTION_CHECK
    DirectionToPwm();
#endif
#ifdef TEST_ATTITUDE_CHECK
    AttitudeToPwm();
#endif
    puts("PWM register-double tests passed: selected pins, configured ceiling, zero-before-AF, watchdog, keys, trip latch and wrap (no hardware)");
#endif
    return 0;
}
