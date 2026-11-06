#!/usr/bin/env python3
"""Observe remote TX and receiver JP13 telemetry with one PC monotonic clock.

No bytes are transmitted by this script. Observed delay includes the receiver's
JP13 log serialization, wireless DAP, USB buffering and host scheduling. It is
not a synchronized one-way RF measurement. Board uptimes are never subtracted.
"""
import argparse
import csv
import json
import re
import select
import time
from collections import defaultdict, deque
from pathlib import Path


COMMANDS = ("HEARTBEAT", "F2", "F3", "F4", "F1")
RX_PATTERN = re.compile(
    r"RX v=(\d+) seq=(\d+) cmd=(\d+) keys=(\d+) tx_ms=(\d+) rx_ms=(\d+) raw=([0-9A-F]+) motors=0"
)


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 0x80 else 0)) & 255
    return crc


def decode_frame(raw):
    if len(raw) < 3 or raw[:2] != b"\xa5\x5a":
        return None
    version = raw[2]
    size = {1: 6, 2: 12, 3: 12}.get(version)
    if size != len(raw) or crc8(raw[:-1]) != raw[-1]:
        return None
    cmd = raw[4] if version == 1 else raw[5]
    if cmd > 4 or (version == 2 and raw[6] > 15):
        return None
    return {
        "version": version,
        "sequence": raw[3] if version == 1 else int.from_bytes(raw[3:5], "little"),
        "command": cmd,
        "keys": None if version == 1 else raw[6] & 15,
        "event_token": raw[6] >> 4 if version == 3 else None,
        "tx_ms": None if version == 1 else int.from_bytes(raw[7:11], "little"),
        "raw": raw.hex().upper(),
    }


def decode_hex_frame(raw):
    try:
        return decode_frame(bytes.fromhex(raw))
    except ValueError:
        return None


def parse_receiver_line(raw):
    """Validate/expand a fixed 64-byte compact record, or retain legacy text."""
    raw = raw.rstrip(b"\r")
    compact = (raw[:2] in (b"R ", b"S ", b"D ", b"C ", b"B ")
               or re.search(rb"\*[0-9A-Fa-f]{2}$", raw) is not None)
    if compact and (len(raw) != 62 or raw[59:60] != b"*"):
        return None
    if len(raw) == 62 and raw[59:60] == b"*":
        try:
            if crc8(raw[:59]) != int(raw[60:62], 16):
                return None
            line = raw[:59].decode("ascii").rstrip()
            fields = line.split()
            if fields[0] == "R" and len(fields) == 3:
                event = decode_hex_frame(fields[1])
                if event is None:
                    return None
                return (f"RX v={event['version']} seq={event['sequence']} cmd={event['command']} "
                        f"keys={event['keys'] or 0} tx_ms={event['tx_ms'] or 0} "
                        f"rx_ms={int(fields[2], 16)} raw={event['raw']} motors=0")
            if fields[0] == "S" and len(fields) == 9:
                names = ("bytes", "frames", "reject", "err", "drop", "tx_err")
                parts = [f"{name}={int(value, 16)}" for name, value in zip(names, fields[1:7])]
                return "STAT " + " ".join(parts) + f" link={int(fields[7])} motors={int(fields[8])}"
            if fields[0] == "D" and len(fields) == 5:
                names = ("crc", "timeout", "timeout_bytes", "gap_max")
                return "DIAG " + " ".join(f"{name}={int(value, 16)}" for name, value in zip(names, fields[1:]))
            if fields[0] == "C" and len(fields) == 7:
                return (f"CFG RX read={int(fields[1])} applied={int(fields[2])} rollback={int(fields[3])} "
                        f"original={fields[4]} active={fields[5]} motors={int(fields[6])}")
            if fields[0] == "B" and len(fields) == 4:
                return f"BAD crc rx_ms={int(fields[1], 16)} raw={fields[2]} gap_max={int(fields[3], 16)}"
            return line
        except (ValueError, UnicodeDecodeError, IndexError):
            return None
    return raw.decode("ascii", errors="replace")


