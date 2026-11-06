/* Real sensor/radio entry. Default and preview profiles have no motor output.
 * AIRCRAFT1_PHYSICAL selects the separately tested physical bench controller. */
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include "i2c.h"
#include "sensors.h"
#include "f22_sensor_bus.h"
#include "tof.h"
#include "flow.h"
#ifdef FLOW_CAL_CAPTURE
#ifndef FLOW_ID_PROBE
#error "Calibration capture requires the motor-locked diagnostic"
#endif
#include "bench/flow_capture.h"
static FlowCapture flow_capture;
static unsigned flow_capture_i2c_boot;
#endif
static unsigned i2c2_boot_clear;
#include "observation.h"
#include "imu_calibration.h"
#include "hover.h"
#ifdef CLOSED_LOOP_RADIO_V2
#include "closed_loop_radio.h"
#include "bench/radio_fast_config.h"
typedef ClosedLoopRadio TaskInput;
#define TaskInput_Init ClosedLoopRadio_Init
#define TaskInput_Feed ClosedLoopRadio_Feed
#define TaskInput_Take ClosedLoopRadio_Take
#else
#include "task_input.h"
#endif
#include "config.h"
#ifdef AIRCRAFT1_INTEGRATION
#include "aircraft1.h"
#include "aircraft1_config.h"
#include <stdio.h>
#endif
#ifdef AIRCRAFT1_PHYSICAL
#include "closed_loop.h"
#include "closed_loop_config.h"
#include "flow_runtime.h"
#include "aircraft1_pwm.h"
#ifdef CL_DIRECTION_CHECK
#include "bench/motor_direction.h"
#define PhysicalControl_Step MotorDirection_Step
#elif defined(CL_ATTITUDE_CHECK)
#include "bench/attitude_check.h"
#define PhysicalControl_Step AttitudeCheck_Step
#else
#define PhysicalControl_Step ClosedLoop_Step
#endif
#endif
#ifdef AIRCRAFT1_STANDALONE
#include "standalone_keys.h"
static StandaloneKeys local_keys;
#define CL_LOG_UART (&huart1)
#define CL_LOG_PREFIX "[CL LOCAL] "
#elif defined(CL_LOG_JP13)
#define CL_LOG_UART (&huart2)
#define CL_LOG_PREFIX "[CL] "
#else
#define CL_LOG_UART (&huart4)
#define CL_LOG_PREFIX "[CL] "
#endif
#include <math.h>
#include <string.h>

#ifndef AIRCRAFT1_STANDALONE
static unsigned InputLink(const TaskInput *input, uint32_t now)
{
#ifdef CLOSED_LOOP_RADIO_V2
    return ClosedLoopRadio_LinkValid(input, now);
#else
    return Radio_LinkValid(&input->radio, now);
#endif
}
#endif

#define RX_QUEUE_SIZE 256U
static volatile uint8_t rx_queue[RX_QUEUE_SIZE], rx_head, rx_tail, rx_error;
static volatile uint32_t rx_times[RX_QUEUE_SIZE];
#ifndef AIRCRAFT1_STANDALONE
static uint8_t rx_byte;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uint8_t next = (uint8_t)(rx_head + 1U);
    if (next == rx_tail) {
        rx_error = 1U;
#ifdef AIRCRAFT1_PHYSICAL
        Aircraft1Pwm_Trip();
#endif
    }
    else { rx_queue[rx_head] = rx_byte; rx_times[rx_head] = HAL_GetTick(); rx_head = next; }
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) {
        rx_error = 1U;
#ifdef AIRCRAFT1_PHYSICAL
        Aircraft1Pwm_Trip();
#endif
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == &huart1) {
        rx_error = 1U;
#ifdef AIRCRAFT1_PHYSICAL
        Aircraft1Pwm_Trip();
#endif
    }
}
#endif
static void ReceiveReset(TaskInput *input)
{
#ifdef AIRCRAFT1_STANDALONE
    TaskInput_Init(input); StandaloneKeys_Init(&local_keys);
#else
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    (void)HAL_UART_AbortReceive(&huart1);
#ifdef CLOSED_LOOP_RADIO_V2
    /* Startup/calibration deliberately discard old DR/ORE along with the ring. */
    __HAL_UART_CLEAR_OREFLAG(&huart1);
#endif
    rx_head = rx_tail = rx_error = 0U; TaskInput_Init(input);
    if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) rx_error = 1U;
    __set_PRIMASK(mask);
#endif
}
static uint8_t LocalStop(void)
{
#ifdef AIRCRAFT1_STANDALONE
    return HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET;
#else
    return HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET ||
           HAL_GPIO_ReadPin(BOARD_KEY2_GPIO_Port, BOARD_KEY2_Pin) == GPIO_PIN_RESET;
#endif
}
#ifdef AIRCRAFT1_INTEGRATION
static void MotorPinsOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_8;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOA, &gpio);
}
/* UART4 interrupt-driven text: avoid stalling the attitude loop on telemetry.
 * USART1 remains RX only, because USB CH340 shares it with E49. */
