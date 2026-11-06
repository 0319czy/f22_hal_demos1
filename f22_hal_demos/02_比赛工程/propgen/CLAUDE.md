# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Python/build123d (OpenCascade) port of the OpenSCAD "Propeller Generator" (Orion Lord, after Jason
Koolman and Alex Matulich). Generates fully parametric propellers with NACA 4-digit airfoil blades, a
hub, shaft bore, optional spinner/shroud, and writes a watertight binary STL. The whole project is two
scripts (`propeller.py`, `viewer.py`) plus a smoke-test script — there is no package/build step.

Original design license: CC BY-NC-SA 4.0.

## Commands

```bash
# Setup: init.sh offers to install `mise` itself if missing (prompts before doing
# so; non-interactive runs just fail with an error). mise then provides both Python
# (pinned to 3.12, virtualenv = ".venv" so mise's shell hook auto-activates it on
# cd) and uv (tracks latest), per .mise.toml, which init.sh uses to build .venv.
# It also offers to run the smoke tests once setup finishes. Caveat: the
# auto-activation only checks whether .venv exists, not whether it matches the
# pinned version — silently activates a stale one otherwise. Re-run init.sh
# after changing the pin (or after any git pull that might have), not just
# when something seems off; don't rely on cd to catch drift. Before the first
# ./init.sh run, mise prints its own "no venv found ... python -m venv ..."
# warning on every cd — ignore it. That command does land on the correctly
# pinned Python, but skips the dependency install, and silences the warning
# in the process — leaving a .venv that looks set up but isn't, with no more
# signal telling you to run ./init.sh.
./init.sh

# Generate a propeller. .mise.toml pins python with `virtualenv = ".venv"`, so
# once .venv exists (init.sh above), mise's shell hook activates it automatically
# on cd — plain `python` just works. No mise hook active? Use `uv run python ...`
# or `source .venv/bin/activate` instead.
python propeller.py                                   # default preset -> propeller.stl
python propeller.py --preset pc_fan -o fan.stl
python propeller.py --blades 4 --diameter 200 --spinner ogive --shaft-type hex
python propeller.py --preset pullcopter -o p.stl --viewer p.html   # also emit a 3D viewer HTML
python propeller.py --list-presets

# Run all smoke tests (builds every preset + a shaft/spinner/chord-mode matrix, checks
# solids are single-body, watertight, and mesh volume agrees with accurate_volume() to <2%)
python test_presets.py

# Restricts the preset loop to one preset, but the shaft/spinner/chord-mode matrix still
# runs unconditionally afterward — not a fast single-case check
python test_presets.py pc_fan
```

There is no lint config, formatter config, or test framework (pytest etc.) in this repo —
`test_presets.py` is a plain script with its own pass/fail printer and process exit code; run it
directly rather than through a test runner.

## Architecture

### Two independent scripts, connected only through the STL file

- `propeller.py` — all geometry. Builds a build123d `Part` and writes STL (`save_stl`).
- `viewer.py` — unrelated to build123d/OCCT. Reads a binary STL and base64-embeds it into a
  single self-contained HTML file (`write_viewer`) using `three.min.js` (bundled locally next to
  `viewer.py`, falls back to a CDN `<script>` tag if the local file is missing). No dependency
  between the two beyond the STL bytes, so viewer changes never affect geometry and vice versa.

### Geometry pipeline (all in `propeller.py`)

1. **NACA airfoil math** (`naca_params`, `naca_camber`, `naca_gradient`, `naca_thickness`,
   `naca_surface`, `naca_profile`) — pure functions producing 2D airfoil coordinates from a 4-digit
   NACA code.
2. **Spanwise section generation** (`airfoil_section3d`, `blade_sections`) — places NACA profiles at
   each radial station, applying chord law (`chord_mode`: ellipse/linear/constant), pitch law,
   sweep, fairing near the root, and cosine point clustering. Returns a list of 3D point rings, one
   per spanwise slice.
3. **Solid construction** (`section_wire`, `loft_sections`) — each ring becomes a closed wire (a
   B-spline around the nose/trailing edge plus a straight segment for the blunt trailing edge), and
   the wires are ruled-lofted into a real B-rep solid. Root and tip are capped with filled surfaces
   since the sections sit on a cylinder and aren't planar (OCCT can't cap the loft itself).
4. **Primitives** (`cylinder`, `tube`, `parabolic_spinner`, `ogive_spinner`, `shaft_bore`,
   `_bore_profile`, `keyway_size`) — hub, shroud, spinner and shaft-bore solids built from 2D face
   profiles revolved/extruded.
5. **Assembly** (`build_blade`, `build`) — one blade is lofted then trimmed to the hub, then copied
   and rotated `cfg.blades` times, unioned with the hub, spinner and shroud, and finally the shaft
   bore is subtracted. `build()` is the entry point for constructing geometry; everything in steps
   1-4 is internal plumbing (export helpers in step 6 are public too — see below).
6. **Export** (`accurate_volume`, `tessellate`, `mesh_report`, `save_stl`) — `accurate_volume` calls
   OCP's `BRepGProp`/`GProp_GProps` directly with adaptive integration because build123d's
   `Shape.volume` uses fixed-order Gauss integration that is tens of percent off on ~200-spline-patch
   bodies. `tessellate` reads OCCT's triangulation directly and welds coincident vertices instead of
   going through `StlAPI`, because that path leaves cracks along edges shared by boolean-split faces
   — this is what makes the exported mesh watertight. `auto_filename(cfg)` builds the default output
   name from `cfg` when `-o` isn't given — standard RC (diameter x pitch x blades) format plus this
   generator's own `_C<chord*100>_S<sweep*100>_B<bore>` suffix; `unique_path()` appends a timestamp
   instead of overwriting if that name is already taken (only for the auto-generated name — an
   explicit `-o` always overwrites).

### Config and CLI

`PropellerConfig` (a dataclass) holds every OpenSCAD-customizer field, grouped by comment into
propeller / blade / airfoils / hub / spinner / shroud / render. `_build_argparser()` builds the CLI
by reflecting over `fields(PropellerConfig)` — every field automatically becomes a
`--kebab-case` flag with type inferred from its default value (bool/int/float/str). That
`if/elif` chain has no `else`: a field whose default is any other type (e.g. the
`Tuple[float, float]` of `keyway_size`) silently gets **no flag at all** unless special-cased —
which is why `keyway_size` has its own explicit `nargs=2` line right after the loop. So new
bool/int/float/str fields need no CLI wiring; any other type does.
`PRESETS` (near the end of the file) is a dict of named `PropellerConfig` instances used by both
`--preset` and `test_presets.py`; `main()` starts from a deep copy of the chosen preset and overrides
any field the user passed explicitly on the command line.

### Notable deliberate differences from the original OpenSCAD version

- OpenSCAD stitches sections into a polygon mesh (`vnf_vertex_array`); here they're lofted into a
  real B-rep solid (see pipeline step 3 above).
- `weld` (default 0.2mm) moves the innermost (and, if a shroud is present, outermost) section
  slightly into the hub/shroud so the boolean union has real overlap — in OpenSCAD/CGAL the blade
  merely touching the hub was tolerated, but OCCT produces separate bodies without it.
- The original's `slicelen` bug (reads an undefined `length` when `slices > 0`) is fixed here to
  `blade_length / slices`.
- `pitch = -1` still means "use the blade radius" and `centerline = 1.0` still keeps the blade above
  `z = 0` (flat, printable trailing edge) — both match the original's semantics exactly.
