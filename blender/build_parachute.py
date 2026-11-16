"""Procedurally builds the parachute: a ram-air (square, steerable) parafoil on its
suspension lines and risers, for attaching to a player.

Run headless (from the project root):
    blender --background --python blender/build_parachute.py

Outputs:
    BattleRoyale/data/parachute.p3d   MLOD P3D (needs the Arma 3 Object Builder add-on):
                                      LOD 1 (detailed) and LOD 2 (distance), visual only:
                                      no geometry, fire geometry or memory LODs, so it
                                      collides with nothing
    BattleRoyale/data/model.cfg       class parachute, no skeleton
    models/parachute.blend            source scene, one collection per LOD (plus Preview)
    models/parachute_*.png            preview renders (textured from textures/*.png, which
                                      tools/make_parachute_textures.py makes)

Axes: Blender X/Y/Z = game X/Z/Y. Origin = the jumper's feet; forward (where the jumper
faces and flies) is +Y here, +Z in game. The risers come down to the shoulders at
x = +-0.22, 1.45 m up.

The canopy: CELLS cells over SPAN m of arc, CHORD m deep, a cambered airfoil section
(tools/parachute_layout.py, shared with the texture), with top and bottom skins pillowing
out between the ribs, open cell mouths at the nose (dark inside walls behind them),
a scalloped trailing edge, stabiliser panels hanging under the tips, A-D lines on every
rib, brake lines from the outer trailing edge through a cascade to the rear of the risers.
Everything is one material (one texture atlas), so it's one draw call.
"""

import math
import os
import sys

import addon_utils
import bmesh
import bpy
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import parachute_layout as L  # noqa: E402

OUT_DIR = os.path.join(ROOT, "models")
DATA_DIR = os.path.join(ROOT, "BattleRoyale", "data")   # PBO prefix BattleRoyale
TEX_DIR = os.path.join(ROOT, "textures")
CANOPY_TEX = ("BattleRoyale\\data\\parachute_co.paa", "BattleRoyale\\data\\parachute.rvmat")

# --- shape ---------------------------------------------------------------------------
TIP_ANGLE = math.radians(46)    # slope of the canopy at the tips (anhedral)
ARC_LINEAR = 0.6                # 1 = a circular arc; less = flatter middle, curlier tips
PITCH = math.radians(8)         # nose-down trim
CANOPY_LOW_Z = 7.65             # underside of the canopy centre, m above the feet
CP_X = 0.33                     # this point of the chord sits right over the shoulders
TOP_PILLOW = 0.075              # m, how far the top skin bulges between ribs
BOTTOM_PILLOW = 0.04
SCALLOP = 0.055                 # m, trailing edge pulled forward between ribs
INSET = 0.012                   # inside walls this far in from the skins
SPAN_SEGS_TOP, SPAN_SEGS_BOTTOM = 4, 2

SHOULDER = (0.22, 0.0, 1.45)
RISER_TOP = (0.25, 0.0, 1.97)
RISER_W, RISER_T = 0.045, 0.007     # webbing width (fore-aft) and thickness
LINE_R = 0.0045                     # suspension line "radius" (triangular prism)
BRAKE_R = 0.0055

A3OB_MODULE = "bl_ext.user_default.Arma3ObjectBuilder"
LOD_VISUAL, LOD_GEOMETRY = "0", "6"

MODEL_CFG = '''class CfgSkeletons
{
	class Default
	{
		isDiscrete = 1;
		skeletonInherit = "";
		skeletonBones[] = {};
	};
};
class CfgModels
{
	class Default
	{
		sectionsInherit = "";
		sections[] = {};
		skeletonName = "";
	};
	class parachute: Default {};
};
'''

S2 = L.SPAN / 2
CELL_W = L.SPAN / L.CELLS


def smoothstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# --- the canopy's spanwise centre line (arc-length parametrised) ---------------------

def slope(s):
    """Tangent angle (down-going on the right) at arc length s from the centre."""
    u = s / S2
    return TIP_ANGLE * (ARC_LINEAR * u + (1 - ARC_LINEAR) * u ** 3)