static char telemetry[1024];
static volatile unsigned telemetry_busy;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{ if (uart == CL_LOG_UART) telemetry_busy = 0U; }
#ifdef CL_LOG_JP13
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
#else
void UART4_IRQHandler(void) { HAL_UART_IRQHandler(&huart4); }
#endif
#ifdef CLOSED_LOOP_RADIO_V2
static char key_notice[8][96];
static unsigned key_head, key_tail, key_log_dropped;
static void RecordKey(const TaskInput *input)
{
    unsigned next = (key_head + 1U) % 8U;
    if (next == key_tail) { key_log_dropped++; return; }
    unsigned key = input->event_command == RADIO_CALIBRATE ? 1U :
                   input->event_command == RADIO_START ? 2U :
                   input->event_command == RADIO_LAND ? 3U : 4U;
    (void)snprintf(key_notice[key_head], sizeof(key_notice[0]),
                  "KEY F%u event=%u seq=%u allowed=%u ready=%u\r\n",
                  key, input->event_id, input->sequence, input->event_allowed, input->ready);
    key_head = next;
}
static void SendKey(void)
{
    if (telemetry_busy || key_tail == key_head) return;
    size_t n = strlen(key_notice[key_tail]);
    memcpy(telemetry, key_notice[key_tail], n + 1U); telemetry_busy = 1U;
    if (HAL_UART_Transmit_IT(CL_LOG_UART, (uint8_t *)telemetry, (uint16_t)n) == HAL_OK)
        key_tail = (key_tail + 1U) % 8U;
    else telemetry_busy = 0U;
}
#endif
#ifndef AIRCRAFT1_PHYSICAL
static void SendAircraft1(const HoverOutput *out, const HoverSample *sample,
                          const TofSensor *tof, const FlowSensor *flow,
                          const TaskInput *input, unsigned raw_imu_valid,
                          unsigned wait, uint32_t step_ms, unsigned errors)
{
    if (telemetry_busy) return;
    float board[4]; Aircraft1_MapMotors(out->virtual_motor, board);
    int n = snprintf(telemetry, sizeof(telemetry),
        "[A1] state=%u wait=0x%02X dt_ms=%lu link=%u packets=%u err=%u "
        "imu=%u mount=%u roll_cd=%ld pitch_cd=%ld "
        "tof=%u mm=%u height=%u flow=%u squal=%u dx=%d dy=%d xy=%u axes=%u "
        "demand_permille_M1_M2_M3_M6=%u,%u,%u,%u PWM=0\r\n",
        (unsigned)out->state, wait, (unsigned long)step_ms,
        sample->command_link_valid, input->radio.accepted, errors, raw_imu_valid,
        IMU_MOUNT_VERIFIED, (long)(sample->roll_deg * 100.0f), (long)(sample->pitch_deg * 100.0f),
        (unsigned)tof->present, (unsigned)tof->distance_mm, sample->height_valid,
        (unsigned)flow->valid, (unsigned)flow->quality, (int)flow->dx, (int)flow->dy,
        sample->position_valid, FLOW_CALIBRATED,
        (unsigned)(board[0] * 1000.0f), (unsigned)(board[1] * 1000.0f),
        (unsigned)(board[2] * 1000.0f), (unsigned)(board[3] * 1000.0f));
    if (n <= 0 || (unsigned)n >= sizeof(telemetry)) return;
    telemetry_busy = 1U;
    if (HAL_UART_Transmit_IT(&huart4, (uint8_t *)telemetry, (uint16_t)n) != HAL_OK)
        telemetry_busy = 0U;
}
#else
static char fault_snapshot[512];
static unsigned fault_pending;
static long DiagnosticScaled(float value, float scale)
{
    float v = value * scale;
    if (!isfinite(v) || v <= -2147483648.0f || v >= 2147483648.0f) return -2147483647L;
    return (long)v;
}
static void RecordFault(ClosedLoopState before, const ClosedLoopOutput *out,
                        const ClosedLoopSample *s, const TofSensor *tof,
                        unsigned raw_imu_valid, unsigned calibrated, unsigned arm_level,
                        uint32_t settled_ms, const float acc[3], float dt)
{
    int n = snprintf(fault_snapshot, sizeof(fault_snapshot),
        "[FAULT] code=%u from=%u t_ms=%lu imu_raw=%u imu_used=%u calibrated=%u settled_ms=%lu "
        "armlevel=%u acc_mg=%ld,%ld,%ld gyro_cdps=%ld,%ld,%ld "
        "roll_cd=%ld pitch_cd=%ld height_um=%ld vz_mmps=%ld height_valid=%u "
        "tof_mm=%u status=%u fresh=%u age_ms=%lu output_trip=%u dt_us=%ld\r\n",
        (unsigned)out->fault, (unsigned)before, (unsigned long)HAL_GetTick(),
        raw_imu_valid, s->imu_valid, calibrated, (unsigned long)settled_ms, arm_level,
        DiagnosticScaled(acc[0],1000), DiagnosticScaled(acc[1],1000), DiagnosticScaled(acc[2],1000),
        DiagnosticScaled(s->gyro[0],100), DiagnosticScaled(s->gyro[1],100), DiagnosticScaled(s->gyro[2],100),
        DiagnosticScaled(s->roll,100), DiagnosticScaled(s->pitch,100),
        DiagnosticScaled(s->range_m,1000000), DiagnosticScaled(s->vz,1000), s->range_valid,
        (unsigned)tof->distance_mm, (unsigned)tof->raw_status, s->tof_fresh,
        (unsigned long)Tof_AgeMs(tof), s->output_fault, DiagnosticScaled(dt,1000000));
    fault_pending = n > 0 && (unsigned)n < sizeof(fault_snapshot);
}
static void SendFault(void)
{
    if (!fault_pending || telemetry_busy) return;
    size_t n = strlen(fault_snapshot);
    memcpy(telemetry, fault_snapshot, n+1U); telemetry_busy = 1U;
    if (HAL_UART_Transmit_IT(CL_LOG_UART, (uint8_t *)telemetry, (uint16_t)n) == HAL_OK)
        fault_pending = 0U;
    else telemetry_busy = 0U;
}
static void SendClosedLoop(const ClosedLoopOutput *out, const Attitude *attitude,
                           const HeightEstimate *height,
                           const TofSensor *tof, const FlowSensor *flow,
                           const TaskInput *input, uint32_t step_ms, unsigned arm_level,
                           unsigned calibrated)
{
#ifdef AIRCRAFT1_STANDALONE
    (void)input;
#endif
    if (telemetry_busy || fault_pending) return;
#ifdef CLOSED_LOOP_RADIO_V2
    if (key_tail != key_head) return;
#endif
#ifdef FLOW_CAL_CAPTURE
    /* Use compact calibration telemetry instead of the flight display.
     * Body-axis candidate from operator: forward=dy, right=dx.
     * hx/hy and hangle scaled here only for integer UART formatting. */
    int n = snprintf(telemetry, sizeof(telemetry),
        "[FCAL] ms=%lu epoch=%lu polls=%lu raw_n=%lu raw_dx=%ld raw_dy=%ld n=%lu bad=%lu pollerr=%lu why=%u sat=%u "
        "dxsum=%ld dysum=%ld hx_mcount_milli=%ld hy_mcount_milli=%ld "
        "angle_urad=%ld,%ld,%ld hangle_umrad=%ld,%ld,%ld time_ms=%ld "
        "imu=%u tof=%u valid=%u mm=%u roll_cd=%ld pitch_cd=%ld flow=%u id=%u inv=%u q=%u "
        "i2c_boot=%u PWM=%u,%u,%u,%u output_locked=1\r\n",
        (unsigned long)HAL_GetTick(), (unsigned long)flow_capture.epoch,
        (unsigned long)flow_capture.polls, (unsigned long)flow_capture.raw_samples,
        (long)flow_capture.raw_dx, (long)flow_capture.raw_dy, (unsigned long)flow_capture.accepted,
        (unsigned long)flow_capture.rejected, (unsigned long)flow_capture.io_errors,
        flow_capture.invalid_reasons, flow_capture.saturated,
        (long)flow_capture.dx, (long)flow_capture.dy,
        DiagnosticScaled(flow_capture.hx, 1000), DiagnosticScaled(flow_capture.hy, 1000),
        DiagnosticScaled(flow_capture.angle[0], 1000000), DiagnosticScaled(flow_capture.angle[1], 1000000),
        DiagnosticScaled(flow_capture.angle[2], 1000000),
        DiagnosticScaled(flow_capture.hangle[0], 1000000), DiagnosticScaled(flow_capture.hangle[1], 1000000),
        DiagnosticScaled(flow_capture.hangle[2], 1000000), DiagnosticScaled(flow_capture.seconds, 1000),
        attitude->valid, (unsigned)tof->present, (unsigned)tof->valid, (unsigned)tof->distance_mm,
        DiagnosticScaled(attitude->roll, 100), DiagnosticScaled(attitude->pitch, 100),
        flow->valid, (unsigned)flow->id, (unsigned)flow->inverse_id, (unsigned)flow->quality,
        flow_capture_i2c_boot,
        (unsigned)(TIM3->CCR1*5U), (unsigned)(TIM3->CCR2*5U),
        (unsigned)(TIM3->CCR3*5U), (unsigned)(TIM4->CCR1*5U));
    (void)out; (void)input; (void)step_ms; (void)arm_level; (void)calibrated;
#else
    int n = snprintf(telemetry, sizeof(telemetry),
        CL_LOG_PREFIX "state=%u fault=%u dt_ms=%lu link=%u packets=%u imu=%u roll_cd=%ld pitch_cd=%ld "
        "height_mm=%ld vz_mmps=%ld height_valid=%u tof_innovation_rejects=%lu "
        "tof=%u valid=%u mm=%u flow=%u flow_present=%u squal=%u flow_err=%u "
        "dx=%d dy=%d axes=%u armlevel=%u calibrated=%u "
        "flow_id=%u flow_inv=%u flow_fault=%u flow_reg=%u flow_io=%d "
        "flow_reads=%u flow_attempts=%u flow_init=%u "
#ifdef FLOW_ID_PROBE
        "flow_probe=%u flow_rev=%u flow_cfg=%u,%u output_locked=1 "
#ifdef FLOW_SPI_DUPLEX_PROBE
        "spi_transport=duplex "
#else
        "spi_transport=split "
#endif
#endif
        "PWM_permille_M1_M2_M3_M6=%u,%u,%u,%u trip=%u"
#if CL_FLIGHT_OUTPUT_LOCKED
        " output_locked=1 lock_reason=incident"
#endif
#ifdef CL_TETHERED_DEBUG_OUTPUT
        " output_locked=0 debug_output=1"
#endif
#ifdef CL_TETHERED_FLOW_DEBUG_OUTPUT
        " output_locked=0 debug_output=flow_provisional"
#endif
#ifdef CL_ATTITUDE_CHECK
        " bench=attitude20"
#endif
#ifdef CL_TETHERED_FLIGHT
#if defined(CL_TETHERED_PWM100)
        " flight=tether100_noflow u_permille=%ld ref_permille=%ld target_mm=%ld"
#elif defined(CL_FLOW_ASSIST)
        " flight=tether100_flow u_permille=%ld ref_permille=%ld target_mm=%ld"
#else
        " flight=tether90 u_permille=%ld ref_permille=%ld target_mm=%ld"
#endif
#endif
#ifdef CL_FLOW_ASSIST
        " flow_req=%u flow_mode=%u flow_gain=%ld flow_land=%u flow_r_cd=%ld flow_p_cd=%ld"
        " flow_hold=%u flow_err_fwd_mm=%ld flow_err_right_mm=%ld flow_vf_ref_mmps=%ld flow_vr_ref_mmps=%ld"
#if CL_FLOW_PROVISIONAL_CONTROL
        " flow_model=provisional"
#else
        " flow_model=calibrated_only"
#endif
#endif
        " link_lost_land=%u"
#ifdef CLOSED_LOOP_RADIO_V2
        " proto=%u seq=%u ready=%u radio_fault=%u reject=%u crc=%lu timeout=%lu key_drop=%u"
#endif
        "\r\n",
        (unsigned)out->state, (unsigned)out->fault, (unsigned long)step_ms,
#ifdef AIRCRAFT1_STANDALONE
        1U, 0U,
#else
        InputLink(input, HAL_GetTick()), input->radio.accepted,
#endif
        attitude->valid, (long)(attitude->roll * 100), (long)(attitude->pitch * 100),
        DiagnosticScaled(height->height,1000), DiagnosticScaled(height->vz,1000), height->valid,
        (unsigned long)height->tof_innovation_rejects,
        (unsigned)tof->present, (unsigned)tof->valid, (unsigned)tof->distance_mm,
        (unsigned)flow->valid, (unsigned)flow->present, (unsigned)flow->quality, flow->errors,
        (int)flow->dx, (int)flow->dy, CL_FLOW_AXES_VERIFIED, arm_level, calibrated,
        (unsigned)flow->id, (unsigned)flow->inverse_id, (unsigned)flow->fault,
        (unsigned)flow->last_reg, flow->io_status, (unsigned)flow->id_read_mask,
        flow->attempts, flow->initializing,
#ifdef FLOW_ID_PROBE
        flow->probe_only, flow->revision, flow->config_4e, flow->config_5b,
#endif
        (unsigned)(TIM3->CCR1 * 5U), (unsigned)(TIM3->CCR2 * 5U),
        (unsigned)(TIM3->CCR3 * 5U), (unsigned)(TIM4->CCR1 * 5U), Aircraft1Pwm_Tripped()
#ifdef CL_TETHERED_FLIGHT
        , (long)(out->collective * 1000), (long)(out->height_feedforward * 1000),
        (long)(out->target_range * 1000)
#endif
#ifdef CL_FLOW_ASSIST
        , out->flow_requested, out->flow_mode, (long)(out->flow_gain * 1000),
        out->flow_auto_land, (long)(out->flow_roll_target * 100), (long)(out->flow_pitch_target * 100),
        out->flow_position_hold,
        DiagnosticScaled(out->flow_position_error_forward, 1000),
        DiagnosticScaled(out->flow_position_error_right, 1000),
        DiagnosticScaled(out->flow_velocity_target_forward, 1000),
        DiagnosticScaled(out->flow_velocity_target_right, 1000)
#endif
        , out->link_lost_landing
#ifdef CLOSED_LOOP_RADIO_V2
        , input->version, input->sequence, input->ready, input->fault_latched,
        input->radio.rejected + (unsigned)input->trace.rejected,
        (unsigned long)input->trace.crc_failures, (unsigned long)input->trace.timeouts,
        key_log_dropped
#endif
        );
#endif
    if (n <= 0 || (unsigned)n >= sizeof(telemetry)) return;
    telemetry_busy = 1U;
    if (HAL_UART_Transmit_IT(CL_LOG_UART, (uint8_t *)telemetry, (uint16_t)n) != HAL_OK)
        telemetry_busy = 0U;
}
#endif
#endif
static void Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init();
#ifdef AIRCRAFT1_INTEGRATION
#ifndef AIRCRAFT1_PHYSICAL
    _Static_assert(AIRCRAFT1_PHYSICAL_PWM == 0U, "Preview has no accepted physical PWM driver");
