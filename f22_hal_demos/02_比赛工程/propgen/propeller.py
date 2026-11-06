"""
Propeller Generator - Python / build123d port
=============================================

A faithful port of the OpenSCAD "Propeller Generator" by Orion Lord
(based on Jason Koolman's examples and the original work by Alex Matulich, 2022).

Generates fully parametric propellers with elliptical blades built from NACA
4-digit airfoil profiles, plus hub, shaft bore, spinner and shroud, and writes
the result to STL.

Original license: CC BY-NC-SA 4.0
  https://creativecommons.org/licenses/by-nc-sa/4.0/

------------------------------------------------------------------------------
Differences from the OpenSCAD original
------------------------------------------------------------------------------
* OpenSCAD builds the blade as a polygon mesh (``vnf_vertex_array``).  Here the
  same spanwise sections are converted into B-spline wires and lofted with
  OCCT, so the blade is a real B-rep solid (smooth surfaces, exact booleans).
* ``slicelen`` in the original reads an undefined variable when ``slices > 0``;
  here it correctly evaluates to ``blade_length / slices``.
* Everything else - the airfoil maths, the chord/pitch/sweep laws, the fairing,
  the hub-height chord clamp, the shaft bores, spinners and shroud - follows the
  original formula for formula.

------------------------------------------------------------------------------
Usage
------------------------------------------------------------------------------
    python propeller.py                        # default prop -> auto-named .stl (see auto_filename)
    python propeller.py --preset pc_fan        # a built-in example preset
    python propeller.py --blades 2 --diameter 200 --spinner ogive
    python propeller.py --list-presets
    python propeller.py -o prop.stl            # explicit path overrides auto-naming

    # from Python
    from propeller import PropellerConfig, build, save_stl
    cfg  = PropellerConfig(blades=4, diameter=180, spinner="ogive")
    part = build(cfg)
    save_stl(part, "prop.stl")
"""

from __future__ import annotations

import argparse
import os
import math
import time
from dataclasses import dataclass, fields
from typing import List, Sequence, Tuple

from build123d import (
    Axis,
    Edge,
    Face,
    Kind,
    Location,
    Part,
    Solid,
    Vector,
    Wire,
)

Point2 = Tuple[float, float]
Point3 = Tuple[float, float, float]

EPSILON = 1e-9


# ---------------------------------------------------------------------------
# small helpers (OpenSCAD-flavoured)
# ---------------------------------------------------------------------------


def clamp(x: float, lo: float, hi: float) -> float:
    """Clamp ``x`` to the range [lo, hi]."""
    return max(lo, min(hi, x))


def lerp(a: float, b: float, t: float) -> float:
    """Linear interpolation between ``a`` and ``b``."""
    return a + (b - a) * t


def _sign(x: float) -> float:
    return 1.0 if x > 0 else (-1.0 if x < 0 else 0.0)


def _openscad_range(start: float, step: float, stop: float) -> List[float]:
    """Reproduce OpenSCAD's ``[start : step : stop]`` list-comprehension range."""
    out: List[float] = []
    if step == 0:
        return out
    n = 0
    while True:
        v = start + n * step
        if step > 0 and v > stop + 1e-12:
            break
        if step < 0 and v < stop - 1e-12:
            break
        out.append(v)
        n += 1
        if n > 1_000_000:  # runaway guard
            break
    return out


# ---------------------------------------------------------------------------
# NACA 4-digit airfoil maths
# ---------------------------------------------------------------------------