class FrameDecoder:
    def __init__(self):
        self.buffer = bytearray()
        self.noise_bytes = self.bad_frames = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while len(self.buffer) >= 3:
            size = {1: 6, 2: 12, 3: 12}.get(self.buffer[2])
            if self.buffer[:2] != b"\xa5\x5a" or size is None:
                del self.buffer[0]
                self.noise_bytes += 1
                continue
            if len(self.buffer) < size:
                break
            event = decode_frame(bytes(self.buffer[:size]))
            if event is None:
                self.bad_frames += 1
                del self.buffer[0]
            else:
                del self.buffer[:size]
                frames.append(event)
        return frames


class Matcher:
    def __init__(self):
        self.pending = {"tx": defaultdict(deque), "rx": defaultdict(deque)}
        self.expired = {"tx": 0, "rx": 0}

    def expire(self, now_ns, max_age_ns=5_000_000_000):
        for side, groups in self.pending.items():
            for key in list(groups):
                queue = groups[key]
                while queue and now_ns - queue[0]["pc_ns"] > max_age_ns:
                    queue.popleft()
                    self.expired[side] += 1
                if not queue:
                    del groups[key]

    def add(self, side, event):
        other = "rx" if side == "tx" else "tx"
        key = event["raw"]  # Includes sequence, command, timestamp and CRC.
        waiting = self.pending[other].get(key)
        if waiting:
            paired = waiting.popleft()
            if not waiting:
                del self.pending[other][key]
            return (event, paired) if side == "tx" else (paired, event)
        self.pending[side][key].append(event)
        return None

    def counts(self):
        return {side: sum(map(len, groups.values())) for side, groups in self.pending.items()}

    def clear(self):
        for side, count in self.counts().items():
            self.expired[side] += count
            self.pending[side].clear()


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = (len(ordered) - 1) * fraction
    lo, hi = int(index), min(int(index) + 1, len(ordered) - 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (index - lo)


def main():
    import serial

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--remote", required=True, help="Remote USB serial, 9600 baud")
    parser.add_argument("--remote-baud", type=int, default=9600)
    parser.add_argument("--receiver", required=True, help="Wireless DAP master CDC, 115200 baud")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--warmup-seconds", type=float, default=3,
                        help="Drain pre-existing DAP/USB data before measuring")
    parser.add_argument("--drain-seconds", type=float, default=1,
                        help="Keep receiving after the TX observation window closes")
    parser.add_argument("--output", type=Path, default=Path("docs/validation/radio-latency-2026-10-13"))
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.warmup_seconds < 0 or args.drain_seconds < 0:
        parser.error("Warmup/drain durations must be nonnegative")
    if Path(args.remote).resolve() == Path(args.receiver).resolve():
        parser.error("Remote and receiver must be different serial devices")
    args.output.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    event_path = args.output / f"events-{stamp}.jsonl"
    csv_path = args.output / f"matched-{stamp}.csv"
    report_path = args.output / f"summary-{stamp}.json"
    decoder, matcher = FrameDecoder(), Matcher()
    ports, rx_buffer, samples = {}, bytearray(), []
    totals = {"tx": 0, "rx": 0, "rx_bad": 0}
    gaps = {"tx": 0, "rx": 0}
    duplicates = {"tx": 0, "rx": 0}
    resets = {"tx": 0, "rx": 0}
    previous = {}
    stats = []
    diagnostics = []
    captured_tx_raw = set()
    boundary_rx_ignored = 0
    started_ns = time.monotonic_ns()
    last_print = time.monotonic()

    def open_port(side, path, baud):
        uart = serial.Serial(port=None, baudrate=baud, timeout=0, exclusive=True)
        # Set levels before open. No reset pulse, no write into either radio UART.
        uart.dtr = True
        uart.rts = True
        uart.port = path
        uart.open()
        uart.reset_input_buffer()
        ports[side] = uart

    with event_path.open("w", encoding="utf-8") as events, csv_path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=(
            "version", "sequence", "command", "keys", "tx_ms", "rx_ms",
            "tx_pc_ms", "rx_pc_ms", "observed_delay_ms", "raw"
        ))
        writer.writeheader()

        def record(side, event, observed_ns):
            matcher.expire(observed_ns)
            event = dict(event, pc_ns=observed_ns, side=side)
            if side == "tx":
                captured_tx_raw.add(event["raw"])
            totals[side] += 1
            prior = previous.get(side)
            if prior and prior["version"] == event["version"]:
                bits = 8 if event["version"] == 1 else 16
                advance = (event["sequence"] - prior["sequence"]) & ((1 << bits) - 1)
                tx_reset = (event["tx_ms"] is not None and prior["tx_ms"] is not None
                            and event["tx_ms"] < prior["tx_ms"]
                            and prior["tx_ms"] - event["tx_ms"] < 0x80000000)
                if tx_reset:
                    resets[side] += 1
                    matcher.clear()  # Never match identical early packets across reboots.
                elif advance == 0:
                    duplicates[side] += 1
                elif advance < 1 << (bits - 1):
                    gaps[side] += advance - 1
                else:
                    resets[side] += 1  # Restart or out-of-order; not a huge loss count.
            previous[side] = event
            events.write(json.dumps(event) + "\n")
            pair = matcher.add(side, event)
            if pair:
                tx, rx = pair
                delay = (rx["pc_ns"] - tx["pc_ns"]) / 1e6
                sample = {key: tx[key] for key in ("version", "sequence", "command", "keys", "tx_ms", "raw")}
                sample.update(rx_ms=rx["rx_ms"], tx_pc_ms=(tx["pc_ns"] - started_ns) / 1e6,
                              rx_pc_ms=(rx["pc_ns"] - started_ns) / 1e6, observed_delay_ms=delay)
                samples.append(sample)
                writer.writerow(sample)
                if tx["command"]:
                    print(f"MATCH seq={tx['sequence']} {COMMANDS[tx['command']]} tx_ms={tx['tx_ms']} "
                          f"rx_ms={rx['rx_ms']} observed_delay={delay:.2f}ms motors=0", flush=True)

        try:
            open_port("tx", args.remote, args.remote_baud)
            open_port("rx", args.receiver, 115200)
            mapping = {uart.fileno(): side for side, uart in ports.items()}
            print(f"WARMUP {args.warmup_seconds}s: draining old data", flush=True)
            warmup_end = time.monotonic() + args.warmup_seconds
            while time.monotonic() < warmup_end:
                readable, _, _ = select.select(list(mapping), [], [], .02)
                for fd in readable:
                    side = mapping[fd]
                    data = ports[side].read(ports[side].in_waiting or 1)
                    if side == "tx":
                        decoder.feed(data)
                    else:
                        rx_buffer.extend(data)
                        while b"\n" in rx_buffer:
                            _, _, rx_buffer = rx_buffer.partition(b"\n")
            decoder.bad_frames = decoder.noise_bytes = 0
            started_ns = time.monotonic_ns()
            last_print = time.monotonic()
            deadline_ns = started_ns + int(args.seconds * 1e9)
            drain_deadline = deadline_ns / 1e9 + args.drain_seconds
            print(f"READY remote={args.remote}@{args.remote_baud} receiver={args.receiver}@115200 seconds={args.seconds}", flush=True)
            while time.monotonic() < drain_deadline:
                readable, _, _ = select.select(list(mapping), [], [], min(.02, max(0, drain_deadline - time.monotonic())))
                for fd in readable:
                    side = mapping[fd]
                    data = ports[side].read(ports[side].in_waiting or 1)
                    observed_ns = time.monotonic_ns()
                    if side == "tx":
                        for event in decoder.feed(data):
                            if observed_ns <= deadline_ns:
                                record("tx", event, observed_ns)
                    else:
                        rx_buffer.extend(data)
                        while b"\n" in rx_buffer:
                            raw_line, _, rx_buffer = rx_buffer.partition(b"\n")
                            line = parse_receiver_line(raw_line)
                            if line is None:
                                totals["rx_bad"] += 1
                                events.write(json.dumps({"side": "invalid_rx_log", "pc_ns": observed_ns, "raw": raw_line.hex()}) + "\n")
                                continue
                            match = RX_PATTERN.fullmatch(line)
                            if match:
                                version, seq, cmd, keys, tx_ms, rx_ms, raw = match.groups()
                                event = decode_hex_frame(raw)
                                if (event and
                                    (event["version"], event["sequence"], event["command"]) == (int(version), int(seq), int(cmd)) and
                                    (event["version"] == 1 or
                                     (event["keys"], event["tx_ms"]) == (int(keys), int(tx_ms)))):
                                    if observed_ns <= deadline_ns or event["raw"] in captured_tx_raw:
                                        record("rx", dict(event, rx_ms=int(rx_ms)), observed_ns)
                                    else:
                                        boundary_rx_ignored += 1
                                else:
                                    totals["rx_bad"] += 1
                            elif line.startswith("STAT "):
                                stats.append(line)
                                events.write(json.dumps({"side": "stat", "pc_ns": observed_ns, "line": line}) + "\n")
                            elif line.startswith("DIAG "):
                                diagnostics.append(line)
                                events.write(json.dumps({"side": "diag", "pc_ns": observed_ns, "line": line}) + "\n")
                            else:
                                events.write(json.dumps({"side": "text", "pc_ns": observed_ns, "line": line}) + "\n")
                if time.monotonic() - last_print >= 1:
                    matcher.expire(time.monotonic_ns())
                    print(f"TX={totals['tx']} RX={totals['rx']} matched={len(samples)} "
                          f"latest_delay={samples[-1]['observed_delay_ms']:.2f}ms" if samples else
                          f"TX={totals['tx']} RX={totals['rx']} matched=0", flush=True)
                    last_print = time.monotonic()
                    events.flush(); output.flush()
        except KeyboardInterrupt:
            print("Stopped by user", flush=True)
        finally:
            for uart in ports.values():
                uart.close()
            delays = [row["observed_delay_ms"] for row in samples]
            matched_rx_gaps = 0
            for prior, current in zip(samples, samples[1:]):
                if prior["version"] != current["version"]:
                    continue
                bits = 8 if current["version"] == 1 else 16
                advance = (current["sequence"] - prior["sequence"]) & ((1 << bits) - 1)
                if 0 < advance < 1 << (bits - 1):
                    matched_rx_gaps += advance - 1
            report = {
                "remote": args.remote, "receiver": args.receiver,
                "remote_baud": args.remote_baud,
                "tx_observation_seconds": args.seconds,
                "warmup_seconds": args.warmup_seconds, "drain_seconds": args.drain_seconds,
                "post_window_rx_excluded": boundary_rx_ignored,
                "measurement": "PC observation difference; includes JP13/DAP/USB; board uptimes not subtracted",
                "totals": totals, "matched": len(samples), "unmatched": matcher.counts(),
                "expired_unmatched": matcher.expired,
                "unmatched_note": "Unmatched observations include capture boundaries and telemetry loss; not pure RF loss",
                "sequence_gaps": gaps, "duplicates": duplicates, "restart_or_reorder": resets,
                "matched_window_rx_sequence_gaps": matched_rx_gaps,
                "sequence_gap_note": "Unfiltered rx gaps can include pre-capture DAP backlog; matched-window gaps exclude it",
                "remote_bad_frames": decoder.bad_frames, "remote_noise_bytes": decoder.noise_bytes,
                "negative_delays": sum(x < 0 for x in delays),
                "observed_delay_ms": {"min": min(delays) if delays else None,
                                      "p50": percentile(delays, .5), "p95": percentile(delays, .95),
                                      "max": max(delays) if delays else None},
                "commands_matched": sorted({row["command"] for row in samples}),
                "receiver_stats": stats, "events": str(event_path), "csv": str(csv_path),
                "receiver_diagnostics": diagnostics,
            }
            report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
            print("SUMMARY " + json.dumps(report, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
