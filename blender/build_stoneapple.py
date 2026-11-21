"""Builds the stone apple: an apple carved in stone, several times life size, on top of the
lobby fountain's column (BR_Round.PutTarget), for the apples people throw into the fountain.

Run headless (from the project root):
    blender --background --python blender/build_stoneapple.py

Outputs:
    BattleDayZ/data/stoneapple.p3d   MLOD P3D (needs the Arma 3 Object Builder add-on): one
                                       visual LOD, smooth shaded, and an empty Geometry LOD
                                       (autocenter = 0), so it collides with nothing
    models/stoneapple.blend

Axes: Blender X/Y/Z = game X/Z/Y. The origin is at the bottom of the apple (it stands on it).
The shape is a turned profile: wider above the middle than below, a dimple at the top round
the stem and a smaller one underneath, and five faint lobes round it; then a stem, leaning a
little. The game's limestone (the old railway bridge's) tiles over it, with
stoneapple.rvmat for its normal map.
model.cfg's class stoneapple is written by build_parachute.py (the file is shared).
"""

import math
import os

import addon_utils
import bmesh
import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "models")
DATA_DIR = os.path.join(ROOT, "BattleDayZ", "data")
TEX = ("dz\\structures\\data\\concrete\\rails_bridge_15_limestone_co.paa", "BattleDayZ\\data\\stoneapple.rvmat")
A3OB_MODULE = "bl_ext.user_default.Arma3ObjectBuilder"
LOD_VISUAL, LOD_GEOMETRY = "0", "6"

SCALE = 0.5            # of the first one (0.45 m radius): the sizes below are that one's
RADIUS = 0.45 * SCALE  # m: the apple is about 0.45 m across
RINGS = 24             # bottom to top
SEGMENTS = 40          # round it
LOBES = 5
LOBE_DEPTH = 0.025 * SCALE
TOP_DIMPLE = 0.12 * SCALE      # m deep, round the stem
BOTTOM_DIMPLE = 0.05 * SCALE
UV_TILES = (3.0 * SCALE, 2.0 * SCALE)  # the stone repeats this often round it and up it (its grain the same size whatever the scale)
STEM_LENGTH = 0.18 * SCALE
STEM_RADIUS = (0.035 * SCALE, 0.022 * SCALE)   # at its foot and its tip
STEM_LEAN = 0.25       # radians


def reset_scene():
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.images,
                 bpy.data.collections):
        for block in list(coll):
            coll.remove(block)


def make_material():
    mat = bpy.data.materials.new("stoneapple")
    props = mat.a3ob_properties_material
    props.texture_type = "CUSTOM"
    props.color_raw = TEX[0]
    props.material_path = TEX[1]
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


def profile(t):
    """t from 0 (the bottom) to 1 (the top): the radius across and the height there"""
    a = math.pi * t
    across = RADIUS * math.sin(a) * (1.0 - 0.10 * math.cos(a))     # wider above the middle
    up = RADIUS * (1.0 - math.cos(a)) * 0.95
    up -= TOP_DIMPLE * math.exp(-((1.0 - t) ** 2) / 0.012)          # sunk round the stem
    up += BOTTOM_DIMPLE * math.exp(-(t ** 2) / 0.008)               # and a little underneath
    return across, up