#endif
    MotorPinsOff();
#endif
    SystemClock_Config(); MX_GPIO_Init();
    MX_USART1_UART_Init();
#ifdef CL_LOG_JP13
    MX_USART2_UART_Init();
#else
    MX_USART4_UART_Init();
#endif
    i2c2_boot_clear = I2C2_ClearBusAtBoot();
#ifdef FLOW_CAL_CAPTURE
    flow_capture_i2c_boot = i2c2_boot_clear;
#endif
    MX_I2C2_Init();
#ifdef AIRCRAFT1_INTEGRATION
    {
        char bus_line[48];
        int n = snprintf(bus_line, sizeof(bus_line), "[I2C2] boot_bus_clear=%u\r\n",
                         i2c2_boot_clear);
        if (n > 0 && (unsigned)n < sizeof(bus_line))
            (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)bus_line, (uint16_t)n, 20U);
    }
#endif
#ifdef AIRCRAFT1_STANDALONE
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET); /* E49 mode3, no wireless. */
#else
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
#endif
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOA, &gpio);
#ifdef CLOSED_LOOP_RADIO_V2
    /* Match the already verified sender: UART9600, RF50k; C2/readback. */
    RadioFastConfig radio_config; RadioFast_Configure(&radio_config);
    char line[96];
    int n = snprintf(line, sizeof(line),
        "BOOT CL_V2 JP13=115200 radio=%lu config=%u motors=OFF\r\n",
        (unsigned long)huart1.Init.BaudRate, radio_config.applied);
    if (n > 0 && (unsigned)n < sizeof(line))
        (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)line, (uint16_t)n, 30U);