_STEPS = 3000
_CURVE = [(0.0, 0.0)]
for _i in range(_STEPS):
    _s0, _s1 = S2 * _i / _STEPS, S2 * (_i + 1) / _STEPS
    _a = slope((_s0 + _s1) / 2)
    _x, _z = _CURVE[-1]
    _CURVE.append((_x + math.cos(_a) * (_s1 - _s0), _z - math.sin(_a) * (_s1 - _s0)))


def curve(s):
    """(x, z) of the centre line at arc length s (before placing the canopy)."""
    f = abs(s) / S2 * _STEPS
    i = min(int(f), _STEPS - 1)
    t = f - i
    x = _CURVE[i][0] + (_CURVE[i + 1][0] - _CURVE[i][0]) * t
    z = _CURVE[i][1] + (_CURVE[i + 1][1] - _CURVE[i][1]) * t
    return (math.copysign(x, s), z)


def frame(s):
    """Span tangent T, and chord-forward F and section-up U at s (with the pitch)."""
    a = slope(s)
    T = Vector((math.cos(a), 0, -math.sin(a)))
    N = Vector((math.sin(a), 0, math.cos(a)))
    Y = Vector((0, 1, 0))
    F = Y * math.cos(PITCH) - N * math.sin(PITCH)
    U = Y * math.sin(PITCH) + N * math.cos(PITCH)
    return T, F, U


# --- the section ---------------------------------------------------------------------

def station_t(x, upper):
    """Parameter round the profile: t = -1 trailing edge (top), 0 nose, +1 trailing edge
    (underside). x = t^2, which keeps the rounded nose smooth."""
    return -math.sqrt(x) if upper else math.sqrt(x)


def prof_t(t):
    return L.profile(t * t, t < 0)


def prof_normal(t):
    """Outward normal of the profile (in chord fractions: x back, y up) at t."""
    e = 1e-4
    t0, t1 = max(-1.0, t - e), min(1.0, t + e)
    if t0 < 0 <= t1 and t != 0:          # don't straddle the nose
        t0, t1 = (t - e, t) if t < 0 else (t, t + e)
    a, b = prof_t(t0), prof_t(t1)
    tx, ty = b[0] - a[0], b[1] - a[1]
    n = math.hypot(tx, ty)
    return (ty / n, -tx / n)              # profile runs counter-clockwise: outward = right


class Placement:
    origin = Vector((0, 0, 0))   # where the centre line's s=0 point sits


def section_point(s, x, y):
    """3D point of section coordinates (x back from the nose, y up; chord fractions) at s."""
    cx, cz = curve(s)
    _, F, U = frame(s)
    base = Placement.origin + Vector((cx, 0, cz))
    return base + F * ((0.25 - x) * L.CHORD) + U * (y * L.CHORD)


def section_vec(s, dx, dy):
    _, F, U = frame(s)
    return -F * dx + U * dy


def place_canopy():
    """Put the centre so the underside is CANOPY_LOW_Z up and CP_X is over the shoulders."""
    Placement.origin = Vector((0, 0, 0))
    low = min(section_point(0.0, x, L.profile(x, False)[1]).z for x in [i / 50 for i in range(51)])
    cp = section_point(0.0, CP_X, L.camber_line(CP_X))
    Placement.origin = Vector((0, -cp.y, CANOPY_LOW_Z - low))


def envelope(x):
    """How much the skins pillow along the chord: most at the nose, none at the trailing edge."""
    return (0.35 + 0.65 * (1 - x) ** 0.7) * (1 - smoothstep(0.72, 1.0, x))


def skin_point(s, x, upper, sigma, pillow):
    """A point on a skin: the section at s, pillowed out by sigma (0..1 across the cell)."""
    t = station_t(x, upper)
    px, py = prof_t(t)
    p = section_point(s, px, py)
    nx, ny = prof_normal(t)
    b = math.sin(math.pi * sigma)
    p += section_vec(s, nx, ny) * (pillow * envelope(px) * b)
    _, F, _ = frame(s)
    p += F * (SCALLOP * b * smoothstep(0.82, 1.0, px))
    return p, section_vec(s, nx, ny).normalized()


# --- mesh building ------------------------------------------------------------------

