#ifndef F22_FLIGHT_LOG_H
#define F22_FLIGHT_LOG_H

/* 飞行数据记录：飞行中把关键状态写进 RAM 环形缓冲，落地（回到待机）时
 * 一次性写入 MCU 内部 Flash 的保留槽位（最后 8 KB，两个 4 KB 槽轮转）。
 * 刻意不在飞行中写 Flash：擦写会让 CPU 停顿数十毫秒，可能破坏控制周期。
 * 读取方式：落地后用 SWD dump 0x0803E000 起 8 KB，再用
 * scripts/decode_flightdata.py 解码（不需要串口）。 */

#include "closed_loop.h"
#include "tof.h"
#include <stdint.h>

/* 保留区（STM32F103RC 256 KB Flash 的最后 8 KB）：
 *   槽 0: 0x0803E000..0x0803EFFF（2 页 × 2 KB）
 *   槽 1: 0x0803F000..0x0803FFFF
 * 固件当前约 53 KB，远低于 0x0803E000，不冲突。 */
#define FLIGHT_LOG_SLOT0_ADDR 0x0803E000UL
#define FLIGHT_LOG_SLOT_SIZE  0x00001000UL
#define FLIGHT_LOG_SLOTS      2U

/* 初始化（清零 RAM 缓冲，飞行序号从 Flash 里已有记录继续）。 */
void FlightLog_Reset(void);

/* 每个主循环调用一次。返回 1 表示本轮刚写过 Flash（调用方需重置时间基准，
 * 否则下一拍 dt 会把停顿算进去，触发 CL_TIMING）。 */
unsigned FlightLog_Tick(const ClosedLoop *c, const ClosedLoopOutput *out,
                        const ClosedLoopSample *s, const TofSensor *tof,
                        uint32_t now_ms);

/* 已缓存样本数 / 是否有一轮待写（调试用）。 */
unsigned FlightLog_SampleCount(void);

#endif

/* 悬停油门学习：把本轮 HOLD 段稳定时的平均油门存到独立 Flash 页，
 * 下次上电读回来作为高度环前馈（比"起飞斜坡末段油门"可靠）。
 * 返回 <0 表示还没有学过。 */
float FlightLog_LoadHoverDuty(void);