def naca_params(code: int) -> Tuple[float, float, float]:
    """Convert a 4-digit NACA code into ``(M, P, T)`` as fractions of chord.

    * ``M`` - maximum camber
    * ``P`` - position of maximum camber
    * ``T`` - maximum thickness
    """
    if not 0 <= code <= 9999:
        raise ValueError("NACA code must be between 0000 and 9999")
    return (0.01 * (code // 1000), 0.1 * ((code // 100) % 10), 0.01 * (code % 100))


def naca_camber(x: float, M: float, P: float) -> float:
    """Camber line ordinate at chord fraction ``x``."""
    if M == 0:
        return 0.0
    Pc = clamp(P, EPSILON, 1 - EPSILON)
    a = 2 * Pc * x - x * x
    if x < Pc:
        return M * a / (Pc * Pc)
    return M * (1 - 2 * Pc + a) / ((1 - Pc) * (1 - Pc))


def naca_gradient(x: float, M: float, P: float) -> float:
    """Slope of the camber line at chord fraction ``x``."""
    if M == 0:
        return 0.0
    Pc = clamp(P, EPSILON, 1 - EPSILON)
    den = Pc if x < Pc else (1 - Pc)
    return (2 * M * (Pc - x)) / (den * den)


def naca_thickness(x: float, T: float) -> float:
    """Half-thickness distribution at chord fraction ``x`` (closed trailing edge)."""
    a0, a1, a2, a3, a4 = 0.2969, -0.126, -0.3516, 0.2843, -0.1036
    x2 = x * x
    return 5 * T * (a0 * math.sqrt(x) + a1 * x + a2 * x2 + a3 * x2 * x + a4 * x2 * x2)


def naca_surface(
    x: float,
    M: float,
    P: float,
    T: float,
    chordlen: float = 1.0,
    origin: float = 0.0,
    direction: int = 1,
    te_thick: float = 0.2,
    s: int = 1,
) -> Point2:
    """A single point on a NACA 4-digit surface.

    ``s`` selects the surface: ``1`` upper, ``-1`` lower.  ``x`` is a *parameter*
    in [0, 1] that is cosine-clustered towards the leading and trailing edges.
    """
    xp = 0.5 * (1 - math.cos(math.pi * x))  # cosine clustering
    theta = math.atan(naca_gradient(xp, M, P))
    yc = naca_camber(xp, M, P)
    yt = naca_thickness(xp, T)
    d = -1.0 if direction < 0 else 1.0
    org = clamp(origin, 0.0, 1.0)
    return (
        chordlen * d * (xp - s * yt * math.sin(theta) - org),
        chordlen * (yc + s * yt * math.cos(theta)) + s * xp * te_thick / 2,
    )


def naca_profile(
    n: int,
    M: float,
    P: float,
    T: float,
    chordlen: float = 1.0,
    origin: float = 0.0,
    direction: int = 1,
    te_thick: float = 0.2,
) -> List[Point2]:
    """Closed 2D airfoil outline: trailing edge -> nose -> trailing edge.

    ``n`` is the number of segments per side.  The returned list is *not*
    explicitly closed; the gap between the last and first point is the blunt
    trailing edge of thickness ``te_thick``.
    """
    n = int(n)
    desc = [1 - k / n for k in range(n)]  # x = 1.0 .. 1/n
    asc = [k / n for k in range(n)]  # x = 0 .. (n-1)/n
    first_s, second_s = (-1, 1) if direction < 0 else (1, -1)
    pts = [naca_surface(x, M, P, T, chordlen, origin, direction, te_thick, first_s) for x in desc]
    pts += [naca_surface(x, M, P, T, chordlen, origin, direction, te_thick, second_s) for x in asc]
    pts.append(naca_surface(1.0, M, P, T, chordlen, origin, direction, te_thick, second_s))
    return pts


# ---------------------------------------------------------------------------
# blade geometry helpers
# ---------------------------------------------------------------------------


def ellipse_d(majaxis: float, minaxis: float, angle_deg: float) -> float:
    """Diameter of an ellipse measured across its centre at ``angle_deg``."""
    a, b = 0.5 * majaxis, 0.5 * minaxis
    bc = b * math.cos(math.radians(angle_deg))
    asn = a * math.sin(math.radians(angle_deg))
    den = math.hypot(bc, asn)
    return 0.0 if den == 0 else 2 * a * b / den


def fairing_thickness(z: float, fairing: float, fairthickness: float) -> float:
    """Extra thickness blended into the blade root over the first ``fairing`` mm."""
    if fairing <= 0:
        return 0.0
    return fairthickness * (1 - math.sin(math.radians(90 * min(1.0, z / fairing))))


def attack_angle_r(
    r: float,
    direction: int,
    pitch: float,
    tspan: float,
    pitch_mode: str = "constant",
    pitch_tip: float | None = None,
) -> float:
    """Local blade angle (degrees) at radius ``r`` for the given pitch law."""
    pt = pitch if pitch_tip is None else pitch_tip
    p = (pitch + (pt - pitch) * tspan) if pitch_mode == "linear" else pitch
    return -direction * math.degrees(math.atan(p / (2 * math.pi * r)))


def chord_length(
    z: float,
    length: float,
    maxchord: float,
    hh: float,
    attackangle: float,
    esemimajor: float,
    mode: str = "ellipse",
) -> float:
    """Chord at span position ``z``, clamped by the hub-height envelope ``hh``."""
    t = clamp(z / length, 0.0, 1.0)
    if mode == "ellipse":
        c = maxchord * math.sqrt(max(0.0, 1 - (z * z) / (esemimajor * esemimajor)))
    elif mode == "constant":
        c = maxchord
    elif mode == "linear":
        c = lerp(maxchord, maxchord / 2, t)
    else:
        c = maxchord
    return ellipse_d(c, hh, attackangle)


def airfoil_section3d(
    p: Sequence[Point2], r: float, attackangle: float = 0.0, sweep: float = 0.0
) -> List[Point3]:
    """Rotate a 2D airfoil by its attack angle and wrap it around the prop axis.

    The section is laid onto the cylinder of radius ``r`` about the Y axis, so
    the chord follows the tangential direction and airfoil thickness runs along
    the (pre-rotation) prop axis.
    """
    ca = math.cos(math.radians(attackangle))
    sa = math.sin(math.radians(attackangle))
    ang0 = math.radians(r * sweep)
    out: List[Point3] = []
    for px, py in p:
        x2 = ca * px - sa * py
        y2 = sa * px + ca * py
        a = x2 / r + ang0
        out.append((r * math.sin(a), y2, r * math.cos(a)))
    return out


# ---------------------------------------------------------------------------
# spanwise section generation
# ---------------------------------------------------------------------------


def blade_sections(
    propdia: float = 100,
    hubdia: float = 10,
    bladepitch: float = -1,
    maxchordfrac: float = 0.15,
    hublen: float = 10,
    elenfrac: float = 1.1,
    direction: int = -1,
    centerline: float = 0.3,
    angle_sweep: float = 0.0,
    te_thickness: float = 0.4,
    fairing: float = 3.0,
    naca_root: int = 9430,
    naca_mid: int = 6412,
    naca_tip: int = 3412,
    root_transition: float = 0.33,
    pitch_mode: str = "constant",
    pitch_tip: float = 100,
    chord_mode: str = "ellipse",
    profilepoints: int = 0,
    slices: int = 0,
    root_weld: float = 0.2,
    tip_weld: float = 0.0,
) -> List[List[Point3]]:
    """Generate the ordered list of 3D blade cross-sections, root to tip."""
    if propdia <= hubdia:
        raise ValueError("Propeller diameter must be greater than hub diameter.")

    rootcamber, rootcamberpos, rootthickness = naca_params(naca_root)
    midcamber, midcamberpos, midthickness = naca_params(naca_mid)
    tipcamber, tipcamberpos, tipthickness = naca_params(naca_tip)

    rhub = hubdia / 2
    blength = propdia / 2 - rhub
    bpitch = bladepitch if bladepitch >= 0 else blength
    esemimajor = elenfrac * blength  # semi-major axis of planform ellipse
    maxchordlen = 2 * maxchordfrac * blength  # full minor axis of that ellipse
    hh = min(maxchordlen, hublen)  # hub height limit
    ztrans = blength * root_transition  # root -> mid transition
    blen = blength - ztrans
    slicelen = (blength / slices) if slices > 0 else 0.8

    if profilepoints > 0:
        airfoilsegs = int(profilepoints)
    else:
        airfoilsegs = int(round(2 * blength * maxchordfrac))
    airfoilsegs = max(6, airfoilsegs)

    root_attack = -direction * math.degrees(math.atan(bpitch / (2 * math.pi * rhub)))
    denom = ellipse_d(maxchordlen, hh, root_attack)
    fairingthick = (fairing / denom) if denom else 0.0

    sweep = angle_sweep * _sign(direction)

    sections: List[List[Point3]] = []
    radii: List[float] = []
    ends: List[Tuple[List[Point2], float]] = []  # (profile, attack) for first/last

    def emit(rz: float, cam, campos, thick, attack, chordlen):
        prof = naca_profile(
            airfoilsegs, cam, campos, thick, chordlen, centerline, direction, te_thickness
        )
        sections.append(airfoil_section3d(prof, rz, attack, sweep))
        radii.append(rz)
        if len(ends) < 1:
            ends.append((prof, attack))
        elif len(ends) == 1:
            ends.append((prof, attack))
        else:
            ends[1] = (prof, attack)

    # ---- root section: hub -> transition ---------------------------------
    if ztrans > 0:
        for z in _openscad_range(0.0, slicelen, max(0.0, ztrans - EPSILON)):
            rz = rhub + z
            interp = math.sin(math.radians(90 * z / ztrans))
            fthick = fairing_thickness(z, fairing, fairingthick)
            thick = lerp(rootthickness, midthickness, interp) + fthick
            cam = lerp(rootcamber, midcamber, interp)
            campos = lerp(rootcamberpos, midcamberpos, interp)
            attack = attack_angle_r(
                rz, direction, bpitch, z / blength, pitch_mode, pitch_tip
            )
            cl = chord_length(z, blength, maxchordlen, hh, attack, esemimajor, chord_mode)
            emit(rz, cam, campos, thick, attack, cl)

    # ---- main section: transition -> tip ---------------------------------
    for bz in _openscad_range(0.0, slicelen, blen + 0.9 * slicelen):
        z = min(bz, blen)
        ze = ztrans + z
        rz = rhub + ze
        interp = 1 - math.cos(math.radians(90 * z / blen)) if blen > 0 else 0.0
        fthick = fairing_thickness(ze, fairing, fairingthick)
        thick = lerp(midthickness, tipthickness, interp) + fthick
        cam = lerp(midcamber, tipcamber, interp)
        campos = lerp(midcamberpos, tipcamberpos, interp)
        attack = attack_angle_r(rz, direction, bpitch, ze / blength, pitch_mode, pitch_tip)
        cl = chord_length(ze, blength, maxchordlen, hh, attack, esemimajor, chord_mode)
        emit(rz, cam, campos, thick, attack, cl)

    # ---- drop duplicate / sliver sections (OCCT dislikes them) -----------
    min_gap = max(1e-4, 0.02 * slicelen)
    cleaned: List[List[Point3]] = []
    last_r = None
    for r, sec in zip(radii, sections):
        if last_r is not None and (r - last_r) < min_gap:
            cleaned[-1] = sec  # keep the outermost of the pair
            last_r = r
            continue
        cleaned.append(sec)
        last_r = r

    # ---- weld allowances -------------------------------------------------
    # Every section lies exactly on the cylinder of radius rz, so the blade root
    # only *touches* the hub (and the tip only touches a shroud) - the union
    # would leave separate bodies.  Two things are needed:
    #   1. an extra section buried inside the hub / shroud, so there is real
    #      overlap for the boolean to work with;
    #   2. *replacing* (not keeping) the section that sat exactly on the hub /
    #      shroud surface, because OCCT cannot cleanly intersect a solid along a
    #      curve that is also an edge of its own shell.  Moving the section
    #      inside means the surface now crosses the hub mid-patch.
    if root_weld > 0 and len(cleaned) > 2:
        w = min(root_weld, 0.25 * rhub)
        prof, attack = ends[0]
        cleaned[0] = airfoil_section3d(prof, rhub - w, attack, sweep)
    if tip_weld > 0 and len(cleaned) > 2:
        rtip = rhub + blength
        prof, attack = ends[-1]
        cleaned[-1] = airfoil_section3d(prof, rtip + tip_weld, attack, sweep)
    return cleaned


# ---------------------------------------------------------------------------
# section -> B-rep
# ---------------------------------------------------------------------------


def section_wire(pts: Sequence[Point3]) -> Wire:
    """Closed wire for one blade section: a spline nose-to-tail plus a flat TE."""
    vs = [Vector(*p) for p in pts]
    edges = [Edge.make_spline(vs)]
    if (vs[-1] - vs[0]).length > 1e-7:
        edges.append(Edge.make_line(vs[-1], vs[0]))
    return Wire(edges)


def loft_sections(sections: Sequence[Sequence[Point3]]) -> Solid:
    """Ruled loft through all blade sections, capped at root and tip.

    The section wires are not planar (they are wrapped onto cylinders about the
    prop axis), so OCCT's ThruSections cannot close them itself.  The lateral
    surface is lofted first and the two ends are then closed with filled
    (non-planar) faces before sewing everything into a solid.
    """
    from build123d import Shell

    wires = [section_wire(s) for s in sections]
    lateral = Solid.make_loft(wires, ruled=True)
    faces = list(lateral.faces())
    faces += [Face.make_surface(wires[0]), Face.make_surface(wires[-1])]
    solid = Solid(Shell(faces))
    if solid.volume <= 0:
        raise RuntimeError("Blade loft produced an invalid solid.")
    return solid


# ---------------------------------------------------------------------------
# primitives (ported from BOSL2 usage in the original)
# ---------------------------------------------------------------------------


def _face_from_pts(pts: Sequence[Point2]) -> Face:
    """Planar face in the XY plane from a closed polygon."""
    vs = [Vector(x, y, 0) for x, y in pts]
    if (vs[-1] - vs[0]).length > 1e-9:
        vs.append(vs[0])
    return Face(Wire([Edge.make_line(vs[i], vs[i + 1]) for i in range(len(vs) - 1)]))


def _circle_face(r: float) -> Face:
    return Face(Wire([Edge.make_circle(r)]))


def _regular_polygon_face(circumradius: float, n: int, rotate_deg: float = 0.0) -> Face:
    pts = []
    for i in range(n):
        a = math.radians(rotate_deg + 360.0 * i / n)
        pts.append((circumradius * math.cos(a), circumradius * math.sin(a)))
    return _face_from_pts(pts)


def _rect_face(x0: float, y0: float, x1: float, y1: float) -> Face:
    return _face_from_pts([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])


def _offset_face(face: Face, amount: float, kind: Kind = Kind.INTERSECTION) -> Face:
    """2D outward offset of a planar face."""
    from build123d import offset as bd_offset

    res = bd_offset(face, amount=amount, kind=kind)
    return res.faces()[0] if hasattr(res, "faces") else res


def cylinder(d: float, h: float, z0: float = 0.0) -> Solid:
    """Plain cylinder with its base at ``z0``."""
    return Solid.make_cylinder(d / 2, h).move(Location((0, 0, z0)))


def tube(inner_d: float, wall: float, length: float, chamfer: float = 0.0) -> Part:
    """Ring (shroud) standing on z=0, chamfered on all four rim edges."""
    outer = Solid.make_cylinder(inner_d / 2 + wall, length)
    inner = Solid.make_cylinder(inner_d / 2, length + 2)
    ring = Part() + outer - inner.move(Location((0, 0, -1)))
    if chamfer > 0:
        edges = ring.edges().filter_by(Axis.Z, reverse=True)
        try:
            from build123d import chamfer as bd_chamfer

            ring = bd_chamfer(edges, length=chamfer)
        except Exception:
            pass
    return ring


def parabolic_spinner(length: float = 24, d: float = 13, rounding: float = 0.5) -> Solid:
    """Parabolic nose cone: pointy at ``rounding=0``, blunt at ``rounding=1``."""
    r = d / 2
    exponent = rounding * 1.5 + 0.5
    pts: List[Point2] = [(0.0, 0.0)]
    for x in _openscad_range(0.0, 0.05, 1.001):
        x = min(x, 1.0)
        pts.append((x * r, length * (1 - math.pow(x, exponent) * math.pow(x, exponent))))
    profile = _face_from_pts(pts)
    return _revolve_xy_profile(profile)


def ogive_spinner(length: float = 20, d: float = 20, nose_r: float = 0.20) -> Solid:
    """Secant-ogive nose cone with a rounded tip of radius ``nose_r * d``."""
    if nose_r >= 0.25:
        raise ValueError("Nose radius must be less than 0.25.")
    rnose = nose_r * d
    r = 0.5 * d - rnose
    ht = length - rnose
    x = (ht * ht - r * r) / (2 * r)
    circrad = x + r
    astart = math.degrees(math.atan(ht / x))
    pts: List[Point2] = [(0.0, rnose)]
    for a in _openscad_range(astart, -0.05 * astart, -0.001):
        pts.append(
            (circrad * math.cos(math.radians(a)) - x, circrad * math.sin(math.radians(a)))
        )
    profile = _offset_face(_face_from_pts(pts), rnose, kind=Kind.ARC)
    # trim to the first quadrant (x >= 0, y >= 0), as the original does with
    # two subtracted squares
    big = max(length, d) * 4
    quadrant = _rect_face(0, 0, big, big)
    trimmed = profile.intersect(quadrant)
    face = trimmed.faces().sort_by(lambda f: f.area)[-1]
    return _revolve_xy_profile(face)


def _revolve_xy_profile(face: Face) -> Solid:
    """Revolve a profile drawn in XY (x = radius, y = height) about the Z axis."""
    rotated = face.rotate(Axis.X, 90)  # XY profile -> XZ plane
    return Solid.revolve(rotated, 360, Axis.Z)


# ---------------------------------------------------------------------------
# shaft bores
# ---------------------------------------------------------------------------


def keyway_size(diameter: float) -> Point2:
    """Standard keyway ``(width, depth)`` for a shaft diameter (DIN 6885 / ISO 773)."""
    table = [
        (6, (2, 1.0)), (8, (2, 1.2)), (10, (3, 1.4)), (12, (3, 1.8)),
        (17, (4, 2.3)), (22, (5, 2.8)), (30, (6, 3.3)), (38, (8, 3.3)),
        (44, (10, 4.3)), (50, (12, 4.3)), (58, (14, 4.9)), (65, (16, 5.4)),
        (75, (18, 6.4)), (85, (20, 6.4)), (95, (22, 7.4)), (110, (25, 8.4)),
    ]
    for limit, size in table:
        if diameter <= limit:
            return size
    return (25, 8.4)


def _bore_profile(bore_type: str, size: float, chamfer: float, kw_override: Point2) -> Face:
    """2D cross-section of the shaft bore, in the XY plane."""
    if bore_type == "cylinder":
        return _circle_face(size / 2)

    if bore_type == "hex":
        return _regular_polygon_face(size / math.sqrt(3), 6)

    if bore_type == "D":
        flat_depth = size * 0.15
        circle = _circle_face(size / 2)
        y_top = -size / 2 + flat_depth
        res = circle.intersect(_rect_face(-size, y_top, size, size))
        return res.faces().sort_by(lambda f: f.area)[-1]

    if bore_type == "keyway":
        kw = keyway_size(size)
        kw_width = min(size, kw_override[0] if kw_override[0] > 0 else kw[0])
        kw_height = min(kw_width, kw_override[1] if kw_override[1] > 0 else kw[1])
        circle = _circle_face(size / 2)
        slot = _rect_face(-kw_width / 2, 0.0, kw_width / 2, size / 2 + kw_height)
        slot = slot.rotate(Axis.Z, 90)  # zrot(90) as in the original
        res = circle.fuse(slot).clean()
        return res.faces().sort_by(lambda f: f.area)[-1]

    raise ValueError(f"Unknown shaft type: {bore_type}")


def shaft_bore(
    bore_type: str,
    size: float,
    height: float,
    chamfer: float,
    chamfer_top: bool,
    kw_override: Point2 = (0, 0),
    shiftout: float = 0.01,
) -> Part:
    """Cutting tool for the shaft bore, standing on z=0.

    A *negative* chamfer in the original BOSL2 code flares the bore outwards at
    the ends, which is what makes the printed hole self-guiding; that is what
    the lofted transitions below reproduce.
    """
    from build123d import loft as bd_loft

    profile = _bore_profile(bore_type, size, chamfer, kw_override)
    h = height + shiftout
    body = Part() + Solid.extrude(profile, (0, 0, h))

    if chamfer > 0:
        flare = _offset_face(profile, chamfer)
        # bottom flare: wide at z=0, nominal at z=chamfer
        body += bd_loft(
            [flare.moved(Location((0, 0, 0))), profile.moved(Location((0, 0, chamfer)))],
            ruled=True,
        )
        if chamfer_top:
            body += bd_loft(
                [
                    profile.moved(Location((0, 0, h - chamfer))),
                    flare.moved(Location((0, 0, h))),
                ],
                ruled=True,
            )
    return body.move(Location((0, 0, -shiftout)))


# ---------------------------------------------------------------------------
# configuration
# ---------------------------------------------------------------------------


@dataclass
class PropellerConfig:
    """Every knob from the OpenSCAD customizer, same names/defaults (snake_case)."""

    # --- propeller ---
    blades: int = 3
    diameter: float = 150.0
    direction: int = -1  # 1 = clockwise, -1 = counter-clockwise
    root_diameter: float = 14.0
    root_height: float = 8.0
    pitch: float = -1.0  # mm/rev, -1 = auto (= blade length)

    # --- blade ---
    chord_mode: str = "ellipse"  # ellipse | linear | constant
    chord_fraction: float = 0.15
    ellipse_length: float = 1.1
    centerline: float = 1.0  # 0 = leading edge, 1 = trailing edge
    fairing: float = 3.0
    sweep: float = 0.0  # degrees per mm of radius
    offset: float = 0.0  # axial shift of the blades
    pitch_tip: float = -1.0  # -1 = same as base pitch

    # --- airfoils ---
    naca_root: int = 8430
    naca_mid: int = 6412
    naca_tip: int = 3412
    transition_point: float = 0.33
    trailing_thickness: float = 0.4

    # --- hub ---
    hub_extend: float = 2.0
    shaft_type: str = "cylinder"  # none | cylinder | keyway | hex | D
    shaft_diameter: float = 5.0
    shaft_depth: float = -1.0  # -1 = full hub height
    shaft_chamfer: float = 0.4
    keyway_size: Tuple[float, float] = (0.0, 0.0)

    # --- spinner ---
    spinner: str = "none"  # none | ogive | parabolic
    spinner_on_hub: bool = True
    spinner_diameter: float = 20.0
    spinner_length: float = 20.0
    spinner_rounding: float = 0.5

    # --- shroud ---
    shroud: bool = False
    shroud_offset: float = 0.0
    shroud_wall: float = 1.8
    shroud_length: float = 10.0
    shroud_chamfer: float = 0.4

    # --- render ---
    points: int = 0  # airfoil points per side, 0 = auto
    slices: int = 0  # spanwise slices, 0 = auto (0.8 mm)
    weld: float = 0.2  # mm of blade buried in the hub (and shroud) so the union welds

    @property
    def hub_diameter(self) -> float:
        return self.root_diameter

    @property
    def hub_length(self) -> float:
        return self.root_height + self.hub_extend


# ---------------------------------------------------------------------------
# assembly
# ---------------------------------------------------------------------------


def build_blade(cfg: PropellerConfig) -> Part:
    """One blade, already rotated into propeller coordinates (axis = +Z)."""
    pitch_tip = cfg.pitch if cfg.pitch_tip < 0 else cfg.pitch_tip
    pitch_mode = "constant" if cfg.pitch_tip < 0 else "linear"

    sections = blade_sections(
        propdia=cfg.diameter,
        hubdia=cfg.root_diameter,
        bladepitch=cfg.pitch,
        maxchordfrac=cfg.chord_fraction,
        hublen=cfg.root_height,
        elenfrac=cfg.ellipse_length,
        direction=cfg.direction,
        centerline=cfg.centerline,
        angle_sweep=cfg.sweep,
        te_thickness=cfg.trailing_thickness,
        fairing=cfg.fairing,
        naca_root=cfg.naca_root,
        naca_mid=cfg.naca_mid,
        naca_tip=cfg.naca_tip,
        root_transition=cfg.transition_point,
        pitch_mode=pitch_mode,
        pitch_tip=pitch_tip,
        chord_mode=cfg.chord_mode,
        profilepoints=cfg.points,
        slices=cfg.slices,
        root_weld=cfg.weld,
        tip_weld=cfg.weld if cfg.shroud else 0.0,
    )
    if len(sections) < 2:
        raise ValueError("Not enough blade sections - check diameter/slices settings.")

    solid = loft_sections(sections)
    blade = Part() + solid.rotate(Axis.X, 90)  # prop axis Y -> Z

    # trim anything protruding below the bottom of the hub
    shift = cfg.root_height * (1 - clamp(cfg.centerline, 0.0, 1.0))
    span = cfg.diameter
    trim = Solid.make_box(2 * span, 2 * span, span).move(
        Location((-span, -span, -shift - span))
    )
    return (blade - trim).clean()


def build(cfg: PropellerConfig | None = None, verbose: bool = False) -> Part:
    """Build the complete propeller part."""
    cfg = cfg or PropellerConfig()
    t0 = time.time()

    blade = build_blade(cfg)
    if verbose:
        print(f"  blade lofted            {time.time() - t0:6.1f}s")

    pieces = [
        blade.rotate(Axis.Z, 360.0 * i / cfg.blades).move(Location((0, 0, cfg.offset)))
        for i in range(cfg.blades)
    ]
    pieces.append(Part() + cylinder(cfg.hub_diameter, cfg.hub_length))

    if cfg.shroud:
        pieces.append(
            tube(
                inner_d=cfg.diameter + cfg.shroud_offset,
                wall=cfg.shroud_wall,
                length=cfg.shroud_length,
                chamfer=cfg.shroud_chamfer,
            )
        )

    result = Part()
    for piece in pieces:
        result += piece
    if verbose:
        print(f"  {cfg.blades} blades + hub fused     {time.time() - t0:6.1f}s")

    if cfg.spinner != "none":
        on_hub = cfg.spinner_on_hub
        sd = min(cfg.spinner_diameter, cfg.hub_diameter) if on_hub else cfg.spinner_diameter
        if cfg.spinner == "parabolic":
            sp = parabolic_spinner(cfg.spinner_length, sd, cfg.spinner_rounding)
        elif cfg.spinner == "ogive":
            sp = ogive_spinner(cfg.spinner_length, sd, cfg.spinner_rounding * 0.24)
        else:
            raise ValueError(f"Unknown spinner type: {cfg.spinner}")
        result += sp.move(Location((0, 0, cfg.hub_length if on_hub else 0)))

    result = result.clean()
    if verbose:
        print(f"  spinner + cleanup       {time.time() - t0:6.1f}s")

    if cfg.shaft_type != "none":
        depth = cfg.shaft_depth if cfg.shaft_depth > 0 else cfg.hub_length
        chamfer_top = not (
            cfg.shaft_depth > 0 or (cfg.spinner != "none" and cfg.spinner_on_hub)
        )
        bore = shaft_bore(
            cfg.shaft_type,
            cfg.shaft_diameter,
            depth,
            cfg.shaft_chamfer,
            chamfer_top,
            tuple(cfg.keyway_size),
        )
        result = (result - bore).clean()
    if verbose:
        print(f"  shaft bore cut          {time.time() - t0:6.1f}s")

    return result


def accurate_volume(shape) -> float:
    """Volume in mm^3, integrated adaptively.

    ``Shape.volume`` uses OCCT's fixed-order Gauss integration, which is off by
    tens of percent on a body made of ~200 ruled spline patches.  This uses the
    adaptive variant instead.
    """
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps

    props = GProp_GProps()
    BRepGProp.VolumeProperties_s(shape.wrapped, props, 1e-5)
    return props.Mass()


def tessellate(shape, tolerance: float = 0.02, angular_tolerance: float = 0.1):
    """Triangulate a shape into ``(vertices, faces)`` numpy arrays.

    Reading OCCT's own triangulation (rather than going through ``StlAPI``)
    keeps the per-face meshes stitched along shared edges, which is what makes
    the exported mesh watertight.
    """
    import numpy as np
    from OCP.BRep import BRep_Tool
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepTools import BRepTools
    from OCP.IMeshTools import IMeshTools_Parameters
    from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopLoc import TopLoc_Location
    from OCP.TopoDS import TopoDS

    obj = shape.wrapped if hasattr(shape, "wrapped") else shape
    BRepTools.Clean_s(obj)
    params = IMeshTools_Parameters()
    params.Deflection = tolerance
    params.Angle = angular_tolerance
    params.InParallel = True
    BRepMesh_IncrementalMesh(obj, params)

    verts: List[Tuple[float, float, float]] = []
    faces: List[Tuple[int, int, int]] = []
    exp = TopExp_Explorer(obj, TopAbs_FACE)
    while exp.More():
        face = TopoDS.Face_s(exp.Current())
        loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(face, loc)
        if tri is not None:
            xf = loc.Transformation()
            off = len(verts)
            for i in range(1, tri.NbNodes() + 1):
                p = tri.Node(i).Transformed(xf)
                verts.append((p.X(), p.Y(), p.Z()))
            reversed_ = face.Orientation() == TopAbs_REVERSED
            for i in range(1, tri.NbTriangles() + 1):
                a, b, c = tri.Triangle(i).Get()
                faces.append(
                    (off + a - 1, off + c - 1, off + b - 1)
                    if reversed_
                    else (off + a - 1, off + b - 1, off + c - 1)
                )
        exp.Next()

    v = np.asarray(verts, dtype=np.float64)
    f = np.asarray(faces, dtype=np.int64)
    # weld coincident vertices coming from adjacent faces
    key = np.round(v, 6)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    v = v[first]
    f = inv.reshape(-1)[f]
    f = f[(f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])]
    return v, f


def mesh_report(vertices, faces) -> dict:
    """Cheap manifold check: every edge must be shared by exactly two triangles."""
    import numpy as np

    e = np.vstack([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    e = np.sort(e, axis=1)
    _, counts = np.unique(e, axis=0, return_counts=True)
    tri = vertices[faces]
    vol = float(
        np.abs(np.einsum("ij,ij->i", tri[:, 0], np.cross(tri[:, 1], tri[:, 2])).sum()) / 6.0
    )
    return {
        "triangles": int(len(faces)),
        "vertices": int(len(vertices)),
        "open_edges": int((counts != 2).sum()),
        "watertight": bool((counts == 2).all()),
        "volume_mm3": vol,
    }


def save_stl(part: Part, path: str = "propeller.stl", tolerance: float = 0.02,
             angular_tolerance: float = 0.1) -> dict:
    """Tessellate and write a binary STL file; returns the mesh report."""
    import struct

    import numpy as np

    v, f = tessellate(part, tolerance, angular_tolerance)
    tri = v[f]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    n = np.divide(n, ln, out=np.zeros_like(n), where=ln > 0)
    data = np.zeros((len(f), 4, 3), dtype=np.float32)
    data[:, 0] = n
    data[:, 1:] = tri
    with open(path, "wb") as fh:
        fh.write(b"propeller.py (build123d)".ljust(80, b"\0"))
        fh.write(struct.pack("<I", len(f)))
        blob = np.zeros(len(f), dtype=[("v", "<f4", (4, 3)), ("attr", "<u2")])
        blob["v"] = data
        fh.write(blob.tobytes())
    return mesh_report(v, f)


def auto_filename(cfg: PropellerConfig) -> str:
    """RC-style descriptive filename: ``DIAxPITCHxBLADES_C{chord*100}_S{sweep*100}[_B{bore}].stl``

    Standard RC propeller convention (diameter x pitch x blade count) with this
    generator's own tunables appended, so a batch of parametric test STLs stay
    distinguishable at a glance without opening each one:

    * ``C<n>`` - ``chord_fraction`` * 100 (e.g. 0.22 -> ``C22``)
    * ``S<n>`` - ``sweep`` * 100 (e.g. 0.64 -> ``S64``; negative sweep keeps its sign)
    * ``B<n>`` - ``shaft_diameter`` in mm, one decimal is normal (e.g. ``B6.5``);
      omitted entirely when ``shaft_type`` is ``"none"``, since there's no bore

    ``pitch`` resolves the same "-1 = auto" rule ``build_blade()`` uses (root
    blade length) so the name always reflects the real geometry, never the
    sentinel value.
    """
    blade_length = cfg.diameter / 2 - cfg.root_diameter / 2
    pitch = cfg.pitch if cfg.pitch >= 0 else blade_length
    name = (
        f"{cfg.diameter:g}x{pitch:g}x{cfg.blades}"
        f"_C{round(cfg.chord_fraction * 100)}_S{round(cfg.sweep * 100)}"
    )
    if cfg.shaft_type != "none":
        name += f"_B{cfg.shaft_diameter:g}"
    return name + ".stl"


def unique_path(path: str) -> str:
    """Return ``path`` unchanged, or with a timestamp inserted before the extension if it exists."""
    if not os.path.exists(path):
        return path
    root, ext = os.path.splitext(path)
    return f"{root}_{time.strftime('%Y%m%d-%H%M%S')}{ext}"


# ---------------------------------------------------------------------------
# example presets (ported from the OpenSCAD example modules)
# ---------------------------------------------------------------------------

PRESETS = {
    "default": PropellerConfig(),
    "rc_plane": PropellerConfig(
        blades=3, diameter=120, root_diameter=10, root_height=12, hub_extend=0,
        pitch=-1, chord_fraction=0.15, ellipse_length=1.1, direction=-1,
        centerline=0.3, sweep=0, trailing_thickness=0.4, fairing=4,
        naca_root=9430, naca_mid=6412, naca_tip=4412, transition_point=0.33,
        pitch_tip=-1, spinner="ogive", spinner_on_hub=True, spinner_diameter=10,
        spinner_length=8, shaft_type="cylinder", shaft_diameter=4,
    ),
    "airplane": PropellerConfig(
        blades=3, diameter=150, root_diameter=12, root_height=10, hub_extend=0,
        pitch=100, chord_fraction=0.15, ellipse_length=1.10, direction=-1,
        centerline=0.3, sweep=0, trailing_thickness=0.4, fairing=2,
        naca_root=6412, naca_mid=4412, naca_tip=2412, transition_point=0.33,
        pitch_tip=-1, shaft_type="cylinder", shaft_diameter=4,
    ),
    "toy_boat": PropellerConfig(
        blades=3, diameter=80, root_diameter=12, root_height=24, hub_extend=0,
        pitch=60, chord_fraction=0.70, ellipse_length=1.00, direction=1,
        centerline=0.5, sweep=1.0, trailing_thickness=0.4, fairing=0,
        naca_root=6410, naca_mid=4410, naca_tip=2410, transition_point=0.40,
        pitch_tip=-1, shaft_type="cylinder", shaft_diameter=4,
    ),
    "rb_plane": PropellerConfig(
        blades=2, diameter=80, root_diameter=3, root_height=6, hub_extend=0,
        pitch=70, chord_fraction=0.50, ellipse_length=1.00, direction=1,
        centerline=0.5, sweep=0, trailing_thickness=0.3, fairing=0,
        naca_root=4410, naca_mid=3410, naca_tip=2410, transition_point=0.25,
        pitch_tip=-1, shaft_type="cylinder", shaft_diameter=1.5, shaft_chamfer=0.2,
    ),
    "pc_fan": PropellerConfig(
        blades=7, diameter=120, root_diameter=40, root_height=10, hub_extend=0,
        pitch=40, pitch_tip=30, chord_fraction=0.28, ellipse_length=1.8,
        direction=1, centerline=0.7, sweep=-0.25, trailing_thickness=0.3,
        fairing=0, naca_root=12, naca_mid=12, naca_tip=10,
        transition_point=0.30, shaft_type="cylinder", shaft_diameter=8,
    ),
    "pullcopter": PropellerConfig(
        blades=3, diameter=160, root_diameter=16, root_height=6, hub_extend=0,
        pitch=160, chord_fraction=0.2, ellipse_length=1.2, direction=1,
        centerline=1, sweep=-0.25, trailing_thickness=0.4, fairing=4,
        naca_root=9440, naca_mid=6412, naca_tip=3412, transition_point=0.33,
        pitch_tip=-1, spinner="ogive", spinner_on_hub=True, spinner_diameter=18,
        spinner_length=10, shaft_type="cylinder", shaft_diameter=3,
        shroud=True, shroud_offset=0, shroud_wall=1.8, shroud_length=8,
    ),
}


# ---------------------------------------------------------------------------
# command line
# ---------------------------------------------------------------------------


def _build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Generate a parametric propeller and write it to STL.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("-o", "--output", default=None,
                   help="output STL path (default: auto-named from the propeller's "
                        "own parameters, see auto_filename())")
    p.add_argument("--preset", default="default", choices=sorted(PRESETS), help="starting preset")
    p.add_argument("--list-presets", action="store_true", help="list presets and exit")
    p.add_argument("--viewer", metavar="HTML", default=None,
                   help="also write a self-contained 3D viewer page")
    p.add_argument("--quality", type=float, default=0.02,
                   help="STL chord tolerance in mm (smaller = finer)")
    p.add_argument("--viewer-quality", type=float, default=0.12,
                   help="coarser tolerance for the embedded preview mesh")

    base = PropellerConfig()
    skip = {"keyway_size"}
    for f in fields(base):
        if f.name in skip:
            continue
        default = getattr(base, f.name)
        if f.type == "bool" or isinstance(default, bool):
            p.add_argument(f"--{f.name.replace('_', '-')}", dest=f.name,
                           type=lambda s: s.lower() in ("1", "true", "yes", "on"),
                           default=None, metavar="BOOL")
        elif isinstance(default, int) and not isinstance(default, bool):
            p.add_argument(f"--{f.name.replace('_', '-')}", dest=f.name, type=int, default=None)
        elif isinstance(default, float):
            p.add_argument(f"--{f.name.replace('_', '-')}", dest=f.name, type=float, default=None)
        elif isinstance(default, str):
            p.add_argument(f"--{f.name.replace('_', '-')}", dest=f.name, type=str, default=None)
    p.add_argument("--keyway-size", nargs=2, type=float, default=None, metavar=("W", "H"))
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_argparser().parse_args(argv)

    if args.list_presets:
        for name in sorted(PRESETS):
            print(name)
        return 0

    import copy

    cfg = copy.deepcopy(PRESETS[args.preset])
    for f in fields(cfg):
        v = getattr(args, f.name, None)
        if v is not None:
            setattr(cfg, f.name, tuple(v) if f.name == "keyway_size" else v)

    print(f"Building '{args.preset}' propeller: "
          f"{cfg.blades} blades, D={cfg.diameter} mm, pitch={cfg.pitch}")
    t0 = time.time()
    part = build(cfg, verbose=True)
    print(f"Solid built in {time.time() - t0:.1f}s   "
          f"volume = {accurate_volume(part) / 1000:.2f} cm^3, "
          f"{len(part.solids())} body/bodies")

    output = args.output or unique_path(auto_filename(cfg))
    report = save_stl(part, output, tolerance=args.quality)
    print(f"Wrote {output}: {report['triangles']:,} triangles, "
          f"watertight={report['watertight']}")
    if not report["watertight"]:
        print(f"  ! {report['open_edges']} open edges - check the parameters")

    if args.viewer:
        try:
            from viewer import write_viewer
        except ImportError:  # pragma: no cover
            from .viewer import write_viewer  # type: ignore

        preview = os.path.splitext(args.viewer)[0] + "_preview.stl"
        save_stl(part, preview, tolerance=args.viewer_quality, angular_tolerance=0.35)
        write_viewer(preview, args.viewer, title=f"Propeller - {args.preset}")
        os.remove(preview)
        print(f"Wrote {args.viewer}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