#endif
    F22_ModuleBusInit();
    HAL_NVIC_SetPriority(USART1_IRQn, 2U, 0U); HAL_NVIC_EnableIRQ(USART1_IRQn);
#ifdef AIRCRAFT1_INTEGRATION
#ifdef CL_LOG_JP13
    HAL_NVIC_SetPriority(USART2_IRQn, 3U, 0U); HAL_NVIC_EnableIRQ(USART2_IRQn);
#else
    HAL_NVIC_SetPriority(UART4_IRQn, 3U, 0U); HAL_NVIC_EnableIRQ(UART4_IRQn);
#endif
#endif
#ifdef AIRCRAFT1_PHYSICAL
    Aircraft1Pwm_Init();
#endif
}
#ifndef AIRCRAFT1_INTEGRATION
static void SendFloats(const float values[32])
{
    uint8_t frame[132]; const uint8_t tail[4] = {0,0,0x80,0x7f};
    _Static_assert(sizeof(float) == 4U, "JustFloat float32 required");
    memcpy(frame, values, 128U); memcpy(frame + 128U, tail, 4U);
    (void)HAL_UART_Transmit(&huart4, frame, sizeof(frame), 20U);
}
#endif
/* Flow-only recovery performs at most one SPI operation per iteration. It has
 * no radio/IMU/height reset and never performs the boot-time blocking delays. */
static void ServiceFlow(FlowSensor *flow, unsigned inactive, uint32_t now, uint32_t *retry)
{
    if (!inactive) return;
    if (!flow->present && !flow->probe_only && !flow->initializing && (uint32_t)(now - *retry) >= 2000U) {
        *retry = now; Flow_BeginRetry(flow);
    }
    if (flow->initializing) (void)Flow_InitStep(flow);
}
/* A rejected F1 window must not erase an earlier accepted bias. Bus failures
 * still invalidate it, and all active-flight/start guards remain in force. */