def build_apple(mat):
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    rings = []
    for r in range(1, RINGS):
        t = r / RINGS
        across, up = profile(t)
        ring = []
        for s in range(SEGMENTS):
            phi = 2 * math.pi * s / SEGMENTS
            lobe = 1.0 + LOBE_DEPTH / RADIUS * math.cos(LOBES * phi) * math.sin(math.pi * t)
            ring.append(bm.verts.new((across * lobe * math.cos(phi), across * lobe * math.sin(phi), up)))
        rings.append((t, ring))
    bottom = bm.verts.new((0.0, 0.0, profile(0.0)[1]))
    top = bm.verts.new((0.0, 0.0, profile(1.0)[1]))

    def uv_of(s, t):
        return (UV_TILES[0] * s / SEGMENTS, UV_TILES[1] * t)

    # the sides, ring to ring
    for (t0, a), (t1, b) in zip(rings, rings[1:]):
        for s in range(SEGMENTS):
            n = (s + 1) % SEGMENTS
            f = bm.faces.new((a[s], a[n], b[n], b[s]))
            for loop, (ss, tt) in zip(f.loops, ((s, t0), (s + 1, t0), (s + 1, t1), (s, t1))):
                loop[uv].uv = uv_of(ss, tt)
    # the two ends
    t_first, first = rings[0]
    t_last, last = rings[-1]
    for s in range(SEGMENTS):
        n = (s + 1) % SEGMENTS
        f = bm.faces.new((first[n], first[s], bottom))
        for loop, (ss, tt) in zip(f.loops, ((s + 1, t_first), (s, t_first), (s + 0.5, 0.0))):
            loop[uv].uv = uv_of(ss, tt)
        f = bm.faces.new((last[s], last[n], top))
        for loop, (ss, tt) in zip(f.loops, ((s, t_last), (s + 1, t_last), (s + 0.5, 1.0))):
            loop[uv].uv = uv_of(ss, tt)

    # the stem: a tapered tube from the dimple, leaning a little, capped
    foot_z = profile(1.0)[1] - 0.02 * SCALE
    sides = 8
    base, tip = [], []
    for i in range(sides):
        phi = 2 * math.pi * i / sides
        base.append(bm.verts.new((STEM_RADIUS[0] * math.cos(phi), STEM_RADIUS[0] * math.sin(phi), foot_z)))
        x = STEM_RADIUS[1] * math.cos(phi) + STEM_LENGTH * math.sin(STEM_LEAN)
        tip.append(bm.verts.new((x, STEM_RADIUS[1] * math.sin(phi), foot_z + STEM_LENGTH * math.cos(STEM_LEAN))))
    for i in range(sides):
        n = (i + 1) % sides
        f = bm.faces.new((base[i], base[n], tip[n], tip[i]))
        for loop, (u, v) in zip(f.loops, ((i / sides, 0), ((i + 1) / sides, 0), ((i + 1) / sides, 0.3), (i / sides, 0.3))):
            loop[uv].uv = (u, v)
    cap = bm.faces.new(tip)
    for loop in cap.loops:
        loop[uv].uv = (0.5, 0.5)

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    mesh = bpy.data.meshes.new("StoneApple_LOD1")
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new("StoneApple_LOD1", mesh)
    mesh.materials.append(mat)
    return obj


def main():
    addon_utils.enable(A3OB_MODULE, default_set=False)
    if not hasattr(bpy.ops.a3ob, "export_p3d"):
        raise RuntimeError(f"Arma 3 Object Builder add-on ({A3OB_MODULE}) is not installed")
    reset_scene()
    os.makedirs(OUT_DIR, exist_ok=True)
    coll = bpy.data.collections.new("LOD1")
    bpy.context.scene.collection.children.link(coll)
    apple = build_apple(make_material())
    coll.objects.link(apple)
    set_lod(apple, LOD_VISUAL, resolution=1)
    geo_coll = bpy.data.collections.new("Geometry")
    bpy.context.scene.collection.children.link(geo_coll)
    geo = bpy.data.objects.new("StoneApple_Geometry", bpy.data.meshes.new("StoneApple_Geometry"))
    geo_coll.objects.link(geo)
    set_lod(geo, LOD_GEOMETRY, properties=(("autocenter", "0"),))
    zs = [v.co.z for v in apple.data.vertices]
    print(f"[stoneapple] {len(apple.data.vertices)} verts, {len(apple.data.polygons)} tris, {min(zs):.2f} to {max(zs):.2f} m high")
    bpy.ops.a3ob.export_p3d(
        filepath=os.path.join(DATA_DIR, "stoneapple.p3d"),
        use_selection=False,
        validate_lods=False,
        validate_lods_warning_errors=False,
        lod_collisions="FAIL",
        generate_components=False,
    )
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "stoneapple.blend"))


main()
