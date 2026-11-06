#!/usr/bin/env python3
"""Offline FCAL audit: preserve button edges and expose motion assumptions.

Never opens a serial port, edits calibration coefficients, or accepts a fit.
Manifest entries: name, folder, kind, displacement_m, distance_source.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from fit_flow_calibration import fit, interval


def marker_pairs(markers):
    """First F2 starts a trial; repeated F2 never moves the start boundary."""
    start = None
    repeats = 0
    for marker in markers:
        if marker["key"] == 2:
            if start is None:
                start, repeats = marker, 0
            else:
                repeats += 1
        elif marker["key"] == 3 and start is not None:
            yield start, marker, repeats
            start = None
        elif marker["key"] == 4:
            start = None


def read_trial(spec, base):
    folder = base / spec["folder"]
    markers = [json.loads(line) for line in (folder / "keys.jsonl").read_text().splitlines()]
    samples = [json.loads(line) for line in (folder / "samples.jsonl").read_text().splitlines()]
    pairs = list(marker_pairs(markers))
    if len(pairs) != 1:
        raise ValueError(f"{spec['name']}: expected one explicit F2/F3 trial, got {len(pairs)}")
    first, last, repeats = pairs[0]
    if not first["snapshot_valid"] or not last["snapshot_valid"]:
        raise ValueError(f"{spec['name']}: invalid marker: {first['reason']}/{last['reason']}")
    start, end = first["snapshot"], last["snapshot"]
    counts, rotation, angles = interval(start, end)
    window = [s for s in samples if s["epoch"] == start["epoch"] and
              start["ms"] <= s["ms"] <= end["ms"] and
              first["host_seconds"] - .5 <= s["host_seconds"] <= last["host_seconds"] + .5]
    jumps = [(b["ms"] - a["ms"]) / 1000 - (b["host_seconds"] - a["host_seconds"])
             for a, b in zip(samples, samples[1:]) if b["ms"] >= a["ms"]]
    result = {
        "name": spec["name"], "kind": spec["kind"],
        "sensor_interval_valid": True, "physical_motion_verified": False,
        "declared_displacement_m": spec["displacement_m"],
        "distance_source": spec["distance_source"],
        "start_event": first["event"], "end_event": last["event"],
        "ignored_repeated_F2": repeats,
        "duration_s": (end["ms"] - start["ms"]) / 1000,
        "accepted_windows": end["n"] - start["n"],
        "bad_delta": end["bad"] - start["bad"],
        "counts_height": counts.tolist(), "rotation_height": rotation.tolist(),
        "gyro_integral_deg": (angles * 180 / np.pi).tolist(),
        "raw_count_delta_dx_dy": [end[k] - start[k] for k in ("dxsum", "dysum")],
        "range_start_end_mm": [start["mm"], end["mm"]],
        "range_minmax_mm": [min(s["mm"] for s in window), max(s["mm"] for s in window)] if window else None,
        "roll_pitch_delta_deg": [(end[k] - start[k]) / 100 for k in ("roll_cd", "pitch_cd")],
        "largest_device_vs_host_time_jump_s": max(jumps, default=0),
        "zero_translation_is_assumed": spec["displacement_m"] == [0, 0],
    }
    return result, dict(name=spec["name"], kind=spec["kind"],
                        displacement_m=spec["displacement_m"],
                        c=counts, b=rotation, angles=angles)


def audit(manifest, base):
    checks, trials = [], []
    for spec in manifest["trials"]:
        check, trial = read_trial(spec, base)
        checks.append(check)
        trials.append(trial)
    candidate = fit(trials)
    return {
        "status": "PARTIAL", "calibration_accepted": False,
        "marker_policy": "first F2 / first subsequent F3; repeated F2 ignored",
        "trials": checks, "candidate": candidate,
        "limitations": [
            "Sensor interval validity does not verify operator displacement or rotation pivot.",
            "Gyro Z integral is not an Euler yaw measurement.",
            "The fitter sums body-frame motion without per-sample yaw rotation.",
            "Hand-held rotations use an unverified zero horizontal translation assumption.",
            "A plausible scale and small training residual alone cannot accept calibration.",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = audit(json.loads(args.manifest.read_text()), args.manifest.parent)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"status": report["status"],
                      "calibration_accepted": False,
                      "radians_per_count": report["candidate"]["radians_per_count"],
                      "trials": report["candidate"]["trials"]}, indent=2))


if __name__ == "__main__":
    main()
