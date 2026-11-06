#!/usr/bin/env python3
"""解码 F22 飞行数据记录（内部 Flash 保留槽 0x0803E000 起的 8 KB）。

用法：
  # 先用 SWD dump 出 8 KB（槽0+槽1）
  openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
      -c "init" -c "dump_image /tmp/f22-log.bin 0x0803E000 0x2000" -c "shutdown"
  # 再解码
  python3 scripts/decode_flightdata.py /tmp/f22-log.bin [--csv out.csv]

布局（小端、packed）：
  头部 32 B : magic 'F22L' | version u16 | interval_ms u16 | seq u32 | count u32
              | duration_ms u32 | crc32 u32 | max_u_pm u16 | max_height_mm u16
              | last_fault u8 | reserved[3]
  样本（按版本变化，v5 = 30 B）:
    v1 12 B: u_pm, height_mm, tof_mm, roll_cd, pitch_cd, sf, pwm_pm
    v2 16 B: + flow_gain_pm, flow_flags
    v3 20 B: + vx_cms, vy_cms
    v4 22 B: + gyro_z_cdps
    v5 30 B: + corr_roll_pm, corr_pitch_pm, corr_yaw_pm
"""
import struct
import sys
import zlib
import pathlib

MAGIC = b'F22L'
VERSION = 1
SLOT_SIZE = 0x1000
HEADER = struct.Struct('<4sHHIIIIHHB3s')   # 32 字节
SAMPLE_V1 = struct.Struct('<HhHhhHH')       # v1: 12 字节
SAMPLE_V2 = struct.Struct('<HhHhhHHHH')     # v2: 16 字节（+ 光流诊断）
SAMPLE_V3 = struct.Struct('<HhHhhHHHHhh')   # v3: 20 字节（+ vx/vy cm/s）
SAMPLE_V4 = struct.Struct('<HhHhhHHHHhhh')  # v4: 22 字节（+ gyro_z）
SAMPLE_V5 = struct.Struct('<HhHhhHHHHhhhhhh')  # v5: 30 字节（+ 三轴差动 ‰）
STATE_NAMES = {0: 'IDLE', 1: 'LAUNCH', 2: 'ASCEND', 3: 'HOLD', 4: 'LAND', 5: 'FAULT'}
FAULT_NAMES = {0: 'OK', 1: 'AXES_UNVERIFIED', 2: 'INPUT_INVALID', 3: 'LINK_LOST', 4: 'TIMING',
               5: 'TILT', 6: 'TOF_LOST', 7: 'LAUNCH_TIMEOUT', 8: 'START_TOO_HIGH',
               9: 'OUTPUT_FAULT', 10: 'LAND_TIMEOUT', 11: 'IMU_INVALID', 12: 'HEIGHT_INVALID',
               13: 'FLOW_UNVERIFIED', 14: 'FLOW_UNAVAILABLE', 15: 'FLIGHT_INHIBITED'}


def parse_slot(buf, base):
    if len(buf) < base + HEADER.size:
        return None
    magic, version, interval_ms, seq, count, duration_ms, crc, max_u, max_h, fault, _ = \
        HEADER.unpack_from(buf, base)
    if magic != MAGIC or version not in (1, 2, 3, 4, 5):
        return None
    sample_struct = {1: SAMPLE_V1, 2: SAMPLE_V2, 3: SAMPLE_V3, 4: SAMPLE_V4, 5: SAMPLE_V5}[version]
    if count == 0 or count > 336:
        return None
    body_len = count * sample_struct.size
    if HEADER.size + body_len > SLOT_SIZE or len(buf) < base + HEADER.size + body_len:
        return None
    hdr_zero = bytearray(buf[base:base + HEADER.size])
    hdr_zero[20:24] = b'\0\0\0\0'
    got = zlib.crc32(bytes(hdr_zero) + bytes(buf[base + HEADER.size:base + HEADER.size + body_len])) & 0xFFFFFFFF
    if got != crc:
        return None
    samples = [sample_struct.unpack_from(buf, base + HEADER.size + i * sample_struct.size)
               for i in range(count)]
    return {'seq': seq, 'version': version, 'interval_ms': interval_ms, 'count': count,
            'duration_ms': duration_ms, 'max_u_pm': max_u, 'max_height_mm': max_h,
            'last_fault': fault, 'samples': samples}


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = pathlib.Path(sys.argv[1])
    buf = path.read_bytes()
    csv_path = None
    if '--csv' in sys.argv:
        csv_path = pathlib.Path(sys.argv[sys.argv.index('--csv') + 1])
    recs = [r for r in (parse_slot(buf, i * SLOT_SIZE) for i in range(2)) if r]
    if not recs:
        print('未找到有效记录（magic/CRC 不符）——确认 dump 起始地址是 0x0803E000')
        return 1
    recs.sort(key=lambda r: r['seq'])
    for r in recs:
        print(f"seq={r['seq']} 样本={r['count']} 时长={r['duration_ms']/1000:.1f}s "
              f"采样={1000//max(r['interval_ms'],1)}Hz 最大油门={r['max_u_pm']}‰ "
              f"最高={r['max_height_mm']}mm 最后故障={FAULT_NAMES.get(r['last_fault'], r['last_fault'])}")
        states = {}
        for s in r['samples']:
            st = s[5] >> 8
            states[st] = states.get(st, 0) + 1
        print('  状态分布:', ', '.join(f'{STATE_NAMES.get(k,k)}={v}' for k, v in sorted(states.items())))
    latest = recs[-1]
    if csv_path:
        with csv_path.open('w', encoding='utf-8') as fh:
            fh.write('t_s,state,fault,u_pm,height_mm,tof_mm,roll_deg,pitch_deg,pwm_pm\n')
            for i, s in enumerate(latest['samples']):
                u, h, tof, roll, pitch, sf, pwm = s[:7]
                st, fault = sf >> 8, sf & 0xFF
                fh.write(f"{i*latest['interval_ms']/1000:.2f},{STATE_NAMES.get(st,st)},{fault},"
                         f"{u},{h},{'--' if tof == 0xFFFF else tof},{roll/100:.2f},{pitch/100:.2f},{pwm}\n")
        print('CSV 已写入:', csv_path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
