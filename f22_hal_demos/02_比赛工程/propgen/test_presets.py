"""Smoke tests: every preset must build into one watertight solid.

    python test_presets.py            # all presets
    python test_presets.py pc_fan     # just one
"""

from __future__ import annotations

import sys
import time

from propeller import PRESETS, PropellerConfig, accurate_volume, build, save_stl


def check(name: str, cfg: PropellerConfig) -> bool:
    t0 = time.time()
    part = build(cfg)
    bodies = len(part.solids())
    report = save_stl(part, f"/tmp/_test_{name}.stl")
    vol = accurate_volume(part)
    mesh_vol = report["volume_mm3"]
    ok = (
        bodies == 1
        and report["watertight"]
        and abs(vol - mesh_vol) / max(vol, 1e-9) < 0.02
    )
    print(
        f"{'PASS' if ok else 'FAIL'}  {name:11s} {time.time() - t0:6.1f}s  "
        f"bodies={bodies}  watertight={report['watertight']}  "
        f"tris={report['triangles']:>7,}  "
        f"vol={vol / 1000:6.2f} cm3 (mesh {mesh_vol / 1000:6.2f})"
    )
    return ok


def main(argv):
    names = argv[1:] or sorted(PRESETS)
    results = [check(n, PRESETS[n]) for n in names]

    # extra coverage: every shaft and spinner variant
    base = dict(blades=2, diameter=60, root_diameter=16, root_height=8,
                shaft_diameter=6, hub_extend=3)
    for st in ("none", "cylinder", "hex", "D", "keyway"):
        results.append(check(f"shaft:{st}", PropellerConfig(shaft_type=st, **base)))
    for sp in ("ogive", "parabolic"):
        results.append(
            check(
                f"spin:{sp}",
                PropellerConfig(spinner=sp, spinner_diameter=20, spinner_length=18, **base),
            )
        )
    results.append(check("shroud", PropellerConfig(shroud=True, **base)))
    for cm in ("ellipse", "linear", "constant"):
        results.append(check(f"chord:{cm}", PropellerConfig(chord_mode=cm, **base)))

    print(f"\n{sum(results)}/{len(results)} passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