class Mesh:
    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")

    def face(self, verts, uvs, want, smooth=True):
        f = self.bm.faces.new(verts)
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv
        f.normal_update()
        if f.normal.dot(want) < 0:
            f.normal_flip()
        f.smooth = smooth
        return f

    def grid(self, pts, uvs, wants, smooth=True):
        """pts[i][j] Vectors, uvs[i][j], wants[i][j] the side each face should face."""
        verts = [[self.bm.verts.new(p) for p in row] for row in pts]
        for i in range(len(pts) - 1):
            for j in range(len(pts[0]) - 1):
                w = wants[i][j] + wants[i + 1][j] + wants[i + 1][j + 1] + wants[i][j + 1]
                self.face((verts[i][j], verts[i + 1][j], verts[i + 1][j + 1], verts[i][j + 1]),
                          (uvs[i][j], uvs[i + 1][j], uvs[i + 1][j + 1], uvs[i][j + 1]), w, smooth)
        return verts

    def polygon(self, pts, uvs, want, smooth=False):
        verts = [self.bm.verts.new(p) for p in pts]
        return self.face(verts, uvs, want, smooth)

    def prism(self, a, b, r, uv_rect, sides=3):
        """A thin line from a to b: an open prism, smooth so it reads round."""
        axis = (b - a).normalized()
        side = axis.cross(Vector((0, 0, 1)))
        if side.length < 1e-3:
            side = axis.cross(Vector((1, 0, 0)))
        side.normalize()
        up = axis.cross(side)
        ring_a, ring_b = [], []
        for k in range(sides):
            ang = 2 * math.pi * k / sides
            off = (side * math.cos(ang) + up * math.sin(ang)) * r
            ring_a.append(self.bm.verts.new(a + off))
            ring_b.append(self.bm.verts.new(b + off))
        u0, v0, u1, v1 = uv_rect
        for k in range(sides):
            k2 = (k + 1) % sides
            fu0, fu1 = k / sides, (k + 1) / sides
            mid = (ring_a[k].co + ring_a[k2].co) / 2 - a
            self.face((ring_a[k], ring_a[k2], ring_b[k2], ring_b[k]),
                      (L.region_uv(uv_rect, fu0, 0), L.region_uv(uv_rect, fu1, 0),
                       L.region_uv(uv_rect, fu1, 1), L.region_uv(uv_rect, fu0, 1)), mid)

    def to_object(self, name, material):
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:], quad_method="BEAUTY", ngon_method="BEAUTY")
        mesh = bpy.data.meshes.new(name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        obj = bpy.data.objects.new(name, mesh)
        mesh.materials.append(material)
        return obj


def span_u(s):
    return (s + S2) / L.SPAN


def rib_positions():
    return [-S2 + i * CELL_W for i in range(L.CELLS + 1)]


def top_uv(s, x, upper):
    return L.region_uv(L.TOP, span_u(s), L.top_arc_at(x, upper) / L.TOP_LENGTH)


def bottom_uv(s, x):
    a = L.bottom_arc_at(x) if x >= L.BOTTOM_LIP_X else 0.0
    return L.region_uv(L.BOTTOM, span_u(s), a / L.BOTTOM_LENGTH)


def interior_uv(s, x):
    return L.region_uv(L.INTERIOR, span_u(s), x / L.INTERIOR_DEPTH)


def build_skins(m, top_stations, bottom_stations, segs_top, segs_bottom):
    """Top and bottom skins, cell by cell (separate vertices: creased seams at the ribs)."""
    ribs = rib_positions()
    for c in range(L.CELLS):
        s0, s1 = ribs[c], ribs[c + 1]
        for stations, segs, pillow, uvf in (
                (top_stations, segs_top, TOP_PILLOW, lambda s, x, up: top_uv(s, x, up)),
                (bottom_stations, segs_bottom, BOTTOM_PILLOW, lambda s, x, up: bottom_uv(s, x))):
            pts, uvs, wants = [], [], []
            for k in range(segs + 1):
                sigma = k / segs
                s = s0 + (s1 - s0) * sigma
                row, rowuv, roww = [], [], []
                for x, up in stations:
                    p, n = skin_point(s, x, up, sigma, pillow)
                    row.append(p)
                    rowuv.append(uvf(s, x, up))
                    roww.append(n)
                pts.append(row)
                uvs.append(rowuv)
                wants.append(roww)
            m.grid(pts, uvs, wants)


def build_cell_insides(m):
    """Inside walls of every cell from the mouth back to INTERIOR_DEPTH, a back wall there,
    and hems closing the skins' cut edges at the mouth, so looking into a mouth shows a
    dark, closed cell rather than sky through back-faces."""
    ribs = rib_positions()
    depth = L.INTERIOR_DEPTH
    upper = [x for x, up in L.TOP_STATIONS if up and x < depth - 0.02]
    top_in = [(depth, True)] + [(x, True) for x in upper if x > 0] + [(0.0, True)] \
        + [(x, up) for x, up in L.TOP_STATIONS if not up]
    top_in = sorted(set(top_in), key=lambda st: station_t(*st))
    bot_in = [(x, False) for x, _ in L.BOTTOM_STATIONS if x < depth - 0.02] + [(depth, False)]
    for c in range(L.CELLS):
        s0, s1 = ribs[c], ribs[c + 1]
        walls = {}
        for name, stations, pillow, segs_out in (("top", top_in, TOP_PILLOW, SPAN_SEGS_TOP),
                                                 ("bottom", bot_in, BOTTOM_PILLOW, SPAN_SEGS_BOTTOM)):
            pts, uvs, wants = [], [], []
            for k in range(3):
                sigma = k / 2
                s = s0 + (s1 - s0) * sigma
                row, rowuv, roww = [], [], []
                for x, up in stations:
                    p, n = skin_point(s, x, up, sigma, pillow)
                    row.append(p - n * INSET)
                    rowuv.append(interior_uv(s, x))
                    roww.append(-n)
                pts.append(row)
                uvs.append(rowuv)
                wants.append(roww)
            walls[name] = m.grid(pts, uvs, wants)
            # Hem: the skin's cut edge at the mouth, outer edge to inside wall
            lip_x, lip_up = (stations[-1] if name == "top" else stations[0])
            j = len(stations) - 1 if name == "top" else 0
            inner = [walls[name][k][j] for k in range(3)]
            outer = []
            for k in range(segs_out + 1):
                sigma = k / segs_out
                p, _ = skin_point(s0 + (s1 - s0) * sigma, lip_x, lip_up, sigma, pillow)
                outer.append(m.bm.verts.new(p))
            t = station_t(lip_x, lip_up)
            nx, ny = prof_normal(t)
            smid = (s0 + s1) / 2
            # Faces into the mouth: along the profile, away from the skin it ends
            tang = section_vec(smid, -ny, nx) if name == "top" else section_vec(smid, ny, -nx)
            hem_uv = lambda s: L.region_uv(L.BOTTOM, span_u(s), 0.004)
            per = segs_out // 2
            for k in range(2):
                o = outer[k * per:(k + 1) * per + 1]
                i0, i1 = inner[k], inner[k + 1]
                for q in range(per):
                    m.face((o[q], o[q + 1], i0), (hem_uv(s0), hem_uv(s1), hem_uv(s0)), tang, False)
                m.face((o[per], i1, i0), (hem_uv(s1), hem_uv(s1), hem_uv(s0)), tang, False)
        # Back wall at INTERIOR_DEPTH, facing forward
        _, F, _ = frame((s0 + s1) / 2)
        top_row = [walls["top"][k][0] for k in range(3)]
        bot_row = [walls["bottom"][k][-1] for k in range(3)]
        for k in range(2):
            sa, sb = s0 + (s1 - s0) * k / 2, s0 + (s1 - s0) * (k + 1) / 2
            m.face((top_row[k], top_row[k + 1], bot_row[k + 1], bot_row[k]),
                   (interior_uv(sa, 1.0 * depth), interior_uv(sb, depth),
                    interior_uv(sb, depth), interior_uv(sa, depth)), F, False)


def rib_outline():
    """The rib's outline: the full section, cut straight across the mouth."""
    pts = list(L.TOP_STATIONS) + list(L.BOTTOM_STATIONS[:-1])
    return [prof_t(station_t(x, up)) for x, up in pts]


def build_ribs(m, interior=True):
    outline = rib_outline()
    ribs = rib_positions()
    ys = [p[1] for p in outline]
    for i, s in enumerate(ribs):
        T, _, _ = frame(s)
        pts = [section_point(s, x, y) for x, y in outline]
        end = i in (0, len(ribs) - 1)
        if end:
            # The outside of the end rib: a tip panel
            out = T * (1 if i == len(ribs) - 1 else -1)
            uvs = [L.region_uv(L.TIP, x, 0.45 + (y - min(ys)) / (max(ys) - min(ys)) * 0.5) for x, y in outline]
            m.polygon(pts, uvs, out)
        if not interior:
            continue
        # Inside faces, coloured as the cell each one faces
        for side in ((+1,) if i == 0 else (-1,) if i == len(ribs) - 1 else (+1, -1)):
            cell = i if side > 0 else i - 1
            sc = -S2 + (cell + 0.5) * CELL_W
            uvs = [interior_uv(sc, min(x, L.INTERIOR_DEPTH)) for x, y in outline]
            m.polygon(pts, uvs, T * side)


def stabiliser_points(s):
    """The stabiliser hanging under a tip: top edge along the underside, two lower corners
    where its lines attach."""
    top = [(x, L.profile(x, False)[1]) for x in (0.12,) + L.LINE_X]
    low_rear = (0.66, L.profile(0.66, False)[1] - 0.17)
    low_front = (0.30, L.profile(0.30, False)[1] - 0.19)
    outline = top + [low_rear, low_front]
    return outline, [section_point(s, x, y) for x, y in outline]


def build_stabilisers(m):
    for s in (-S2, S2):
        T, _, _ = frame(s)
        outline, pts = stabiliser_points(s)
        ys = [y for _, y in outline]
        uvs = [L.region_uv(L.TIP, x, (y - min(ys)) / (max(ys) - min(ys)) * 0.4) for x, y in outline]
        for side in (+1, -1):
            m.polygon(pts, uvs, T * side)


def riser_points(side):
    sx = side
    top = Vector((RISER_TOP[0] * sx, RISER_TOP[1], RISER_TOP[2]))
    return {
        "shoulder": Vector((SHOULDER[0] * sx, SHOULDER[1], SHOULDER[2])),
        "top": top,
        "front": top + Vector((0, 0.016, 0.004)),
        "rear": top + Vector((0, -0.016, 0.004)),
        "brake": Vector((0.265 * sx, -0.032, 1.86)),
    }


def build_risers(m):
    for side in (-1, 1):
        rp = riser_points(side)
        a, b = rp["shoulder"], rp["top"]
        axis = (b - a).normalized()
        across = Vector((0, 1, 0))                      # webbing width runs fore-aft
        thick = axis.cross(across).normalized()
        across = thick.cross(axis).normalized()
        corners = []
        for p in (a, b):
            corners.append([p + across * (sa * RISER_W / 2) + thick * (st * RISER_T / 2)
                            for sa, st in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
        verts = [[m.bm.verts.new(p) for p in ring] for ring in corners]
        for k in range(4):
            k2 = (k + 1) % 4
            mid = (corners[0][k] + corners[0][k2]) / 2 - a
            fu0, fu1 = (0, 1) if k % 2 == 0 else (0, 0.15)
            m.face((verts[0][k], verts[0][k2], verts[1][k2], verts[1][k]),
                   (L.region_uv(L.RISER, fu0, 0), L.region_uv(L.RISER, fu1, 0),
                    L.region_uv(L.RISER, fu1, 1), L.region_uv(L.RISER, fu0, 1)), mid, False)
        m.face((verts[1][0], verts[1][1], verts[1][2], verts[1][3]),
               [L.region_uv(L.RISER, f, g) for f, g in ((0, 0), (1, 0), (1, 0.1), (0, 0.1))], axis, False)


def build_lines(m, rows=(0, 1, 2, 3), brakes=True):
    """A-D lines from every rib (the end ribs' from their stabilisers) to the risers:
    A and B to the front of the riser, C and D to the rear; brake lines from the outer
    trailing edge through a cascade to the rear riser. Returns the line count."""
    count = 0
    ribs = rib_positions()
    for i, s in enumerate(ribs):
        rp = riser_points(1 if s > 0 else -1)
        if i in (0, len(ribs) - 1):
            outline, pts = stabiliser_points(s)
            for p, key in ((pts[-1], "front"), (pts[-2], "rear")):
                m.prism(p, rp[key], LINE_R, L.LINE)
                count += 1
            continue
        for r in rows:
            x = L.LINE_X[r]
            p = section_point(s, x, L.profile(x, False)[1])
            m.prism(p, rp["front" if r < 2 else "rear"], LINE_R, L.LINE)
            count += 1
    if brakes:
        for side in (-1, 1):
            rp = riser_points(side)
            tes = [skin_point(side * (S2 - k * CELL_W), 1.0, True, 0.0, 0.0)[0] for k in range(4)]
            mid = sum(tes, Vector()) / len(tes)
            cascade = mid.lerp(rp["brake"], 0.33)
            for p in tes:
                m.prism(p, cascade, LINE_R * 0.85, L.LINE)
            m.prism(cascade, rp["brake"], BRAKE_R, L.LINE)
            count += len(tes) + 1
    return count


def build_lod1(material):
    m = Mesh()
    build_skins(m, L.TOP_STATIONS, L.BOTTOM_STATIONS, SPAN_SEGS_TOP, SPAN_SEGS_BOTTOM)
    build_cell_insides(m)
    build_ribs(m)
    build_stabilisers(m)
    build_risers(m)
    lines = build_lines(m)
    obj = m.to_object("Parachute_LOD1", material)
    return obj, lines


def build_lod2(material):
    """For distance: no pillowing, closed mouths, half the stations, A and D lines only."""
    m = Mesh()
    top = L.TOP_STATIONS[::2]
    if top[-1] != L.TOP_STATIONS[-1]:
        top.append(L.TOP_STATIONS[-1])
    bottom = [(L.TOP_LIP_X, False)] + L.BOTTOM_STATIONS[::2]
    if bottom[-1] != L.BOTTOM_STATIONS[-1]:
        bottom.append(L.BOTTOM_STATIONS[-1])
    global TOP_PILLOW, BOTTOM_PILLOW, SCALLOP
    saved = TOP_PILLOW, BOTTOM_PILLOW, SCALLOP
    TOP_PILLOW, BOTTOM_PILLOW, SCALLOP = 0.0, 0.0, 0.0
    try:
        build_skins(m, top, bottom, 1, 1)
        build_ribs(m, interior=False)
        build_stabilisers(m)
        build_risers(m)
        lines = build_lines(m, rows=(0, 3), brakes=False)
    finally:
        TOP_PILLOW, BOTTOM_PILLOW, SCALLOP = saved
    return m.to_object("Parachute_LOD2", material), lines


# --- scene, export, previews ---------------------------------------------------------

def reset_scene():
    # Clear by hand: read_factory_settings would also unload the Object Builder add-on
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.images,
                 bpy.data.cameras, bpy.data.lights, bpy.data.collections, bpy.data.worlds):
        for block in list(coll):
            coll.remove(block)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def make_material():
    mat = bpy.data.materials.new("parachute")
    mat.use_nodes = True
    mat.use_backface_culling = True       # like the game: what isn't modelled isn't there
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.8, 0.35, 0.1, 1)
    bsdf.inputs["Roughness"].default_value = 0.7
    bsdf.inputs["Specular IOR Level"].default_value = 0.3
    co = os.path.join(TEX_DIR, "parachute_co.png")
    nohq = os.path.join(TEX_DIR, "parachute_nohq.png")
    if os.path.exists(co):
        img = nodes.new("ShaderNodeTexImage")
        img.image = bpy.data.images.load(co)
        links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    if os.path.exists(nohq):
        nimg = nodes.new("ShaderNodeTexImage")
        nimg.image = bpy.data.images.load(nohq)
        nimg.image.colorspace_settings.name = "Non-Color"
        nmap = nodes.new("ShaderNodeNormalMap")
        nmap.inputs["Strength"].default_value = 0.6
        links.new(nimg.outputs["Color"], nmap.inputs["Color"])
        links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    # The fabric lets light through: a little translucency for the preview's underside
    if "Subsurface Weight" in bsdf.inputs:
        bsdf.inputs["Subsurface Weight"].default_value = 0.0
    props = mat.a3ob_properties_material
    props.texture_type = "CUSTOM"     # raw in-game path, written to the P3D as-is
    props.color_raw = CANOPY_TEX[0]
    props.material_path = CANOPY_TEX[1]
    return mat


def set_lod(obj, lod, resolution=0, properties=()):
    props = obj.a3ob_properties_object
    props.is_a3_lod = True
    props.lod = lod
    props.resolution = resolution
    for name, value in properties:
        prop = props.properties.add()
        prop.name = name
        prop.value = value


def new_collection(name):
    coll = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(coll)
    return coll


def build_stand_in(coll):
    """A grey stand-in jumper for the previews only (not exported): feet at the origin,
    shoulders at the riser bottoms, hands up on the risers."""
    mat = bpy.data.materials.new("stand_in")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.07, 0.08, 0.06, 1)
    bm = bmesh.new()
    from mathutils import Matrix

    def limb(a, b, r):
        a, b = Vector(a), Vector(b)
        d = b - a
        rot = d.to_track_quat("Z", "Y").to_matrix().to_4x4()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=r, radius2=r * 0.9,
                              depth=d.length, matrix=Matrix.Translation((a + b) / 2) @ rot)

    for sx in (-1, 1):
        limb((0.1 * sx, 0.02, 0.05), (0.1 * sx, 0.0, 0.9), 0.075)            # legs
        limb((0.2 * sx, 0.0, 1.42), (0.24 * sx, 0.03, 1.72), 0.045)          # upper arms
        limb((0.24 * sx, 0.03, 1.72), (0.25 * sx, 0.0, 1.9), 0.04)           # forearms
    limb((0, 0, 0.9), (0, 0, 1.45), 0.17)                                    # torso
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=0.11,
                              matrix=Matrix.Translation((0, 0.01, 1.62)))
    mesh = bpy.data.meshes.new("StandIn")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("StandIn", mesh)
    mesh.materials.append(mat)
    for p in mesh.polygons:
        p.use_smooth = True
    coll.objects.link(obj)


