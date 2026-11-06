/* 飞行数据记录实现：RAM 环形缓冲 + 落地后写入内部 Flash 保留槽。
 * 设计要点：
 *  - 只在待机（CL_IDLE）时写 Flash，飞行中绝不擦写，避免控制周期被拉长；
 *  - 两个 4 KB 槽轮转（覆盖 seq 较旧的那个），断电最多丢当前一轮；
 *  - 头部带 magic/版本/CRC32，读取端据此判断有效性，不靠猜。 */

#include "flight_log.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stddef.h>


/* 悬停油门学习页：独立于飞行日志双槽，保存"实测悬停油门"。 */
#define FLIGHT_HOVER_ADDR  0x0803D000UL
#define FLIGHT_HOVER_MAGIC 0x564F4832UL

#define FL_MAGIC            0x4C323246UL  /* "F22L" 小端 */
#define FL_VERSION          5U
#define FL_INTERVAL_MS      100U          /* 10 Hz 采样 */
#define FL_CAPACITY         135U          /* (4096-32)/30 = 135（样本实为 30 字节） */
#define FL_PAGES_PER_SLOT   2UL
#define FL_SAMPLE_BYTES     ((uint32_t)sizeof(FlightLogSample))

/* v5 单条样本 30 字节（15 个 16 位字段，packed）。
 * 注意：FL_CAPACITY 必须按此字节数计算，否则会写穿 4 KB 槽。 */
typedef struct __attribute__((packed)) {
    uint16_t u_pm;        /* 油门需求 ‰ */
    int16_t  height_mm;   /* 融合高度（无效记 0） */
    uint16_t tof_mm;      /* 原始测距（无效 0xFFFF） */
    int16_t  roll_cd;     /* 横滚 0.01° */
    int16_t  pitch_cd;    /* 俯仰 0.01° */
    uint16_t sf;          /* state << 8 | fault */
    uint16_t pwm_pm;      /* 四路最大占空比 ‰ */
    uint16_t flow_gain_pm;/* 光流增益 0.001（firmware 的 flow_gain × 1000） */
    uint16_t flow_flags;  /* bit0 valid(flow_valid) bit1 present bit2 requested(use_flow)
                           * bit3 auto_land bit4..7 flow_mode(0..15) */
    int16_t  vx_cms;      /* 光流估计的前向速度 cm/s（用于判断是否被 2m/s 门限拒绝） */
    int16_t  vy_cms;      /* 右向速度 cm/s */
    int16_t  gyro_z_cdps; /* 偏航角速度 0.01°/s（判断光流偏航门限是否卡住） */
    int16_t  corr_roll_pm;/* 横滚差动指令 ‰（判断姿控是否顶在限幅上） */
    int16_t  corr_pitch_pm;
    int16_t  corr_yaw_pm;
} FlightLogSample;

/* 头部 32 字节。 */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t interval_ms;
    uint32_t seq;
    uint32_t count;
    uint32_t duration_ms;
    uint32_t crc32;
    uint16_t max_u_pm;
    uint16_t max_height_mm;
    uint8_t  last_fault;
    uint8_t  reserved[3];
} FlightLogHeader;

static FlightLogSample s_ram[FL_CAPACITY];
static unsigned s_count;
static uint32_t s_last_ms, s_first_ms;
static uint16_t s_max_u_pm, s_max_height_mm;
static uint8_t s_last_fault;
static ClosedLoopState s_prev_state;

/* ---- CRC32 ---- */
static uint32_t CrcInit(void) { return 0xFFFFFFFFUL; }
static uint32_t CrcUpdate(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8U; ++b)
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1U)));
    }
    return crc;
}
static uint32_t RecordCrc(const FlightLogHeader *hdr, const void *body, uint32_t body_len)
{
    FlightLogHeader copy = *hdr;
    copy.crc32 = 0U;
    uint32_t crc = CrcInit();
    crc = CrcUpdate(crc, &copy, (uint32_t)sizeof(copy));
    crc = CrcUpdate(crc, body, body_len);
    return ~crc;
}

/* ---- Flash 槽 ---- */
static uint32_t SlotAddr(unsigned slot)
{
    return FLIGHT_LOG_SLOT0_ADDR + (uint32_t)slot * FLIGHT_LOG_SLOT_SIZE;
}
static const FlightLogHeader *SlotHeader(unsigned slot)
{
    return (const FlightLogHeader *)SlotAddr(slot);
}
static unsigned SlotValid(unsigned slot)
{
    const FlightLogHeader *h = SlotHeader(slot);
    if (h->magic != FL_MAGIC || h->version != FL_VERSION) return 0U;
    if (h->count == 0U || h->count > FL_CAPACITY) return 0U;
    uint32_t body = h->count * FL_SAMPLE_BYTES;
    if ((uint32_t)sizeof(FlightLogHeader) + body > FLIGHT_LOG_SLOT_SIZE) return 0U;
    const void *src = (const void *)(SlotAddr(slot) + (uint32_t)sizeof(FlightLogHeader));
    return RecordCrc(h, src, body) == h->crc32 ? 1U : 0U;
}