static unsigned calibration_io_failed;
#ifdef AIRCRAFT1_INTEGRATION
static char calibration_notice[384];
static unsigned calibration_pending;
static void SendCalibration(void)
{
    if (!calibration_pending || telemetry_busy) return;
    size_t n = strlen(calibration_notice);
    memcpy(telemetry, calibration_notice, n + 1U); telemetry_busy = 1U;
    if (HAL_UART_Transmit_IT(CL_LOG_UART, (uint8_t *)telemetry, (uint16_t)n) == HAL_OK)
        calibration_pending = 0U;
    else telemetry_busy = 0U;
}
#endif
static unsigned Calibrate(float bias[3], unsigned already_calibrated)
{
    ImuSample sample; ImuCalibration window; uint8_t id = 0;
    const char *reason = "ok"; unsigned ok = 0U, axis = 6U, reused = 0U;
    ImuCalibrationReason verdict = IMU_CAL_INCOMPLETE;
    HAL_StatusTypeDef rc; calibration_io_failed = 0U;
    ImuCalibration_Init(&window);
    rc = Sensors_ImuReadId(&id);
    if (rc != HAL_OK || id != 0x68U) {
        reason = rc != HAL_OK ? "id_read" : "id_mismatch";
        calibration_io_failed = 1U; goto finished;
    }
    /* A healthy running IMU needs a new sample window, not reconfiguration. */
    if (!already_calibrated && (rc = Sensors_ImuInit()) != HAL_OK) {
        reason = "init"; calibration_io_failed = 1U; goto finished;
    }
    for (unsigned n = 0; n < IMU_CALIBRATION_SAMPLES; n++) {
        rc = Sensors_ImuRead(&sample);
        if (rc != HAL_OK) { reason = "sample_read"; calibration_io_failed = 1U; goto finished; }
        float gyro[3], acc[3];
        for (unsigned k = 0; k < 3; k++) {
            gyro[k] = sample.gyro[k] / 131.0f;
            acc[k] = sample.accel[k] / 16384.0f;
        }
        if (!ImuCalibration_Add(&window, acc, gyro)) break;
        HAL_Delay(5U);
    }
    {
        static const char *reasons[] = {"ok", "incomplete", "nonfinite", "moving_rate",
                                       "accel_norm", "variance", "spread"};
        verdict = ImuCalibration_Check(&window, &axis);
        reason = reasons[verdict];
        ok = ImuCalibration_Finish(&window, bias);
    }
finished:
    reused = !ok && already_calibrated && !calibration_io_failed &&
             ImuCalibration_CanRetain(verdict);
#ifdef AIRCRAFT1_INTEGRATION
    {
        long sd[6] = {0}, spread[6] = {0};
        if (window.count) for (unsigned i = 0; i < 6; i++) {
            float scale = i < 3 ? 1000.0f : 100.0f;
            float variance = window.m2[i] / window.count;
            sd[i] = isfinite(variance) && variance >= 0 ? (long)(sqrtf(variance)*scale) : -1L;
            spread[i] = (long)((window.maximum[i]-window.minimum[i])*scale);
        }
        int n = snprintf(calibration_notice, sizeof(calibration_notice),
            "[CAL] ok=%u reason=%s samples=%u axis=%u io=%u id=%u calibrated=%u reused=%u "
            "acc_sd_mg=%ld,%ld,%ld gyro_sd_cdps=%ld,%ld,%ld "
            "acc_spread_mg=%ld,%ld,%ld gyro_spread_cdps=%ld,%ld,%ld\r\n",
            ok, reason, window.count, axis, (unsigned)rc, (unsigned)id, ok || reused, reused,
            sd[0],sd[1],sd[2],sd[3],sd[4],sd[5],
            spread[0],spread[1],spread[2],spread[3],spread[4],spread[5]);
        calibration_pending = n > 0 && (unsigned)n < sizeof(calibration_notice);
    }
#else
    (void)reason;
#endif
    return ok || reused;
}
static void Rotate(const float in[3], float out[3])
{
#ifdef AIRCRAFT1_PHYSICAL
    Aircraft1_ImuToBody(in, out);
#else
    const float *mount = imu_mount;
    for (unsigned i = 0; i < 3; i++)
        out[i] = mount[3*i]*in[0] + mount[3*i+1]*in[1] + mount[3*i+2]*in[2];
#endif
}
int main(void)
{
    TofSensor tof; FlowSensor flow; Attitude attitude; HeightEstimate height;
    FlowEstimate position; HoverController controller; TaskInput input;
#ifndef AIRCRAFT1_PHYSICAL
    HoverOutput output;
#endif
    float bias[3] = {0}, gyro[3] = {0}; unsigned errors = 0;
    Init(); unsigned imu_ready = Calibrate(bias, 0U);
#ifdef FLOW_CAL_CAPTURE
    FlowCapture_Init(&flow_capture);
#endif
#ifdef AIRCRAFT1_PHYSICAL
    ClosedLoop closed; ClosedLoopOutput physical;
    ClosedLoopConfig cfg = {CL_HOVER_DUTY, CL_MAX_DUTY, CL_TARGET_RANGE_M,
                           CL_IMU_AXES_VERIFIED, CL_FLOW_AXES_VERIFIED};
#ifdef CL_FLOW_ASSIST
    cfg.use_flow = 1U; /* requested, never silently downgraded to no-flow flight */
#endif
    ClosedLoop_Init(&closed, cfg);
    FlowCalibration physical_flow = flow_calibration;
    physical_flow.calibrated =
        (FLOW_CALIBRATED && CL_FLOW_AXES_VERIFIED) || CL_FLOW_PROVISIONAL_CONTROL;
#endif
    Attitude_Init(&attitude); Height_Reset(&height); FlowEstimate_Reset(&position);
    Hover_Init(&controller); TaskInput_Init(&input);
    (void)Tof_Init(&tof, F22_TofIo()); (void)Flow_Init(&flow, F22_FlowIo());
    ReceiveReset(&input);
#ifdef CL_DIRECTION_CHECK
    static const uint8_t direction_mode[] =
        "\r\n[BOOT] MOTOR_DIRECTION F2=start F3/F4=stop no_auto_ramp\r\n";
    (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)direction_mode, sizeof(direction_mode)-1U, 20U);
#endif
#ifdef CL_ATTITUDE_CHECK
    static const uint8_t attitude_mode[] =
        "\r\n[BOOT] ATTITUDE_CHECK20 F2=start F3/F4=stop base=150 max=200 no_height_control\r\n";
    (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)attitude_mode, sizeof(attitude_mode)-1U, 20U);
#endif
#ifdef CL_TETHERED_FLIGHT
#if CL_FLIGHT_OUTPUT_LOCKED
    static const uint8_t flight_hold[] =
        "\r\n[BOOT] FLIGHT_HOLD reason=airframe_damage motors=LOCKED no_flight_release\r\n";
    (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)flight_hold, sizeof(flight_hold)-1U, 20U);
#endif
    static const uint8_t tether_mode[] =
#if defined(CL_TETHERED_PWM100)
#ifdef CL_TETHERED_DEBUG_OUTPUT
        "\r\n[BOOT] TETHER100_DEBUG_OUTPUT_ACTIVE F2=takeoff F3=land F4=stop launch=1000 max=1000 base_ramp_pm_s=500 trim_limit_pm=50 trim_slew_pm_s=250 timeout=3s target_mm=200 captured_reference output_locked=0\r\n";
#else
        "\r\n[BOOT] TETHER100_NOFLOW F2=takeoff F3=land F4=stop launch=1000 max=1000 base_ramp_pm_s=500 trim_limit_pm=50 trim_slew_pm_s=250 timeout=3s target_mm=200 captured_reference\r\n";
#endif
#elif defined(CL_FLOW_ASSIST)
#ifdef CL_TETHERED_FLOW_DEBUG_OUTPUT
        "\r\n[BOOT] TETHERED_FLOW_DEBUG F2=takeoff F3=land F4=stop IMU_attitude=always ToF=vertical flow=XY_at_80mm model=provisional max=1000 ramp_pm_s=500 timeout=3s output_locked=0\r\n";
