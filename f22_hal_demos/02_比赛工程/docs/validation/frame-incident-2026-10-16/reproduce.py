#!/usr/bin/env python3
"""Offline forensic reproduction of failures in the WITHDRAWN key remap.

These assertions confirm unsafe legacy-sender behavior under the proposed map;
they are not acceptance tests for F3 emergency stopping and never touch hardware.
"""
from pathlib import Path
import subprocess
import tempfile

folder = Path(__file__).resolve().parent
root = folder.parents[2]
with tempfile.TemporaryDirectory(prefix="f22-incident-repro-") as temp:
    work = Path(temp)
    for name in ("repro.c", "repro_v3.c", "withdrawn-softland-radio.c"):
        (work / name).write_bytes((folder / (name + ".snapshot")).read_bytes())
    common = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic"]
    cases = [
        ("proto2", ["-DCL_F4_SOFT_LAND=1", "-I" + str(root / "src/flight"),
          str(work / "repro.c"), str(work / "withdrawn-softland-radio.c"),
          str(root / "src/flight/radio_trace.c"), str(root / "src/flight/radio.c")]),
        ("proto3", ["-I" + str(root / "remote/tle100"), str(work / "repro_v3.c"),
          str(root / "remote/tle100/key_events.c")]),
    ]
    for name, flags in cases:
        executable = work / name
        subprocess.run(common + flags + ["-o", str(executable)], check=True)
        result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
        (folder / (name + "-reproduced.log")).write_text(result.stdout)
        print(name + ": withdrawn F3-emergency remap defect reproduced (not a safety PASS)")