def setup_render(preview):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.eevee.taa_render_samples = 32
    scene.view_settings.view_transform = "Standard"   # keep the orange saturated
    scene.view_settings.exposure = -0.3
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    nt = world.node_tree
    # A plain daylight sky: brighter, paler towards the horizon
    coord = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.62, 0.68, 0.74, 1)
    ramp.color_ramp.elements[1].position = 0.6
    ramp.color_ramp.elements[1].color = (0.25, 0.42, 0.72, 1)
    nt.links.new(coord.outputs["Generated"], sep.inputs["Vector"])
    nt.links.new(sep.outputs["Z"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], nt.nodes["Background"].inputs["Color"])
    nt.nodes["Background"].inputs["Strength"].default_value = 1.0
    scene.world = world
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 2.8
    sun.data.angle = math.radians(1)
    sun.rotation_euler = (math.radians(45), 0, math.radians(150))
    preview.objects.link(sun)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    preview.objects.link(cam)
    scene.camera = cam
    return cam


def render(cam, path, loc, target, lens, size=(1280, 960), shown=("LOD1", "Preview"), ortho=0.0):
    """lens: focal length in mm; with ortho > 0 an orthographic view that many metres wide."""
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    cam.location = Vector(loc)
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.type = "ORTHO" if ortho else "PERSP"
    cam.data.ortho_scale = ortho or cam.data.ortho_scale
    cam.data.lens = lens
    for c in bpy.data.collections:
        c.hide_render = c.name not in shown
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print(f"[parachute] rendered {path}")


