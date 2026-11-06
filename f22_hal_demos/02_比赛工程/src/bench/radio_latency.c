/* Communication-only profile: all seven motor controls held LOW, no timers/PWM. */
#include "board_f22.h"
#include "common.h"
#include "gpio.h"
#include "system_clock.h"
#include "usart.h"
#include "radio_trace.h"
#include "radio.h"
#ifdef LATENCY_RADIO_FAST
#include "radio_fast_config.h"
#endif
#include <stdio.h>

typedef struct { uint8_t byte; uint32_t ms; } ReceivedByte;
static volatile ReceivedByte queue[256];
static volatile uint8_t head, tail, rx_byte;
static volatile uint32_t rx_bytes, errors, dropped;

static void MotorsOff(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BOARD_M1_Pin | BOARD_M2_Pin | BOARD_M3_Pin | BOARD_M4_Pin;
    HAL_GPIO_WritePin(GPIOC, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = BOARD_M6_Pin | BOARD_M7_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = BOARD_M5_Pin;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart1) return;
    uint32_t received_ms = HAL_GetTick();
    uint8_t next = (uint8_t)(head + 1U);
    rx_bytes++;
    if (next == tail) dropped++;
    else { queue[head].byte = rx_byte; queue[head].ms = received_ms; head = next; }
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U) != HAL_OK) errors++;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{ if (uart == &huart1) errors++; }

static unsigned tx_errors;
static void Log(const char *line, unsigned length)
{
#ifdef LATENCY_COMPACT_LOG
    /* Fixed 64-byte UART record for a packet-size comparison. DAP/USB
     * buffering may split or combine records independently of this size.
     * Content/padding [0..58], '*'+CRC8 hex [59..61], CRLF [62..63]. */
    uint8_t record[64];
    while (length && (line[length - 1U] == '\r' || line[length - 1U] == '\n')) length--;
    if (length > 59U) { tx_errors++; return; }
    for (unsigned i = 0; i < 59U; ++i) record[i] = i < length ? (uint8_t)line[i] : ' ';
    static const char hex[] = "0123456789ABCDEF";
    uint8_t crc = Radio_Crc(record, 59U);
    record[59] = '*'; record[60] = hex[crc >> 4]; record[61] = hex[crc & 15U];
    record[62] = '\r'; record[63] = '\n';
    if (HAL_UART_Transmit(&huart2, record, sizeof(record), 30U) != HAL_OK) tx_errors++;
#else
    if (HAL_UART_Transmit(&huart2, (uint8_t *)line, (uint16_t)length, 30U) != HAL_OK)
        tx_errors++;
#endif
}
int main(void)
{
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB(); __enable_irq();
    HAL_Init(); MotorsOff(); SystemClock_Config(); MX_GPIO_Init();
    MX_USART1_UART_Init(); MX_USART2_UART_Init();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET); HAL_GPIO_Init(GPIOA, &gpio);
#ifdef LATENCY_RADIO_FAST
    RadioFastConfig config; RadioFast_Configure(&config);
    char config_line[144];
    int config_n = snprintf(config_line, sizeof(config_line),
#ifdef LATENCY_COMPACT_LOG
        "C %u %u %u %02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X 0\r\n",
#else
        "CFG RX read=%u applied=%u rollback=%u original=%02X%02X%02X%02X%02X%02X active=%02X%02X%02X%02X%02X%02X motors=0\r\n",
#endif
        config.read_ok, config.applied, config.rollback_ok,
        config.original[0], config.original[1], config.original[2], config.original[3], config.original[4], config.original[5],
        config.active[0], config.active[1], config.active[2], config.active[3], config.active[4], config.active[5]);
    if (config_n > 0 && (unsigned)config_n < sizeof(config_line)) Log(config_line, (unsigned)config_n);
