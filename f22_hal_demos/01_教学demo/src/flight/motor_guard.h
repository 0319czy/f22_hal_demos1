#ifndef F22_MOTOR_GUARD_H
#define F22_MOTOR_GUARD_H
#include <stdint.h>
typedef enum { MOTOR_WAIT_RELEASE, MOTOR_IDLE, MOTOR_HOLD, MOTOR_RUNNING } MotorGuardState;
typedef struct { MotorGuardState state; uint32_t since_ms; } MotorGuard;
void MotorGuard_Init(MotorGuard *guard);
/* Released boot keys, 2 s hold, max 3 s run; release or KEY2 stops immediately. */
uint8_t MotorGuard_Update(MotorGuard *guard, uint8_t key1, uint8_t key2, uint32_t now_ms);
#endif
