"""A disconnected UART must reopen without transmitting or joining fragments."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("flow_capture_host", root / "scripts/capture_flow_calibration.py")
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class Disconnected(OSError):
    pass


def frame(ms):
    values = {key: 0 for key in capture.SCALARS}
    values["ms"] = ms
    values.update(imu=1, tof=1, flow=1, valid=1, mm=200, output_locked=1)
    text = "[FCAL] " + " ".join(f"{key}={value}" for key, value in values.items())
    return (text + " angle_urad=0,0,0 hangle_umrad=0,0,0 PWM=0,0,0,0\n").encode()


with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)

    class Uart:
        def __init__(self, **kwargs):
            self.is_open = False
            self.opens = self.reads = self.writes = 0

        def open(self):
            self.opens += 1
            if self.opens == 1:
                raise Disconnected("simulated device not yet enumerated")
            self.is_open = True

        def close(self):
            self.is_open = False

        def reset_input_buffer(self):
            pass

        def write(self, data):
            self.writes += 1
            raise AssertionError("calibration capture must remain receive-only")

        def read(self, size):
            self.reads += 1
            if self.reads == 1:
                return frame(100) + b"KEY F2 event=1 seq=1 allowed=1 ready=1\n[FCAL] interrupted fragment"
            if self.reads == 2:
                raise Disconnected("simulated USB loss")
            if self.opens == 3:
                return b""  # USB exists, but its stream silently stopped.
            (folder / "STOP").touch()
            return (b"KEY F3 event=2 seq=2 allowed=1 ready=1\n" + frame(200) +
                    b"KEY F3 event=3 seq=3 allowed=1 ready=1\n")

    uart = Uart()
    transport = SimpleNamespace(Serial=lambda **kwargs: uart, SerialException=Disconnected)
    clock = [0.0]

    def monotonic():
        clock[0] += .05
        return clock[0]

    with patch.dict(sys.modules, {"serial": transport}), \
            patch.object(sys, "argv", ["capture", "--out-dir", directory, "--seconds", "30"]), \
            patch.object(capture.time, "monotonic", monotonic), \
            patch.object(capture.time, "sleep", lambda seconds: None):
        capture.main()

    rows = [json.loads(line) for line in (folder / "samples.jsonl").read_text().splitlines()]
    meta = json.loads((folder / "capture.json").read_text())
    assert [row["ms"] for row in rows] == [100, 200]
    assert [row["host_connection"] for row in rows] == [1, 3]
    assert len(meta["disconnects"]) == 2 and meta["connections"] == 3
    assert meta["disconnects"][1]["error"] == "no serial data for 3 seconds"
    assert meta["discarded_partial_bytes"] == len(b"[FCAL] interrupted fragment")
    assert meta["serial_released"] and not meta["connected"]
    assert meta["open_failures"] == 1
    assert uart.opens == 4 and uart.writes == 0 and not uart.is_open
    assert json.loads((folder / "latest.json").read_text())["ms"] == 200
    markers = [json.loads(line) for line in (folder / "keys.jsonl").read_text().splitlines()]
    assert [m["reason"] for m in markers] == ["ok", "no_recent_snapshot", "ok"]
    assert markers[0]["snapshot"]["ms"] == 100
    assert markers[2]["snapshot"]["ms"] == 200
    assert capture.key_marker("KEY F2 event=4 seq=4 allowed=0 ready=0", rows[-1], .1)["reason"] == "key_not_allowed"
    assert capture.key_marker("KEY F2 event=4 seq=4 allowed=1 ready=1", rows[-1], .6)["reason"] == "no_recent_snapshot"
    unlocked = dict(rows[-1], output_locked=0)
    assert capture.key_marker("KEY F2 event=4 seq=4 allowed=1 ready=1", unlocked, .1)["reason"] == "motors_not_locked"
    saturated = dict(rows[-1], sat=1)
    marker = capture.key_marker("KEY F2 event=4 seq=4 allowed=1 ready=1", saturated, .1)
    assert not marker["snapshot_valid"] and marker["reason"] == "accumulators_saturated"
    assert marker["diagnostic_snapshot"]["sat"] == 1
    low = dict(rows[-1], mm=42)
    marker = capture.key_marker("KEY F3 event=5 seq=5 allowed=1 ready=1", low, .1)
    assert not marker["snapshot_valid"] and marker["snapshot"] is None
    assert marker["not_ready_fields"] == ["range_below_80mm"]
    assert marker["diagnostic_snapshot"]["mm"] == 42
    print("PASS: UART loss reopens receive-only capture, discards partial framing, preserves samples and records connection evidence (simulated transport)")
