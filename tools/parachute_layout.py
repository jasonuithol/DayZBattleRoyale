"""Shared by blender/build_parachute.py (the model) and tools/make_parachute_textures.py
(the texture), so the texture's cells, seams and trim land where the model's UVs put them.

The canopy: a ram-air parafoil of CELLS cells, SPAN metres along the arc, CHORD metres
front to back. Its section is a cambered airfoil (rounded nose, thin trailing edge); the
top skin runs from the trailing edge over the top and round the nose to TOP_LIP_X on the
underside, the bottom skin from BOTTOM_LIP_X back to the trailing edge, and between the
two lips each cell is open (the cell mouth).

Texture atlas (ATLAS x ATLAS px), regions in Blender UV (u right, v up):
  TOP       top skin: u = across the span (left tip 0, right tip 1), v = along the
            profile from the trailing edge (0) over the nose to the top lip (1)
  BOTTOM    bottom skin: u across the span, v from the bottom lip (0) to the trailing edge (1)
  INTERIOR  the insides of the cells, seen through the mouths: u across the span,
            v = chord position x / INTERIOR_DEPTH
  TIP       end ribs and stabilisers (outside faces): u = chord x, v = height
  LINE, RISER  suspension lines and riser webbing (u, v over the rect)
"""

import math

CELLS = 9
SPAN = 9.0              # m, along the arc
CHORD = 3.5             # m
THICKNESS = 0.17        # of the chord
CAMBER = 0.03
CAMBER_POS = 0.30
TOP_LIP_X = 0.012       # top skin wraps round the nose to here on the underside
BOTTOM_LIP_X = 0.12     # bottom skin starts here
INTERIOR_DEPTH = 0.72   # cells are dark-walled to this far back (a back wall closes them)
LINE_X = (0.14, 0.35, 0.60, 0.86)   # A, B, C, D line attachment points on each rib

ATLAS = 2048
# (u0, v0, u1, v1)
TOP = (0.0, 0.625, 1.0, 1.0)
BOTTOM = (0.0, 0.3125, 1.0, 0.625)
INTERIOR = (0.0, 0.1875, 1.0, 0.3125)
TIP = (0.0, 0.0, 0.5, 0.1875)
LINE = (0.5, 0.0, 0.625, 0.1875)
RISER = (0.625, 0.0, 0.75, 0.1875)
PAD = 3 / ATLAS          # keep UVs this far inside a region (mipmap bleeding)

ORANGE = (226, 86, 22)
WHITE = (236, 231, 218)
TRIM = (38, 40, 46)


def cell_colour(i):
    """Cells alternate from the tips in: orange at both tips and in the middle."""
    return ORANGE if i % 2 == 0 else WHITE


def camber_line(x):
    m, p = CAMBER, CAMBER_POS
    if x < p:
        return m / p ** 2 * (2 * p * x - x * x)
    return m / (1 - p) ** 2 * ((1 - 2 * p) + 2 * p * x - x * x)


def half_thickness(x):
    """NACA 4-digit thickness, closed trailing edge, scaled to THICKNESS."""
    x = max(0.0, min(1.0, x))
    return 5 * THICKNESS * (0.2969 * math.sqrt(x) - 0.1260 * x - 0.3516 * x ** 2
                            + 0.2843 * x ** 3 - 0.1036 * x ** 4)


def profile(x, upper):
    """(x, y) of the section in chord fractions, x from the nose (0) to the trailing edge (1)."""
    y = camber_line(x) + (half_thickness(x) if upper else -half_thickness(x))
    return (x, y)


def _cos_spacing(n, x0=0.0, x1=1.0):
    return [x0 + (x1 - x0) * 0.5 * (1 - math.cos(math.pi * i / (n - 1))) for i in range(n)]


# Top skin stations, trailing edge first: over the top to the nose, then round it to the lip.
_upper = _cos_spacing(19)          # 0 .. 1, dense at the nose
TOP_STATIONS = [(x, True) for x in reversed(_upper)] + [(0.004, False), (TOP_LIP_X, False)]
# Bottom skin stations, lip first, including the line attachment points
BOTTOM_STATIONS = [(x, False) for x in (BOTTOM_LIP_X,) + LINE_X[:1] + (0.24,) + LINE_X[1:2]
                   + (0.47,) + LINE_X[2:3] + (0.73,) + LINE_X[3:] + (1.0,)]


def _arclen(stations):
    pts = [profile(x, up) for x, up in stations]
    out = [0.0]
    for a, b in zip(pts, pts[1:]):
        out.append(out[-1] + math.dist(a, b) * CHORD)
    return out


TOP_ARC = _arclen(TOP_STATIONS)          # metres from the trailing edge, per station
BOTTOM_ARC = _arclen(BOTTOM_STATIONS)    # metres from the bottom lip, per station
TOP_LENGTH = TOP_ARC[-1]
BOTTOM_LENGTH = BOTTOM_ARC[-1]


def top_arc_at(x, upper):
    """Metres along the top skin (from the trailing edge) of a point on it."""
    for i in range(len(TOP_STATIONS) - 1):
        (xa, ua), (xb, ub) = TOP_STATIONS[i], TOP_STATIONS[i + 1]
        if ua == upper == ub and min(xa, xb) <= x <= max(xa, xb):
            t = (x - xa) / (xb - xa) if xb != xa else 0.0
            return TOP_ARC[i] + t * (TOP_ARC[i + 1] - TOP_ARC[i])
    raise ValueError(x)


def bottom_arc_at(x):
    for i in range(len(BOTTOM_STATIONS) - 1):
        xa, xb = BOTTOM_STATIONS[i][0], BOTTOM_STATIONS[i + 1][0]
        if xa <= x <= xb:
            t = (x - xa) / (xb - xa)
            return BOTTOM_ARC[i] + t * (BOTTOM_ARC[i + 1] - BOTTOM_ARC[i])
    raise ValueError(x)


def region_uv(region, fu, fv):
    """A point in a region (fractions 0..1 across it) to atlas UV, kept PAD inside."""
    u0, v0, u1, v1 = region
    fu = max(0.0, min(1.0, fu))
    fv = max(0.0, min(1.0, fv))
    return (u0 + PAD + (u1 - u0 - 2 * PAD) * fu, v0 + PAD + (v1 - v0 - 2 * PAD) * fv)


def region_px(region):
    """(left, top, right, bottom) pixel box of a region in the image (rows from the top)."""
    u0, v0, u1, v1 = region
    return (round(u0 * ATLAS), round((1 - v1) * ATLAS), round(u1 * ATLAS), round((1 - v0) * ATLAS))
