#!/usr/bin/env python3
"""Save motor-locked FCAL cumulative telemetry; never transmit serial bytes."""
import argparse
import datetime
import json
from pathlib import Path
import re
import time

PORT = "/dev/serial/by-id/usb-Horco_Horco_CMSIS-DAP_814dc35a-if02"
SCALARS = ("ms epoch polls raw_n raw_dx raw_dy n bad pollerr why sat dxsum dysum "
           "hx_mcount_milli hy_mcount_milli time_ms imu tof valid mm roll_cd pitch_cd "
           "flow id inv q s_dx s_dy motion obs rawsum rawmax rawmin shutter "
           "i2c_boot output_locked").split()
VECTORS = {"angle_urad": 3, "hangle_umrad": 3, "PWM": 4}


def parse_line(line):
    if not line.startswith("[FCAL] "):
        return None
    fields = dict(re.findall(r"\b(\w+)=([^ ]+)", line.strip()))
    try:
        result = {key: int(fields[key]) for key in SCALARS}
        for key, size in VECTORS.items():
            result[key] = [int(v) for v in fields[key].split(",")]
            if len(result[key]) != size:
                return None
    except (KeyError, ValueError):
        return None
    return result


def atomic_json(path, value):
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    temp.replace(path)


def key_marker(line, snapshot, age):
    match = re.fullmatch(r"KEY F([234]) event=(\d+) seq=(\d+) allowed=([01]) ready=([01])", line)
    if not match:
        return None
    key, event, sequence, allowed, ready = map(int, match.groups())
    reason = "ok"
    if not allowed:
        reason = "key_not_allowed"
    elif snapshot is None or age > .5:
        reason = "no_recent_snapshot"
    elif snapshot["output_locked"] != 1 or snapshot["PWM"] != [0, 0, 0, 0]:
        reason = "motors_not_locked"
    elif snapshot.get("sat", 0):
        reason = "accumulators_saturated"
    elif not all(snapshot[k] == 1 for k in ("imu", "tof", "flow", "valid")) or snapshot["mm"] < 80:
        reason = "sensors_not_ready"
    unavailable = []
    if snapshot is not None:
        unavailable = [key for key in ("imu", "tof", "flow", "valid") if snapshot[key] != 1]
        if snapshot["mm"] < 80:
            unavailable.append("range_below_80mm")
    return {"key": key, "event": event, "sequence": sequence, "allowed": allowed,
            "ready": ready, "snapshot_valid": reason == "ok", "reason": reason,
            "snapshot": snapshot if reason == "ok" else None,
            "not_ready_fields": unavailable,
            "diagnostic_snapshot": snapshot if reason != "ok" else None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--port", default=PORT)
    parser.add_argument("--seconds", type=float, default=1200)
    args = parser.parse_args()
    if not 0 < args.seconds <= 1800:
        parser.error("seconds must be in (0, 1800]")
    import serial
    folder = args.out_dir
    folder.mkdir(parents=True, exist_ok=True)
    meta = {"start": datetime.datetime.now().astimezone().isoformat(),
            "port": args.port, "baud": 115200, "motor_command_sent": False,
            "connected": False, "connections": 0, "disconnects": [],
            "open_failures": 0, "discarded_partial_bytes": 0}
    atomic_json(folder/"capture.json", meta)
    uart = serial.Serial(port=None, baudrate=115200, timeout=.2, exclusive=True)
    uart.dtr = uart.rts = True
    uart.port = args.port
    count = partial = 0
    start = time.monotonic()
    retry_at = start
    last_byte_at = start
    pending = b""
    latest = None
    latest_at = start
    try:
        with (folder/"serial.log").open("wb") as raw, \
                (folder/"samples.jsonl").open("w") as samples, \
                (folder/"keys.jsonl").open("w") as keys:
            print("CAPTURE_STARTED", str(folder), flush=True)
            while time.monotonic()-start < args.seconds and not (folder/"STOP").exists():
                if not uart.is_open:
                    if time.monotonic() < retry_at:
                        time.sleep(.1)
                        continue
                    try:
                        uart.open()
                        uart.reset_input_buffer()
                    except (serial.SerialException, OSError) as error:
                        uart.close()
                        meta["open_failures"] += 1
                        meta["last_error"] = str(error)
                        atomic_json(folder/"capture.json", meta)
                        retry_at = time.monotonic() + 1
                        continue
                    meta["connected"] = True
                    meta["connections"] += 1
                    last_byte_at = time.monotonic()
                    atomic_json(folder/"capture.json", meta)
                    print("CAPTURE_CONNECTED", meta["connections"], flush=True)
                try:
                    data = uart.read(2048)
                    if data:
                        last_byte_at = time.monotonic()
                    elif time.monotonic() - last_byte_at >= 3:
                        raise serial.SerialException("no serial data for 3 seconds")
                except (serial.SerialException, OSError) as error:
                    meta["connected"] = False
                    meta["disconnects"].append({
                        "host_seconds": round(time.monotonic()-start, 4),
                        "error": str(error)})
                    # A fragment from before disconnect must never be joined
                    # to a new stream. Cumulative FCAL counters stay untouched.
                    meta["discarded_partial_bytes"] += len(pending)
                    pending = b""
                    latest = None
                    uart.close()
                    atomic_json(folder/"capture.json", meta)
                    print("CAPTURE_RECONNECTING", str(error), flush=True)
                    retry_at = time.monotonic() + 1
                    continue
                raw.write(data)
                raw.flush()
                pending += data
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    text = line.decode("utf8", "replace").strip()
                    marker = key_marker(text, latest, time.monotonic() - latest_at)
                    if marker is not None:
                        marker["host_time"] = datetime.datetime.now().astimezone().isoformat()
                        marker["host_seconds"] = round(time.monotonic() - start, 4)
                        marker["host_connection"] = meta["connections"]
                        keys.write(json.dumps(marker) + "\n")
                        keys.flush()
                        atomic_json(folder/"last-key.json", marker)
                        print("CAPTURE_KEY", marker["key"], marker["reason"], flush=True)
                        continue
                    values = parse_line(text)
                    if values is None:
                        partial += text.startswith("[FCAL]")
                        continue
                    values["host_seconds"] = round(time.monotonic()-start, 4)
                    values["host_time"] = datetime.datetime.now().astimezone().isoformat()
                    values["host_connection"] = meta["connections"]
                    samples.write(json.dumps(values) + "\n")
                    samples.flush()
                    atomic_json(folder/"latest.json", values)
                    latest, latest_at = values, time.monotonic()
                    count += 1
            raw.write(pending)
    except KeyboardInterrupt:
        meta["interrupted"] = True
    finally:
        uart.close()
        meta.update(seconds=round(time.monotonic()-start, 3), samples=count,
                    partial_frames=partial, serial_released=True, connected=False)
        atomic_json(folder/"capture.json", meta)
        print("CAPTURE_DONE", json.dumps(meta), flush=True)


if __name__ == "__main__":
    main()