#else
        "\r\n[BOOT] TETHER100_FLOW F2=takeoff F3=land F4=stop launch=1000 max=1000 base_ramp_pm_s=500 trim_limit_pm=50 trim_slew_pm_s=250 timeout=3s target_mm=200 captured_reference\r\n";
#endif
#else
        "\r\n[BOOT] TETHER90 F2=takeoff F3=land F4=stop launch=900 max=900 base_ramp_pm_s=500 trim_limit_pm=50 trim_slew_pm_s=250 timeout=3s target_mm=200 captured_reference\r\n";
#endif
    (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)tether_mode, sizeof(tether_mode)-1U, 20U);
#endif
#ifdef CL_FLOW_ASSIST
    static const uint8_t flow_mode[] =
#if CL_FLOW_PROVISIONAL_CONTROL
        "\r\n[BOOT] FLOW_ASSIST IMU_attitude=always ToF=vertical XY_position_hold=flow enter_mm=80 exit_mm=80 provisional_scale=0.002131946_rad_per_count\r\n";
#else
        "\r\n[BOOT] FLOW_ASSIST ground=IMU valid_flow=blend missing=land calibration_required\r\n";
#endif
    (void)HAL_UART_Transmit(CL_LOG_UART, (uint8_t *)flow_mode, sizeof(flow_mode)-1U, 20U);
#endif
#ifdef AIRCRAFT1_STANDALONE
    static const uint8_t boot[] = "\r\n[BOOT] STANDALONE KEY1=start/land KEY2=STOP USB115200 motors=OFF\r\n";
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)boot, sizeof(boot)-1U, 20U);
#endif
    uint32_t previous = HAL_GetTick(), retry = previous, flow_retry = previous;
#ifdef AIRCRAFT1_PHYSICAL
    uint32_t attitude_started = previous;
#endif
#ifdef AIRCRAFT1_INTEGRATION
    uint32_t sensor_previous = previous, logged = previous;
#ifdef AIRCRAFT1_PHYSICAL
    uint32_t flow_id_checked = previous;
#endif
    float gyro_integral[3] = {0};
    unsigned flow_window_started = 0U;
