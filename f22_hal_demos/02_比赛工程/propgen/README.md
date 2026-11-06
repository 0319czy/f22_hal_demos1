# Propeller Generator — Python / build123d

A port of the OpenSCAD **Propeller Generator** (Orion Lord, after Jason Koolman's
examples and Alex Matulich's 2022 original) to Python using
[build123d](https://build123d.readthedocs.io) / OpenCascade. It generates fully
parametric propellers with NACA 4-digit airfoil blades, a hub, shaft bores,
spinners and shrouds, writes a watertight binary STL, and can drop the result
into a self-contained 3D viewer page.

New to some of these terms? A NACA 4-digit code (e.g. `8430`) describes an
airfoil's camber and thickness; STL is the mesh format 3D printers and slicers
read; build123d builds exact CAD solids rather than approximate meshes, which
is why the exported STL is watertight by construction.

License of the original design: **CC BY-NC-SA 4.0**
([creativecommons.org/licenses/by-nc-sa/4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)).

## Contents

- [Quick start](#quick-start)
- [Use](#use)
- [Presets](#presets)
- [Parameter groups](#parameter-groups)
- [How the port differs from the OpenSCAD version](#how-the-port-differs-from-the-openscad-version)
- [Notes](#notes)
- [Development](#development)

---

## Quick start

From the repo root:

```bash
./init.sh                    # provisions Python + uv via mise, then builds .venv with uv
python propeller.py          # generate propeller.stl using the default preset
```

No `uv run`, no `source .venv/bin/activate`: `.mise.toml` pins Python with
`virtualenv = ".venv"`, so once `.venv` exists on the right version, mise's
own shell hook (`mise activate`, already wired into your shell if you use
mise day to day) puts it on `PATH` automatically every time you're in this
directory — plain `python propeller.py` just works. If your shell doesn't
have the mise hook active, `uv run python propeller.py` and
`source .venv/bin/activate` both still work exactly as before.

One sharp edge worth knowing: this auto-activation only checks whether
`.venv` *exists*, not whether it's on the right Python version — it will
silently activate a stale `.venv` rather than catch the drift. Re-run
`./init.sh` after changing the Python pin in `.mise.toml`, and after any
`git pull` that might have changed it upstream — it's fast when nothing's
out of sync (well under a second), and it's the safe way back if something
seems off, since it always re-verifies the version and rebuilds if needed.

Run `./init.sh` at least once before relying on any of this. Before that
first run, every `cd` into this directory prints mise's own
`no venv found ... To create a virtualenv manually, run: python -m venv ...`
warning — ignore that suggestion and run `./init.sh` instead. The bare
`python -m venv` it suggests does create a `.venv` on the correctly-pinned
Python, but installs no dependencies — worse, doing that *silences the
warning* while leaving an environment that still doesn't work, removing the
only signal pointing you at `./init.sh`. Once `.venv` exists (via `./init.sh`),
plain `python propeller.py` resolves to the correctly-pinned 3.12 either
way — the failure mode without one is a clear `ModuleNotFoundError` (no
build123d installed), not a wrong Python version.

`init.sh` is a bash script (macOS/Linux/WSL). It needs [`mise`](https://mise.jdx.dev/),
which manages every other tool this project needs — including
[`uv`](https://docs.astral.sh/uv/), which it uses to create the virtual
environment and install packages. Python is pinned to 3.12 in `.mise.toml`;
uv tracks whatever `mise` considers latest. You don't install uv separately,
and if `mise` itself is missing, `init.sh` offers to install it for you via
`curl https://mise.run | sh` — accept the prompt, or install it yourself
first (that command; `brew install mise` also works if you prefer) and
re-run.

If you do install mise manually, its installer prints a one-line command to
hook it into your shell's rc file (`.zshrc`/`.bashrc`) — run that, then
restart your shell so `mise` is on `PATH` before running `./init.sh`.

Re-running `./init.sh` is safe — it reuses an existing `.venv` and just
re-syncs dependencies rather than recreating it. It also offers to run the
smoke test suite (see [Development](#development)) once setup finishes, so
you can confirm the environment actually works before relying on it.

`three.min.js` for the viewer ships alongside `viewer.py`, so generated pages
work offline.

## Use

```bash
python propeller.py                                   # default → auto-named .stl, see below
python propeller.py --preset pc_fan -o fan.stl
python propeller.py --blades 4 --diameter 200 --spinner ogive --shaft-type hex
python propeller.py --preset pullcopter -o p.stl --viewer p.html
python propeller.py --list-presets
```

(No mise shell hook? Prefix each with `uv run`, or `source .venv/bin/activate`
once — see [Quick start](#quick-start).)

### Output filenames

Without `-o`, the output filename is generated from the propeller's own
parameters using the standard RC (Radio Control) propeller convention —
diameter × pitch × blade count — with this generator's own tunables appended,
so a batch of parametric test STLs stay distinguishable at a glance without
opening each one:

```text
[Diameter]x[Pitch]x[Blades]_C[Chord]_S[Sweep]_B[Bore].stl
```

| Segment | Meaning | Example |
|---|---|---|
| `[Diameter]x[Pitch]x[Blades]` | The universal RC propeller format, in mm/(mm per rev)/count | `150x68x3` |
| `_C[Chord]` | `chord_fraction` × 100, no decimal point | `_C15` for 0.15 |
| `_S[Sweep]` | `sweep` × 100 (negative sweep keeps its sign) | `_S64` for 0.64°/mm, `_S-25` for -0.25°/mm |
| `_B[Bore]` | `shaft_diameter` in mm — one decimal is normal for shaft sizes; omitted entirely when `shaft_type` is `none`, since there's no bore | `_B6.5` |

`Pitch` resolves the same `-1 = auto` rule `build()` does (root blade length),
so the name always reflects the real geometry, never the sentinel value — e.g.
`python propeller.py` (all defaults) writes `150x68x3_C15_S0_B5.stl`.

If a file with the generated name already exists, a timestamp is appended
before recreating it (`150x68x3_C15_S0_B5_20260804-134323.stl`) rather than
silently overwriting a previous test iteration. This only applies to the
auto-generated name — an explicit `-o` always overwrites, same as any other
tool.

Every field of `PropellerConfig` is exposed as a `--kebab-case` flag, so the
whole OpenSCAD customizer is reachable from the command line — run
`python propeller.py --help` for the full, generated list. Two extra flags
control mesh resolution rather than geometry: `--quality` (STL chord
tolerance in mm, smaller = finer, default `0.02`) and `--viewer-quality`
(coarser tolerance for the `--viewer` preview mesh, default `0.12`).

```python
from propeller import PropellerConfig, build, save_stl
from viewer import write_viewer

cfg  = PropellerConfig(blades=4, diameter=180, pitch=140,
                       naca_root=8430, naca_tip=3412, spinner="ogive")
part = build(cfg)                       # a build123d Part (B-rep solid)
print(save_stl(part, "prop.stl"))       # {'triangles': …, 'watertight': True, …}
write_viewer("prop.stl", "prop.html")
```

## Presets

| preset       | what it is                                      |
| ------------ | ----------------------------------------------- |
| `default`    | 150 mm 3-blade, the script's stock settings     |
| `rc_plane`   | 120 mm trainer prop with an ogive spinner       |
| `airplane`   | 150 mm 3-blade, fixed 100 mm pitch              |
| `toy_boat`   | 80 mm wide-chord marine prop, tall hub          |
| `rb_plane`   | 80 mm 2-blade rubber-band prop                  |
| `pc_fan`     | 120 mm 7-blade back-swept fan, linear pitch law |
| `pullcopter` | 160 mm shrouded 3-blade with spinner            |

## Parameter groups

Same meaning as the OpenSCAD customizer — the fields carry their original
explanatory comments in the `PropellerConfig` class in `propeller.py`, and
`python propeller.py --help` prints the full, generated flag list.

- **Propeller** — `blades`, `diameter`, `direction` (1 = CW, −1 = CCW),
  `root_diameter`, `root_height`, `pitch` (mm/rev, −1 = auto)
- **Blade** — `chord_mode` (`ellipse`/`linear`/`constant`), `chord_fraction`,
  `ellipse_length`, `centerline` (0 = LE … 1 = TE; use 1.0 for a flat printable
  trailing edge), `fairing`, `sweep` (°/mm), `offset`, `pitch_tip`
- **Airfoils** — `naca_root`, `naca_mid`, `naca_tip`, `transition_point`,
  `trailing_thickness`
- **Hub** — `hub_extend`, `shaft_type` (`none`/`cylinder`/`keyway`/`hex`/`D`),
  `shaft_diameter`, `shaft_depth`, `shaft_chamfer`, `keyway_size`
- **Spinner** — `spinner` (`none`/`ogive`/`parabolic`), `spinner_on_hub`,
  `spinner_diameter`, `spinner_length`, `spinner_rounding`
- **Shroud** — `shroud`, `shroud_offset`, `shroud_wall`, `shroud_length`,
  `shroud_chamfer`
- **Render** — `points`, `slices`, `weld`

## How the port differs from the OpenSCAD version

**Blade construction.** OpenSCAD stitches the spanwise airfoil sections into a
polygon mesh with `vnf_vertex_array`. Here each section becomes a closed wire —
a B-spline from trailing edge round the nose and back, plus a straight segment
for the blunt trailing edge — and the sections are ruled-lofted into a real
B-rep solid. Since the sections are wrapped onto cylinders about the prop axis
they are not planar, so OCCT cannot cap the loft itself; the root and tip are
closed with filled surfaces and the whole thing is sewn into a solid
(`loft_sections`).

**Welding.** Every section lies *exactly* on the cylinder of its own radius, so
in the original the blade root merely touches the hub. CGAL tolerates that;
OCCT does not, and you get separate bodies. `weld` (default 0.2 mm) moves the
innermost section inside the hub — and the outermost one into the shroud when
one is present — so the union has real overlap to work with. The replaced
section keeps the original profile, so the visible geometry is unchanged.

**Volume.** `Shape.volume` uses OCCT's fixed-order Gauss integration, which is
tens of percent off on a body made of ~200 spline patches. `accurate_volume()`
uses the adaptive variant; it agrees with the mesh to within ~0.1% (the
mesh volume is itself only a tessellated approximation at the default
`--quality`, so this is the tessellation error, not integration error).

**Tessellation.** Going through `StlAPI` leaves cracks along edges shared by
boolean-split faces. `tessellate()` reads OCCT's triangulation directly and
welds coincident vertices, which produces watertight meshes for every preset.

**Bug fix.** `slicelen` in the original reads an undefined `length` when
`slices > 0`; here it is `blade_length / slices`.

Everything else — the NACA maths, cosine chord clustering, the chord/pitch/sweep
laws, the hub-height chord clamp via the ellipse-diameter trick, the root
fairing, the DIN 6885 keyway table, both spinner profiles — follows the original
formula for formula.

## Notes

- `pitch = -1` means "use the blade radius", exactly as in the original.
- `centerline = 1.0` keeps the blade above `z = 0` (flat trailing edge, best for
  printing). Lower values let the blade hang below the hub, matching the
  original's behaviour.
- Build time is a few seconds for a typical prop; wide-chord designs
  (`toy_boat`) take a minute or two because of the boolean load.
- Print in PETG/ABS/nylon, add perimeters, and test props safely.

## Development

```bash
python test_presets.py           # build every preset plus a shaft/spinner/chord-mode matrix
                                 # checks each result is one solid, watertight, and that its
                                 # mesh volume agrees with accurate_volume() to within 2%
python test_presets.py pc_fan    # restricts the preset loop to pc_fan — the shaft/spinner/
                                 # chord-mode matrix still runs unconditionally afterward
```

(No mise shell hook? `uv run python test_presets.py`, or activate first.)

There's no separate test framework — `test_presets.py` is a standalone script;
run it directly. It writes output STLs to `/tmp/_test_<name>.stl` and exits
non-zero on failure. Even the "one preset" form above still runs ~12 builds
(the coverage matrix isn't gated on the preset argument), so budget a minute,
not a few seconds.