#endif
    huart1.Init.Mode = UART_MODE_RX; /* Never send debug text into E49. */
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0); HAL_NVIC_EnableIRQ(USART1_IRQn);
    if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U) != HAL_OK) errors++;
    char boot[80];
    int boot_n = snprintf(boot, sizeof(boot), "BOOT RADIO_LATENCY JP13=115200 radio=%lu motors=0\r\n",
                          (unsigned long)huart1.Init.BaudRate);
    if (boot_n > 0 && (unsigned)boot_n < sizeof(boot)) Log(boot, (unsigned)boot_n);
    RadioTrace trace; RadioTrace_Init(&trace);
    uint32_t frames = 0, last_frame = 0, logged = HAL_GetTick(), recovered = errors;
    uint32_t prior_rejected = 0;
    uint32_t prior_crc = 0, prior_timeouts = 0, prior_timeout_bytes = 0;
    uint32_t prior_dropped = dropped;
    for (;;) {
        if (recovered != errors || prior_dropped != dropped) {
            (void)HAL_UART_AbortReceive(&huart1);
            __disable_irq(); tail = head; __enable_irq();
            prior_rejected += trace.rejected;
            prior_crc += trace.crc_failures; prior_timeouts += trace.timeouts;
            prior_timeout_bytes += trace.timeout_bytes;
            RadioTrace_Init(&trace);
            if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1U) != HAL_OK) errors++;
            recovered = errors; prior_dropped = dropped;
        }
        while (tail != head) {
            uint8_t byte = queue[tail].byte; uint32_t ms = queue[tail].ms;
            tail = (uint8_t)(tail + 1U);
            RadioTraceEvent event;
            uint32_t prior_bad = trace.crc_failures;
            unsigned accepted = RadioTrace_Feed(&trace, byte, ms, &event);
            if (trace.crc_failures != prior_bad) {
                char bad[25], line[112];
                for (unsigned i = 0; i < trace.last_bad_size; ++i)
                    (void)snprintf(bad + 2U*i, 3U, "%02X", trace.last_bad[i]);
                int n = snprintf(line, sizeof(line),
#ifdef LATENCY_COMPACT_LOG
                    "B %08lX %s %08lX\r\n",
#else
                    "BAD crc rx_ms=%lu raw=%s gap_max=%lu\r\n",
#endif
                    (unsigned long)trace.last_bad_ms, bad, (unsigned long)trace.max_partial_gap_ms);
                if (n > 0 && (unsigned)n < sizeof(line)) Log(line, (unsigned)n);
            }
            if (!accepted) continue;
            frames++; last_frame = event.rx_ms;
            char raw[25], line[144];
            for (unsigned i = 0; i < event.size; ++i)
                (void)snprintf(raw + 2U * i, 3U, "%02X", (unsigned)event.raw[i]);
            int n = snprintf(line, sizeof(line),
#ifdef LATENCY_COMPACT_LOG
                "R %s %08lX\r\n", raw, (unsigned long)event.rx_ms);
#else
                "RX v=%u seq=%u cmd=%u keys=%u tx_ms=%lu rx_ms=%lu raw=%s motors=0\r\n",
                event.version, event.sequence, event.command, event.keys,
                (unsigned long)event.tx_ms, (unsigned long)event.rx_ms, raw);
#endif
            if (n > 0 && (unsigned)n < sizeof(line)) Log(line, (unsigned)n);
        }
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - logged) >= 1000U) {
            char line[240];
            int n = snprintf(line, sizeof(line),
#ifdef LATENCY_COMPACT_LOG
                "S %08lX %08lX %08lX %08lX %08lX %08X %u 0\r\n",
#else
                "STAT bytes=%lu frames=%lu reject=%lu err=%lu drop=%lu tx_err=%u link=%u motors=0 crc=%lu timeout=%lu timeout_bytes=%lu gap_max=%lu\r\n",
#endif
                (unsigned long)rx_bytes, (unsigned long)frames, (unsigned long)(prior_rejected + trace.rejected),
                (unsigned long)errors, (unsigned long)dropped, tx_errors,
                frames != 0U && (uint32_t)(now - last_frame) < 300U
#ifdef LATENCY_COMPACT_LOG
                );
            if (n > 0 && (unsigned)n < sizeof(line)) Log(line, (unsigned)n);
            n = snprintf(line, sizeof(line), "D %08lX %08lX %08lX %08lX\r\n",
#else
                ,
#endif
                (unsigned long)(prior_crc + trace.crc_failures),
                (unsigned long)(prior_timeouts + trace.timeouts),
                (unsigned long)(prior_timeout_bytes + trace.timeout_bytes),
                (unsigned long)trace.max_partial_gap_ms);
            if (n > 0 && (unsigned)n < sizeof(line)) Log(line, (unsigned)n);
            logged = now;
        }
    }
}