def validate_visual_lods(*objs):
    """The export's own validation is off (it would drop the empty Geometry LOD), so run
    the add-on's validator on the visual LODs here, warnings as errors."""
    validator_mod = sys.modules[A3OB_MODULE + ".utilities.validator"]
    logger_mod = sys.modules[A3OB_MODULE + ".utilities.logger"]
    validator = validator_mod.Validator(logger_mod.ProcessLogger())
    validator.setup_lod_specific()
    for obj in objs:
        if not validator.validate_lod(obj, obj.a3ob_properties_object.lod, False, True, False):
            raise RuntimeError(f"{obj.name} failed the Object Builder add-on's validation")


def tri_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def main():
    addon_utils.enable(A3OB_MODULE, default_set=False)
    if not hasattr(bpy.ops.a3ob, "export_p3d"):
        raise RuntimeError(f"Arma 3 Object Builder add-on ({A3OB_MODULE}) is not installed")
    reset_scene()
    place_canopy()
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(DATA_DIR, exist_ok=True)

    mat = make_material()
    lod1_coll = new_collection("LOD1")
    lod1, lines1 = build_lod1(mat)
    lod1_coll.objects.link(lod1)
    set_lod(lod1, LOD_VISUAL, resolution=1)
    lod2_coll = new_collection("LOD2")
    lod2, lines2 = build_lod2(mat)
    lod2_coll.objects.link(lod2)
    set_lod(lod2, LOD_VISUAL, resolution=2)
    # An empty Geometry LOD, only there to carry autocenter = 0: Binarize reads it from
    # nowhere else, and without it re-centres the model on its bounding box (origin 4.9 m
    # up). No vertices, so nothing to collide with.
    geo_coll = new_collection("Geometry")
    geo = bpy.data.objects.new("Parachute_Geometry", bpy.data.meshes.new("Parachute_Geometry"))
    geo_coll.objects.link(geo)
    set_lod(geo, LOD_GEOMETRY, properties=(("autocenter", "0"),))

    for name, obj, n in (("LOD 1", lod1, lines1), ("LOD 2", lod2, lines2)):
        lo, hi = [min(v.co[k] for v in obj.data.vertices) for k in range(3)], \
                 [max(v.co[k] for v in obj.data.vertices) for k in range(3)]
        print(f"[parachute] {name}: {len(obj.data.vertices)} verts, {tri_count(obj)} tris, "
              f"{n} lines, bbox x {lo[0]:.2f}..{hi[0]:.2f} y {lo[1]:.2f}..{hi[1]:.2f} z {lo[2]:.2f}..{hi[2]:.2f}")

    validate_visual_lods(lod1, lod2)
    with open(os.path.join(DATA_DIR, "model.cfg"), "w") as f:
        f.write(MODEL_CFG)
    bpy.ops.a3ob.export_p3d(
        filepath=os.path.join(DATA_DIR, "parachute.p3d"),
        use_selection=False,
        validate_lods=False,         # it would skip the empty Geometry LOD
        validate_lods_warning_errors=False,
        lod_collisions="FAIL",
        generate_components=False,   # the Geometry LOD is empty on purpose
    )

    preview = new_collection("Preview")
    build_stand_in(preview)
    cam = setup_render(preview)
    bpy.context.preferences.filepaths.save_version = 0   # no .blend1 backups
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "parachute.blend"))

    if os.environ.get("PARACHUTE_NO_RENDER"):
        return
    # What a third-person camera sees: from behind and below, looking up at the canopy
    render(cam, os.path.join(OUT_DIR, "parachute_third_person.png"),
           (0.6, -3.1, 2.5), (0.0, 1.0, 4.3), 14)
    # Straight from the right, orthographic: the section, the trim, the line geometry
    render(cam, os.path.join(OUT_DIR, "parachute_side.png"),
           (20.0, -0.55, 4.9), (0.0, -0.55, 4.9), 50, size=(1000, 1000), ortho=9.4)
    render(cam, os.path.join(OUT_DIR, "parachute_front.png"),
           (6.0, 16.0, 5.0), (0.0, 0.5, 5.5), 40)
    # Another player's view: below, behind and off to one side
    render(cam, os.path.join(OUT_DIR, "parachute_below.png"),
           (-9.0, -12.0, -4.0), (0.0, 0.5, 4.5), 40)
    # Seen from above (from the plane, or higher jumpers)
    render(cam, os.path.join(OUT_DIR, "parachute_above.png"),
           (7.0, -9.0, 16.0), (0.0, 0.0, 5.5), 35)
    if os.environ.get("PARACHUTE_LOD2_RENDER"):
        render(cam, os.path.join(OUT_DIR, "parachute_lod2.png"),
               (-9.0, -12.0, -4.0), (0.0, 0.5, 4.5), 40, shown=("LOD2", "Preview"))


main()