static unsigned FlashErasePages(uint32_t addr, uint32_t pages)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = addr;
    erase.NbPages = pages;
    HAL_FLASH_Unlock();
    unsigned ok = HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK ? 1U : 0U;
    HAL_FLASH_Lock();
    return ok;
}

static unsigned FlashWriteHalfwords(uint32_t addr, const void *data, uint32_t len)
{
    const uint16_t *src = (const uint16_t *)data;
    uint32_t halfwords = (len + 1U) / 2U;
    unsigned ok = 1U;
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < halfwords; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i * 2U,
                              (uint64_t)src[i]) != HAL_OK) { ok = 0U; break; }
    }
    HAL_FLASH_Lock();
    return ok;
}


/* ---- 悬停油门学习 ---- */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    float    hover_duty;
    float    reserved[3];
    uint32_t crc32;
} HoverRecord;

float FlightLog_LoadHoverDuty(void)
{
    const HoverRecord *r = (const HoverRecord *)FLIGHT_HOVER_ADDR;
    if (r->magic != FLIGHT_HOVER_MAGIC) return -1.0f;
    uint32_t crc = CrcInit();
    crc = CrcUpdate(crc, r, (uint32_t)offsetof(HoverRecord, crc32));
    if (~crc != r->crc32) return -1.0f;
    /* 2026-10-29：学习页曾存进 512‰（带故障飞行学到的垃圾值），而真实悬停
     * 在 700~750‰。低于 55% 的值不可信，直接作废、回退默认前馈。 */
    if (!(r->hover_duty > 0.55f && r->hover_duty < 0.95f)) return -1.0f;
    return r->hover_duty;
}

static void SaveHoverDuty(float duty)
{
    if (!(duty > 0.55f && duty < 0.95f)) return;   /* 与读取端一致：拒绝不可信值 */
    HoverRecord r;
    memset(&r, 0, sizeof(r));
    r.magic = FLIGHT_HOVER_MAGIC;
    r.hover_duty = duty;
    uint32_t crc = CrcInit();
    crc = CrcUpdate(crc, &r, (uint32_t)offsetof(HoverRecord, crc32));
    r.crc32 = ~crc;
    if (!FlashErasePages(FLIGHT_HOVER_ADDR, 1U)) return;
    (void)FlashWriteHalfwords(FLIGHT_HOVER_ADDR, &r, (uint32_t)sizeof(r));
}

static void WriteRecord(uint32_t seq)
{
    unsigned v0 = SlotValid(0U), v1 = SlotValid(1U);
    unsigned target;
    if (v0 && v1) target = (SlotHeader(0U)->seq <= SlotHeader(1U)->seq) ? 0U : 1U;
    else if (v0) target = 1U;
    else target = 0U;

    FlightLogHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = FL_MAGIC;
    h.version = FL_VERSION;
    h.interval_ms = FL_INTERVAL_MS;
    h.seq = seq;
    h.count = s_count;
    h.duration_ms = s_last_ms - s_first_ms;
    h.max_u_pm = s_max_u_pm;
    h.max_height_mm = s_max_height_mm;
    h.last_fault = s_last_fault;
    uint32_t body = s_count * FL_SAMPLE_BYTES;
    h.crc32 = RecordCrc(&h, s_ram, body);

    uint32_t addr = SlotAddr(target);
    if (!FlashErasePages(addr, FL_PAGES_PER_SLOT)) return;
    if (!FlashWriteHalfwords(addr, &h, (uint32_t)sizeof(h))) return;
    (void)FlashWriteHalfwords(addr + (uint32_t)sizeof(h), s_ram, body);
}

static uint32_t NextSeq(void)
{
    uint32_t seq = 0U;
    unsigned any = 0U;
    if (SlotValid(0U)) { seq = SlotHeader(0U)->seq; any = 1U; }
    if (SlotValid(1U)) {
        uint32_t s1 = SlotHeader(1U)->seq;
        if (!any || s1 > seq) seq = s1;
        any = 1U;
    }
    return any ? seq + 1U : 1U;
}

void FlightLog_Reset(void)
{
    memset(s_ram, 0, sizeof(s_ram));
    s_count = 0U;
    s_last_ms = 0U;
    s_first_ms = 0U;
    s_max_u_pm = 0U;
    s_max_height_mm = 0U;
    s_last_fault = (uint8_t)CL_OK;
    s_prev_state = CL_IDLE;
}

unsigned FlightLog_SampleCount(void) { return s_count; }

