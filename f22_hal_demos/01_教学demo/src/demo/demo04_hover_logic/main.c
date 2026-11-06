#include "../lesson_support.h"
#include "hover_sim.h"

int main(void)
{
    HoverController controller;
    HoverSample sample;
    HoverOutput output;
    uint8_t previous_key;
    uint32_t previous_frame;
    Lesson_Init(); Hover_Init(&controller); HoverSim_Init(&sample);
    previous_key = Lesson_Key1(); previous_frame = HAL_GetTick();
    while (1) {
        uint32_t started = HAL_GetTick();
        uint8_t key = Lesson_Key1();
        float dt = (float)(uint32_t)(started - previous_frame) * 0.001f;
        if (dt == 0.0f) dt = 0.02f;
        HoverCommand command = Lesson_Key2() ? HOVER_STOP :
            (key && !previous_key ?
                (controller.state == HOVER_IDLE ? HOVER_START : HOVER_REQUEST_LAND) : HOVER_NONE);
        previous_key = key; previous_frame = started;
        Hover_Step(&controller, &sample, command, dt, &output);
        /* All values here are simulated. No timer/PWM/motor driver is called. */
        float ch[] = {(float)output.state, sample.height_m, output.height_target_m,
            sample.vertical_speed_mps, sample.roll_deg, sample.pitch_deg,
            sample.x_m, sample.y_m, sample.vx_mps, sample.vy_mps,
            output.collective, output.correction[0], output.correction[1], output.correction[2],
            output.virtual_motor[0], output.virtual_motor[1], output.virtual_motor[2],
            output.virtual_motor[3], 0.0f, 1.0f};
        (void)Lesson_SendFloats(ch, 20U);
        HoverSim_Step(&sample, &output, dt);
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
            output.state == HOVER_FAULT ? BOARD_LED1_ON_LEVEL : BOARD_LED1_OFF_LEVEL);
        uint32_t elapsed = HAL_GetTick() - started;
        if (elapsed < 20U) HAL_Delay(20U - elapsed);
    }
}
