#include "closed_loop.h"
#include "closed_loop_profile.h"
#include "aircraft1_pwm.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
TIM_TypeDef fake_tim3, fake_tim4;
GPIO_TypeDef fake_gpio_a, fake_gpio_b, fake_gpio_c;
static uint32_t irq_mask;
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1; }
void __set_PRIMASK(uint32_t value) { irq_mask = value; }
uint32_t HAL_GetTick(void) { return 1000; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{ (void)port; (void)pin; return GPIO_PIN_SET; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio)
{ (void)port; (void)gpio; }
static void Zero(void)
{ assert(TIM3->CCR1 == 0 && TIM3->CCR2 == 0 && TIM3->CCR3 == 0 && TIM4->CCR1 == 0); }
int main(void)
{
    assert(CL_FLIGHT_OUTPUT_LOCKED);
    ClosedLoopConfig cfg = {CL_HOVER_DUTY,CL_MAX_DUTY,.20f,1,0};
    ClosedLoopSample sample = {.imu_valid=1,.link_valid=1,.tof_present=1,.tof_fresh=1};
    ClosedLoop c; ClosedLoopOutput out;
    Aircraft1Pwm_Init();
    for (unsigned state = CL_IDLE; state <= CL_HOLD; ++state) {
        ClosedLoop_Init(&c,cfg); c.state = (ClosedLoopState)state; c.collective = 1;
        ClosedLoop_Step(&c,&sample,HOVER_START,.005f,&out);
        assert(c.state == CL_FAULT && c.fault == CL_FLIGHT_INHIBITED);
        for (unsigned k = 0; k < 4; ++k) assert(out.motor[k] == 0);
        assert(Aircraft1Pwm_Apply(out.motor,1000)); Zero();
        ClosedLoop_Step(&c,&sample,HOVER_STOP,.005f,&out);
        assert(c.state == CL_IDLE); Zero();
    }
    ClosedLoop_Init(&c,cfg); c.state = CL_LAND; c.collective = 1;
    ClosedLoop_Step(&c,&sample,HOVER_NONE,.005f,&out);
    assert(c.fault == CL_FLIGHT_INHIBITED);
    for (unsigned k = 0; k < 4; ++k) assert(out.motor[k] == 0);
    /* Driver lock is independent: even bypassed controller/corrupt caller
     * cannot output one motor or a below-one-timer-tick positive demand. */
    for (unsigned k = 0; k < 4; ++k) {
        float motor[4] = {0}; motor[k] = .001f;
        Aircraft1Pwm_Clear(); assert(!Aircraft1Pwm_Apply(motor,1000)); Zero();
        assert(Aircraft1Pwm_Tripped());
    }
    float bad[4] = {NAN,0,0,0};
    Aircraft1Pwm_Clear(); assert(!Aircraft1Pwm_Apply(bad,1000)); Zero();
    float zero[4] = {0}; Aircraft1Pwm_Clear(); assert(Aircraft1Pwm_Apply(zero,1000)); Zero();
    puts("Incident quarantine: START/restored-active state inhibited; independent PWM rejects every positive channel/NaN; stop/zero work PASS (register doubles)");
}