unsigned FlightLog_Tick(const ClosedLoop *c, const ClosedLoopOutput *out,
                        const ClosedLoopSample *s, const TofSensor *tof,
                        uint32_t now_ms)
{
    ClosedLoopState st = c->state;

    /* 新的一轮起飞：清空缓冲（序号在写盘时从 Flash 续）。 */
    if (s_prev_state == CL_IDLE && st != CL_IDLE) {
        s_count = 0U;
        s_max_u_pm = 0U;
        s_max_height_mm = 0U;
        s_last_fault = (uint8_t)CL_OK;
        s_first_ms = now_ms;
    }

    if (st != CL_IDLE && s_count < FL_CAPACITY &&
        (s_count == 0U || (uint32_t)(now_ms - s_last_ms) >= FL_INTERVAL_MS)) {
        FlightLogSample *r = &s_ram[s_count];
        float u = out->collective;
        float pm = 0.0f;
        for (unsigned i = 0; i < 4U; ++i) if (out->motor[i] > pm) pm = out->motor[i];
        r->u_pm = (uint16_t)(u <= 0.0f ? 0.0f : u * 1000.0f + 0.5f);
        r->height_mm = (int16_t)(s->range_valid ? s->range_m * 1000.0f : 0.0f);
        r->tof_mm = (s->tof_present && tof) ? tof->distance_mm : 0xFFFFU;
        r->roll_cd = (int16_t)(s->roll * 100.0f);
        r->pitch_cd = (int16_t)(s->pitch * 100.0f);
        r->sf = (uint16_t)(((uint16_t)st << 8) | (uint16_t)c->fault);
        r->pwm_pm = (uint16_t)(pm * 1000.0f + 0.5f);
        r->flow_gain_pm = (uint16_t)(out->flow_gain <= 0.0f ? 0.0f
                                     : out->flow_gain * 1000.0f + 0.5f);
        r->flow_flags = (uint16_t)((s->flow_valid ? 1U : 0U) |
                                   (s->flow_present ? 2U : 0U) |
                                   (out->flow_requested ? 4U : 0U) |
                                   (out->flow_auto_land ? 8U : 0U) |
                                   ((out->flow_mode & 0x0FU) << 4));
#ifdef CL_INS_VEL_FUSE
        /* 记录"控制器实际使用"的速度（加速度+光流互补滤波结果），
         * 而不是原始光流速度——否则无法判断融合是否起作用。 */
        r->vx_cms = (int16_t)(c->ins_vx * 100.0f);
        r->vy_cms = (int16_t)(c->ins_vy * 100.0f);
#else
        r->vx_cms = (int16_t)(s->vx * 100.0f);
        r->vy_cms = (int16_t)(s->vy * 100.0f);
#endif
        r->gyro_z_cdps = (int16_t)(s->gyro[2] * 100.0f);
#ifdef CL_LOG_POSITION
        /* 2026-10-31：改为记录"光流累计位移(mm)"，用于量化实际漂移距离。
         * corr_roll_pm 字段存 X(mm)，corr_pitch_pm 字段存 Y(mm)。 */
        r->corr_roll_pm = (int16_t)(s->flow_x * 1000.0f);
        r->corr_pitch_pm = (int16_t)(s->flow_y * 1000.0f);
        r->corr_yaw_pm = (int16_t)(out->rate_correction[2] * 1000.0f);
#else
        r->corr_roll_pm = (int16_t)(out->rate_correction[0] * 1000.0f);
        r->corr_pitch_pm = (int16_t)(out->rate_correction[1] * 1000.0f);
        r->corr_yaw_pm = (int16_t)(out->rate_correction[2] * 1000.0f);
#endif
        s_count++;
        if (r->u_pm > s_max_u_pm) s_max_u_pm = r->u_pm;
        if (r->height_mm > 0 && (uint16_t)r->height_mm > s_max_height_mm)
            s_max_height_mm = (uint16_t)r->height_mm;
        s_last_fault = (uint8_t)c->fault;
        s_last_ms = now_ms;
    }

    unsigned wrote = 0U;
    if (s_prev_state != CL_IDLE && st == CL_IDLE && s_count > 0U) {
        /* 学习：取本轮 HOLD 段油门的平均值（样本足够才更新），
         * 下次上电作为高度环前馈使用。 */
        uint32_t sum = 0U; unsigned n = 0U;
        for (unsigned i = 0; i < s_count; ++i) {
            if ((s_ram[i].sf >> 8) == CL_HOLD) { sum += s_ram[i].u_pm; n++; }
        }
        if (n >= 5U) SaveHoverDuty((float)sum / (float)n / 1000.0f);
        WriteRecord(NextSeq());
        wrote = 1U;
    }
    s_prev_state = st;
    return wrote;
}
