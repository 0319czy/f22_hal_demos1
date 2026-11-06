#include "motor_guard.h"
void MotorGuard_Init(MotorGuard *guard)
{
    guard->state = MOTOR_WAIT_RELEASE;
    guard->since_ms = 0U;
}
uint8_t MotorGuard_Update(MotorGuard *guard, uint8_t key1, uint8_t key2, uint32_t now_ms)
{
    if (key2) {
        guard->state = MOTOR_WAIT_RELEASE;
        return 0U;
    }
    switch (guard->state) {
    case MOTOR_WAIT_RELEASE:
        if (!key1) guard->state = MOTOR_IDLE;
        break;
    case MOTOR_IDLE:
        if (key1) { guard->state = MOTOR_HOLD; guard->since_ms = now_ms; }
        break;
    case MOTOR_HOLD:
        if (!key1) guard->state = MOTOR_IDLE;
        else if ((uint32_t)(now_ms - guard->since_ms) >= 2000U) {
            guard->state = MOTOR_RUNNING;
            guard->since_ms = now_ms;
        }
        break;
    case MOTOR_RUNNING:
        if (!key1 || (uint32_t)(now_ms - guard->since_ms) >= 3000U)
            guard->state = MOTOR_WAIT_RELEASE;
        break;
    default: guard->state = MOTOR_WAIT_RELEASE; break;
    }
    return guard->state == MOTOR_RUNNING ? 15U : 0U;
}
