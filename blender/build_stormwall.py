"""Builds the storm wall: one flat panel of the glowing red wall along the edge of the
battle royale circle (BR_MissionGameplay.c puts a row of them along the nearby edge).

Run headless (from the project root):
    blender --background --python blender/build_stormwall.py

Outputs:
    BattleDayZ/data/stormwall.p3d   MLOD P3D (needs the Arma 3 Object Builder add-on): one
                                      visual LOD, both sides of the panel, and an empty
                                      Geometry LOD (autocenter = 0), so it collides with
                                      nothing and casts no shadow
    models/stormwall.blend

Axes: Blender X/Y/Z = game X/Z/Y. The panel runs along X, WIDTH m centred on the origin, and
rises HEIGHT m from it (the origin is at the bottom edge, the ground line). It faces +-Y,
game +-Z: towards and away from the circle's middle. The texture (stormwall_ca.paa, from
tools/make_stormwall_texture.py) fills it once: v 0 at the ground, 1 at the top.
model.cfg's class stormwall is written by build_parachute.py (the file is shared).
"""

import os

import addon_utils
import bmesh
import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "models")
DATA_DIR = os.path.join(ROOT, "BattleDayZ", "data")
TEX = ("BattleDayZ\\data\\stormwall_ca.paa", "BattleDayZ\\data\\stormwall.rvmat")
A3OB_MODULE = "bl_ext.user_default.Arma3ObjectBuilder"
LOD_VISUAL, LOD_GEOMETRY = "0", "6"

WIDTH = 10.0
HEIGHT = 40.0
ROWS = 4   # horizontal strips, so the glow's vertex lighting doesn't stretch over 40 m


def reset_scene():
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.images,
                 bpy.data.collections):
        for block in list(coll):
            coll.remove(block)


def make_material():
    mat = bpy.data.materials.new("stormwall")
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


def build_panel(mat):
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    for side in (1, -1):   # the two faces, back to back
        verts = [[bm.verts.new((x, 0.0, HEIGHT * r / ROWS)) for x in (-WIDTH / 2, WIDTH / 2)] for r in range(ROWS + 1)]
        for r in range(ROWS):
            quad = [verts[r][0], verts[r][1], verts[r + 1][1], verts[r + 1][0]]
            uvs = [(0, r / ROWS), (1, r / ROWS), (1, (r + 1) / ROWS), (0, (r + 1) / ROWS)]
            if side < 0:
                quad.reverse()
                uvs.reverse()
            f = bm.faces.new(quad)
            for loop, t in zip(f.loops, uvs):
                loop[uv].uv = t
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    mesh = bpy.data.meshes.new("StormWall_LOD1")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("StormWall_LOD1", mesh)
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
    panel = build_panel(make_material())
    coll.objects.link(panel)
    set_lod(panel, LOD_VISUAL, resolution=1)
    geo_coll = bpy.data.collections.new("Geometry")
    bpy.context.scene.collection.children.link(geo_coll)
    geo = bpy.data.objects.new("StormWall_Geometry", bpy.data.meshes.new("StormWall_Geometry"))
    geo_coll.objects.link(geo)
    set_lod(geo, LOD_GEOMETRY, properties=(("autocenter", "0"),))
    print(f"[stormwall] {len(panel.data.vertices)} verts, {len(panel.data.polygons)} tris, {WIDTH} x {HEIGHT} m")
    bpy.ops.a3ob.export_p3d(
        filepath=os.path.join(DATA_DIR, "stormwall.p3d"),
        use_selection=False,
        validate_lods=False,
        validate_lods_warning_errors=False,
        lod_collisions="FAIL",
        generate_components=False,
    )
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "stormwall.blend"))


main()