#endif
    while (1) {
#ifdef AIRCRAFT1_INTEGRATION
        SendCalibration();
#endif
        uint32_t now = HAL_GetTick(); float dt = (now - previous) * .001f; previous = now;
        if (dt == 0) dt = .02f;
        if (rx_error) {
#ifdef AIRCRAFT1_PHYSICAL
            Aircraft1Pwm_Trip();
#endif
            ReceiveReset(&input); ++errors;
        }
        while (rx_tail != rx_head) {
            uint8_t byte = rx_queue[rx_tail]; uint32_t arrived = rx_times[rx_tail];
            rx_tail = (uint8_t)(rx_tail + 1U);
#ifdef CLOSED_LOOP_RADIO_V2
            unsigned event_before = input.event_id;
#endif
            TaskInput_Feed(&input, byte, arrived);
#ifdef CLOSED_LOOP_RADIO_V2
            if (event_before != input.event_id) RecordKey(&input);
#endif
        }
        /* The ISR may receive a byte after the loop's initial now was sampled. */
        uint32_t command_now = HAL_GetTick();
#ifdef AIRCRAFT1_STANDALONE
        HoverCommand command = StandaloneKeys_Step(&local_keys,
            HAL_GPIO_ReadPin(BOARD_KEY1_GPIO_Port, BOARD_KEY1_Pin) == GPIO_PIN_RESET,
            LocalStop(), closed.state, command_now);
#else
        HoverCommand command = TaskInput_Take(&input, command_now);
#endif
        if (LocalStop()) command = HOVER_STOP;
#ifdef AIRCRAFT1_PHYSICAL
        if (command == HOVER_STOP) Aircraft1Pwm_Clear();
#endif
        /* Blocking calibration/reconnect are permitted only while not active.
         * Flush queued bytes afterward: an old START cannot become newly fresh. */
#ifdef CLOSED_LOOP_RADIO_V2
        SendKey();
        unsigned recalibrate = ClosedLoopRadio_TakeCalibration(&input,
            closed.state == CL_IDLE && TIM3->CCR1 == 0U && TIM3->CCR2 == 0U &&
            TIM3->CCR3 == 0U && TIM4->CCR1 == 0U, command_now);
#else
        unsigned recalibrate = TaskInput_TakeCalibration(&input, controller.state, command_now);
#endif
        if (recalibrate && command == HOVER_NONE) {
            imu_ready = Calibrate(bias, imu_ready); Attitude_Init(&attitude);
            Height_Reset(&height); FlowEstimate_Reset(&position); ReceiveReset(&input);
            previous = HAL_GetTick();
#ifdef AIRCRAFT1_PHYSICAL
            attitude_started = previous;
#endif
#ifdef AIRCRAFT1_INTEGRATION
            sensor_previous = previous; flow_window_started = 0U;
            memset(gyro_integral, 0, sizeof(gyro_integral));
#ifdef FLOW_CAL_CAPTURE
            FlowCapture_Break(&flow_capture);
#endif
#endif
            continue;
        }
#ifdef CLOSED_LOOP_RADIO_V2
        if ((closed.state == CL_IDLE || closed.state == CL_FAULT) &&
#else
        if ((controller.state == HOVER_IDLE || controller.state == HOVER_FAULT) &&
#endif
            command == HOVER_NONE && (!imu_ready || !tof.present) &&
            (uint32_t)(now - retry) >= 2000U) {
            if (!imu_ready) { imu_ready = Calibrate(bias, 0U); Attitude_Init(&attitude); }
            if (!tof.present) (void)Tof_Init(&tof, F22_TofIo());
            retry = HAL_GetTick(); previous = retry; ReceiveReset(&input);
#ifdef AIRCRAFT1_PHYSICAL
            attitude_started = previous;
#endif
#ifdef AIRCRAFT1_INTEGRATION
            sensor_previous = retry; flow_window_started = 0U;
            Height_Reset(&height); FlowEstimate_Reset(&position);
            memset(gyro_integral, 0, sizeof(gyro_integral));
#ifdef FLOW_CAL_CAPTURE
            FlowCapture_Break(&flow_capture);
#endif
#endif
            continue;
        }
        unsigned flow_was_present = flow.present;
#ifdef AIRCRAFT1_PHYSICAL
        unsigned flow_inactive = (closed.state == CL_IDLE || closed.state == CL_FAULT) &&
            TIM3->CCR1 == 0U && TIM3->CCR2 == 0U && TIM3->CCR3 == 0U && TIM4->CCR1 == 0U;
        /* Invalidate unused flow immediately, even between sensor windows. */
        FlightFlow_BeginCycle(&flow, flow_inactive, command);
#else
        unsigned flow_inactive = controller.state == HOVER_IDLE || controller.state == HOVER_FAULT;
#endif
        ServiceFlow(&flow, flow_inactive && command == HOVER_NONE, now, &flow_retry);
        if (!flow_was_present && flow.present) {
            FlowEstimate_Reset(&position);
#ifdef FLOW_CAL_CAPTURE
            FlowCapture_Break(&flow_capture);
#endif
#ifdef AIRCRAFT1_INTEGRATION
            flow_window_started = 0U; memset(gyro_integral, 0, sizeof(gyro_integral));
#endif
        }
        ImuSample imu; float acc_sensor[3], gyro_sensor[3], acc[3] = {0};
        if (imu_ready && Sensors_ImuRead(&imu) == HAL_OK) {
            for (unsigned k = 0; k < 3; k++) {
                acc_sensor[k] = imu.accel[k] / 16384.0f;
                gyro_sensor[k] = imu.gyro[k] / 131.0f - bias[k];
            }
            Rotate(acc_sensor, acc); Rotate(gyro_sensor, gyro);
            (void)Attitude_Update(&attitude, acc, gyro, dt);
        } else { imu_ready = 0; attitude.valid = 0; }
#ifdef AIRCRAFT1_PHYSICAL
        /* Run the short inertial coast at the fast control rate. Ground launch
         * remains range-blind until the first policy-valid ToF sample because
         * the accelerometer cannot separate prop thrust from ground support. */
        if (attitude.valid && closed.state != CL_IDLE && closed.state != CL_FAULT)
            (void)Height_Predict(&height, &attitude, acc, now);
        else
            Height_Invalidate(&height);
#endif
#ifdef AIRCRAFT1_INTEGRATION
        for (unsigned k = 0U; k < 3U; ++k) gyro_integral[k] += gyro[k] * dt;
        if ((uint32_t)(now - sensor_previous) >= AIRCRAFT1_SENSOR_PERIOD_MS) {
            float flow_dt = (now - sensor_previous) * .001f;
            sensor_previous = now;
            int fresh = Tof_Poll(&tof);
#ifdef AIRCRAFT1_PHYSICAL
            /* SPI has no ACK: 0xFF from a disconnected MISO is not proof of
             * healthy SQUAL. No-flow profiles perform this only at zero output,
             * never in a START/active cycle where unused SPI could hard-trip. */
            int flow_error = FlightFlow_Poll(&flow, flow_inactive, command, now,
                                             &flow_id_checked);
#else
            int flow_error = Flow_Poll(&flow);
#endif
            if (fresh == 1 && tof.valid && attitude.valid) {
#ifdef AIRCRAFT1_PHYSICAL
                /* Advance inertial state to the ToF reception timestamp before
                 * applying this range observation. */
                (void)Height_Predict(&height, &attitude, acc, tof.last_sample_ms);
                (void)Height_Correct(&height, tof.distance_mm*.001f, attitude.roll, attitude.pitch, tof.last_sample_ms);
#else
                (void)Height_Update(&height, tof.distance_mm*.001f, attitude.roll, attitude.pitch, tof.last_sample_ms);
#endif
            }
#ifndef AIRCRAFT1_PHYSICAL
            if (!tof.valid || !attitude.valid) Height_Reset(&height);
#endif
            position.valid = 0U;
            float average_gyro[3];
            for (unsigned k = 0U; k < 3U; ++k) {
                average_gyro[k] = gyro_integral[k] / flow_dt;
                gyro_integral[k] = 0.0f;
            }
            /* First burst may include motion before this integration interval. */
#ifdef FLOW_CAL_CAPTURE
            FlowCaptureSample capture_sample = {
                .ms=now, .dx=flow.dx, .dy=flow.dy, .dt=flow_dt,
                .height_m=height.height,
                .gyro_dps={average_gyro[0],average_gyro[1],average_gyro[2]},
                .flow_ok=!flow_error && flow.present && flow.valid,
                .range_ok=height.valid && Tof_AgeMs(&tof)<TOF_MAX_AGE_MS,
                .imu_ok=attitude.valid, .same_window=flow_window_started,
                .io_error=flow_error != 0
            };
            FlowCapture_Add(&flow_capture, &capture_sample);
#endif
            if (flow_window_started && !flow_error && flow.valid && height.valid && attitude.valid)
#ifdef AIRCRAFT1_PHYSICAL
                (void)FlowEstimate_Update(&position, &physical_flow, flow.dx, flow.dy,
#else
                (void)FlowEstimate_Update(&position, &flow_calibration, flow.dx, flow.dy,
#endif
                                          height.height, average_gyro, attitude.yaw, flow_dt);
            flow_window_started = !flow_error;
        }
        if (!height.valid || !attitude.valid) position.valid = 0U;
#ifndef AIRCRAFT1_PHYSICAL
        if (Tof_AgeMs(&tof) >= TOF_MAX_AGE_MS || !tof.valid || !attitude.valid) {
            Height_Reset(&height); position.valid = 0U;
        }
#endif
        if (!flow.valid || (uint32_t)(HAL_GetTick() - flow.last_sample_ms) >= AIRCRAFT1_FLOW_MAX_AGE_MS)
            position.valid = 0U;
#else
        int fresh = Tof_Poll(&tof); (void)Flow_Poll(&flow);
        if (fresh == 1 && tof.valid && attitude.valid) {
#ifdef AIRCRAFT1_PHYSICAL
            (void)Height_Predict(&height, &attitude, acc, tof.last_sample_ms);
            (void)Height_Correct(&height, tof.distance_mm*.001f, attitude.roll, attitude.pitch, tof.last_sample_ms);
#else
            (void)Height_Update(&height, tof.distance_mm*.001f, attitude.roll, attitude.pitch, tof.last_sample_ms);
#endif
        }
#ifndef AIRCRAFT1_PHYSICAL
        if (!tof.valid || !attitude.valid) Height_Reset(&height);
#endif
        position.valid = 0;
        if (flow.valid && height.valid && attitude.valid)
            (void)FlowEstimate_Update(&position, &flow_calibration, flow.dx, flow.dy,
                                      height.height, gyro, attitude.yaw, dt);
#endif
#ifdef AIRCRAFT1_STANDALONE
        unsigned link = 1U; /* Local mode: no wireless heartbeat is required. */
#else
        unsigned link = InputLink(&input, HAL_GetTick());
#endif
#ifndef AIRCRAFT1_PHYSICAL
        HoverSample sample = {.roll_deg=attitude.roll, .pitch_deg=attitude.pitch, .yaw_deg=attitude.yaw,
            .gyro_dps={gyro[0],gyro[1],gyro[2]}, .height_m=height.height, .vertical_speed_mps=height.vz,
            .x_m=position.x, .y_m=position.y, .vx_mps=position.vx, .vy_mps=position.vy,
            .imu_valid=attitude.valid && IMU_MOUNT_VERIFIED, .height_valid=height.valid,
            .position_valid=position.valid, .command_link_valid=link};
#endif
#ifdef AIRCRAFT1_INTEGRATION
#ifndef AIRCRAFT1_PHYSICAL
        sample.imu_valid = attitude.valid;
        unsigned wait = Aircraft1_WaitMask(&sample, IMU_MOUNT_VERIFIED, FLOW_CALIBRATED, dt);
        command = Aircraft1_FilterStart(controller.state, command, wait);
        if (wait & (AIRCRAFT1_WAIT_TIMING | AIRCRAFT1_WAIT_MOUNT)) sample.imu_valid = 0U;
#endif
#endif
#ifdef AIRCRAFT1_PHYSICAL
        float yaw_rad = attitude.yaw * .0174532925f;
        unsigned arm_level = attitude.valid && ClosedLoop_ArmLevel(acc);
        ClosedLoopSample actual = {
            .roll = attitude.roll, .pitch = attitude.pitch, .yaw = attitude.yaw,
            .gyro = {gyro[0],gyro[1],gyro[2]},
            /* Use current raw range for the IDLE start-height guard even when
             * the fused estimate has just aged out. */
            .range_m = closed.state == CL_IDLE ?
                       tof.distance_mm*.001f*cosf(attitude.roll*.0174532925f)*
                       cosf(attitude.pitch*.0174532925f) : height.height,
            .vz = height.vz,
            .vx = cosf(yaw_rad)*position.vx + sinf(yaw_rad)*position.vy,
            .vy = -sinf(yaw_rad)*position.vx + cosf(yaw_rad)*position.vy,
            .imu_valid = attitude.valid && (uint32_t)(now - attitude_started) >= 1000U &&
                         (closed.state != CL_IDLE || arm_level),
            .link_valid = link,
            .tof_present = tof.present,
            .tof_fresh = tof.seen && Tof_AgeMs(&tof) < TOF_MAX_AGE_MS &&
                         ((tof.raw_status == 9U && (tof.valid ||
                           (tof.distance_mm >= 5U && tof.distance_mm < 40U))) ||
                          (closed.state != CL_IDLE && height.valid)),
            .tof_near_ground = tof.seen && Tof_AgeMs(&tof) < TOF_MAX_AGE_MS &&
                               tof.raw_status == 9U && tof.distance_mm >= 5U &&
                               tof.distance_mm < 40U,
            .range_valid = height.valid, .flow_valid = position.valid,
            .flow_present = flow.present,
            .flow_calibrated = FLOW_CALIBRATED && CL_FLOW_AXES_VERIFIED,
            .flow_control_provisional = CL_FLOW_PROVISIONAL_CONTROL,
            .flow_x = position.x, .flow_y = position.y,
            .output_fault = Aircraft1Pwm_Tripped()
        };
        ClosedLoopState before = closed.state;
#ifdef FLOW_ID_PROBE
        command = HOVER_STOP; /* Explicit raw-data diagnostic cannot start motors. */
#endif
        PhysicalControl_Step(&closed, &actual, command, dt, &physical);
        if (!Aircraft1Pwm_Apply(physical.motor, HAL_GetTick())) {
            actual.output_fault = 1U;
            PhysicalControl_Step(&closed, &actual, HOVER_NONE, dt, &physical);
        }
        if (before != CL_FAULT && closed.state == CL_FAULT)
            RecordFault(before, &physical, &actual, &tof, attitude.valid, imu_ready, arm_level,
                        now - attitude_started, acc, dt);
        SendFault();
        /* Legacy state is used only to gate blocking idle calibration/reconnect. */
        controller.state = closed.state == CL_IDLE ? HOVER_IDLE :
                           closed.state == CL_FAULT ? HOVER_FAULT : HOVER_TAKEOFF;
        if ((uint32_t)(now - logged) >= 200U) {
            SendClosedLoop(&physical, &attitude, &height, &tof, &flow, &input, (uint32_t)(dt * 1000), arm_level,
                           imu_ready);
            logged = now;
        }
#else
        Hover_Step(&controller, &sample, command, dt, &output);
#ifdef AIRCRAFT1_INTEGRATION
        if ((uint32_t)(now - logged) >= 200U) {
            SendAircraft1(&output, &sample, &tof, &flow, &input, attitude.valid,
                          wait, (uint32_t)(dt * 1000.0f), errors);
            logged = now;
        }
#else
        float ch[32] = {output.state, attitude.valid?attitude.roll:NAN, attitude.valid?attitude.pitch:NAN,
            attitude.valid?attitude.yaw:NAN, height.valid?height.height:NAN, height.valid?height.vz:NAN,
            position.valid?position.x:NAN, position.valid?position.y:NAN,
            position.valid?position.vx:NAN, position.valid?position.vy:NAN, flow.dx, flow.dy, flow.quality,
            imu_ready, tof.valid, flow.valid, IMU_MOUNT_VERIFIED, FLOW_CALIBRATED,
            output.height_target_m, output.collective, output.virtual_motor[0], output.virtual_motor[1],
            output.virtual_motor[2], output.virtual_motor[3], 0, controller.stable_seconds,
            controller.stable_seconds>5.0f, link, input.radio.command, input.radio.accepted, errors, 0};
        SendFloats(ch);
#endif
#endif
        HAL_GPIO_WritePin(BOARD_LED1_GPIO_Port, BOARD_LED1_Pin,
                          link?BOARD_LED1_ON_LEVEL:BOARD_LED1_OFF_LEVEL);
        uint32_t elapsed = HAL_GetTick() - now;
#ifdef AIRCRAFT1_INTEGRATION
        if (elapsed < AIRCRAFT1_PERIOD_MS) HAL_Delay(AIRCRAFT1_PERIOD_MS - elapsed);
#else
        if (elapsed < 20U) HAL_Delay(20U - elapsed);
#endif
    }
}
