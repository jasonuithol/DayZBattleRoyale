#!/usr/bin/env python3
"""Copies a spot from one DayZ map into missions/<to>/lobbies/<name>.json, a lobby in the sky for
another map (--to: its mission folder, e.g. nasdara). A map's own places don't need this: the
server copies those itself (BR_Maps.c).

Everything the map has within --radius of --at (buildings, walls, rocks, trees, ...) is
listed with its model, its transform relative to the spot (the ground there is y 0), and a
Land_ class when it has one (so doors and ladders work). Under it goes a level floor: flat
slabs of --floor-model laid edge to edge (by the size of its collision geometry), at --floor-y (m relative to the
ground at the spot). Buildings, water and rocks keep their height (foundations go into the floor,
rocks below it hang under the island); trees, bushes and small things (benches, litter, paths)
are moved up or down to stand on the floor, as they stood on the ground. The server builds the file
named by "lobby" in battleroyale.json, "lobbyHeight" metres above lobbyX/lobbyZ.

usage: tools/make_lobby.py NAME --to MAP --at X Z [--radius 45] [--world chernarusplus]
                           [--title "Zub castle"] [--no-plants] [--floor-model P3D]
                           [--floor-y 0] [--spawn X Z R]
examples:
  tools/make_lobby.py zub --to nasdara --at 6543 5597 --radius 42 --title "Zub castle"
  tools/make_lobby.py zub --list --at 6543 5597    (just print what's there)
Then add the name to "lobbies" in that mission's battleroyale.json.

Reads the game's PBOs with DayZ3dMap (~/Projects/DayZ3dMap, or $DAYZ3DMAP). Since DayZ 1.30
(WRP v32) the terrain heights are compressed in a way DayZ3dMap can't read yet, so they come
from its viewer export (viewer/data/<world>/terrain_<n>.f32) when the WRP can't give them."""
import argparse, json, math, os, re, struct, sys

import numpy as np

D3M = os.environ.get('DAYZ3DMAP', os.path.expanduser('~/Projects/DayZ3dMap'))
sys.path.insert(0, D3M)
from dayz3dmap.locate import find_dayz, list_worlds, addons_dirs    # noqa: E402
from dayz3dmap.pbo import PBO                                       # noqa: E402
from dayz3dmap.rap import Rap                                       # noqa: E402
from dayz3dmap.wrp import WRP, Reader                               # noqa: E402
from dayz3dmap.odol import ODOL                                     # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLOOR_MODEL = 'dz\\structures\\rail\\misc\\rail_crossing_block_10.p3d'   # a 3.3 x 9.1 m concrete slab, 2 m thick


# --- the map -----------------------------------------------------------------------------
class World:
    """Models, objects (model index + 4x3 transform) and terrain heights of one map."""

    def __init__(self, name):
        dayz = find_dayz()
        worlds = list_worlds(dayz)
        if name not in worlds:
            sys.exit(f'no world "{name}"; installed: {", ".join(sorted(worlds))}')
        pbo = PBO(worlds[name])
        data = pbo.read(next(n for n in pbo.names() if n.lower().endswith('.wrp')))
        cfg = next((n for n in pbo.names() if n.lower().endswith('config.bin')), None)
        self.names = self.read_names(pbo.read(cfg)) if cfg else {}
        try:
            w = WRP(data)
            self.models, self.objects = w.models, w.objects
            self.elevation, self.cell = np.asarray(w.elevation, np.float32), w.cell
        except Exception:
            self.read_v32(data)
            self.elevation, self.cell = self.exported_terrain(name)
        self.dayz = dayz

    @staticmethod
    def read_names(config_bin):
        out = {}
        for w in Rap(config_bin).root.get('CfgWorlds', {}).values():
            if isinstance(w, dict) and isinstance(w.get('Names'), dict):
                for k, v in w['Names'].items():
                    if isinstance(v, dict) and 'position' in v:
                        out[k.split('_', 1)[-1].lower()] = (float(v['position'][0]), float(v['position'][1]))
        return out

    def read_v32(self, d):
        """DayZ 1.30's WRP: read the model list and the object table from the end of the file,
        past the blocks DayZ3dMap can't decompress yet. Objects are 64 bytes now (60 before)."""
        r = Reader(d)
        r.pos = 4 + 4 + 9
        lx, ly = r.u32(), r.u32()
        # the model list: a count, then that many names ending .p3d; the first one is found
        # by its text, the count just before it
        first = d.index(b'.p3d\x00')
        start = d.rfind(b'\x00', 0, first) + 1
        count, = struct.unpack_from('<I', d, start - 4)
        r.pos = start
        self.models = [r.asciiz() for _ in range(count)]
        if not all(m.lower().endswith('.p3d') for m in self.models):
            sys.exit('could not find the model list in this WRP')
        for _ in range(r.u32()):                 # classed models
            r.asciiz(); r.asciiz(); r.pos += 12 + 4 + 4
        r.quadtree(lx, ly, 4, np.int32)
        size_objects = r.u32()
        r.quadtree(lx, ly, 4, np.int32)
        size_mapinfo = r.u32()
        rec = 64 if size_objects % 64 == 0 else 60
        dt = [('id', '<u4'), ('model', '<u4'), ('m', '<f4', (4, 3)), ('flags', '<u4')]
        if rec == 64:
            dt.append(('extra', '<u4'))
        self.objects = np.frombuffer(d, np.dtype(dt), count=size_objects // rec,
                                     offset=len(d) - size_mapinfo - size_objects)
        if self.objects['model'].max() >= len(self.models):
            sys.exit('the object table in this WRP is not where it was expected')

    @staticmethod
    def exported_terrain(name):
        folder = os.path.join(D3M, 'viewer', 'data', name)
        files = sorted((f for f in os.listdir(folder) if re.fullmatch(r'terrain_\d+\.f32', f)),
                       key=lambda f: -int(f[8:-4])) if os.path.isdir(folder) else []
        if not files:
            sys.exit(f'no terrain heights for {name}: run DayZ3dMap\'s export for it first')
        n = int(files[0][8:-4])
        meta = json.load(open(os.path.join(folder, 'meta.json'))) if os.path.exists(os.path.join(folder, 'meta.json')) else {}
        size = float(meta.get('world_size') or meta.get('worldSize') or {'chernarusplus': 15360, 'enoch': 12800}.get(name, 0))
        if not size:
            sys.exit(f'unknown world size for {name}')
        return np.fromfile(os.path.join(folder, files[0]), '<f4').reshape(n, n), size / n

    def height(self, x, z):
        """Terrain height at x, z (bilinear between the grid points)."""
        e, c = self.elevation, self.cell
        fx, fz = x / c, z / c
        i, j = int(math.floor(fz)), int(math.floor(fx))
        i = max(0, min(e.shape[0] - 2, i)); j = max(0, min(e.shape[1] - 2, j))
        tz, tx = fz - i, fx - j
        a = e[i, j] * (1 - tx) + e[i, j + 1] * tx
        b = e[i + 1, j] * (1 - tx) + e[i + 1, j + 1] * tx
        return float(a * (1 - tz) + b * tz)


def land_classes(dayz):
    """Model path (lower case, backslashes) -> its Land_ class, from every config in the game."""
    out = {}
    for d in addons_dirs(dayz):
        for f in sorted(os.listdir(d)):
            if not f.endswith('.pbo'):
                continue
            try:
                p = PBO(os.path.join(d, f))
                for n in p.names():
                    if not n.lower().endswith('config.bin'):
                        continue
                    for cls, c in Rap(p.read(n)).root.get('CfgVehicles', {}).items():
                        if cls.startswith('Land_') and isinstance(c, dict) and isinstance(c.get('model'), str):
                            out.setdefault(c['model'].lower().replace('/', '\\').lstrip('\\'), cls)
            except Exception:
                pass
    return out


def model_size(dayz, model):
    """x and z size of a model's collision geometry (its geometry LOD), what players stand on.
    Its bounding box can be much bigger: the rail crossing slab's is 9.5 x 10 m, for a solid
    part of 3.3 x 9.1 m (the rest is the rails' room)."""
    want = model.replace('\\', '/').lower()
    for d in addons_dirs(dayz):
        for f in sorted(os.listdir(d)):
            if not f.endswith('.pbo'):
                continue
            p = PBO(os.path.join(d, f))
            prefix = p.props.get('prefix', '').replace('\\', '/').lower()
            for e in p.entries:
                if (prefix + '/' + e[0].replace('\\', '/')).lower() == want:
                    geometry = lambda r: abs(r - 1e13) < 1e10
                    o = ODOL(p.read(e[0]), want=geometry)
                    k = [i for i, r in enumerate(o.resolutions) if geometry(r)]
                    if not k:
                        sys.exit(f'{model} has no collision geometry to stand on')
                    v = np.asarray(o.lods[k[0]]['vertices']).reshape(-1, 3)
                    size = v.max(0) - v.min(0)
                    return float(size[0]), float(size[2])
    sys.exit(f'model not found: {model}')


# --- the copy ----------------------------------------------------------------------------
def r4(v):
    return [round(float(a), 4) for a in v]


def matrix(aside, up, direction, pos):
    return r4(aside) + r4(up) + r4(direction) + [round(float(a), 3) for a in pos]


def floor_tiles(cx, cz, radius, step_x, step_z, model, y):
    """Level slabs on a grid over the circle, edge to edge, their tops at y."""
    tiles = []
    nx, nz = int(math.ceil(radius / step_x)), int(math.ceil(radius / step_z))
    for i in range(-nx, nx + 1):
        for j in range(-nz, nz + 1):
            dx, dz = i * step_x, j * step_z
            if math.hypot(dx, dz) > radius + max(step_x, step_z) * 0.5:
                continue
            tiles.append({'p3d': model, 'floor': 1, 'm': matrix((1, 0, 0), (0, 1, 0), (0, 0, 1), (dx, y, dz))})
    return tiles


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('name')
    ap.add_argument('--at', nargs=2, type=float, metavar=('X', 'Z'))
    ap.add_argument('--radius', type=float, default=45)
    ap.add_argument('--world', default='chernarusplus')
    ap.add_argument('--title')
    ap.add_argument('--no-plants', action='store_true', help='leave out trees and bushes')
    ap.add_argument('--floor-model', default=FLOOR_MODEL)
    ap.add_argument('--floor-y', type=float, default=0, help='the floor, m above the ground at --at')
    ap.add_argument('--spawn', nargs=3, type=float, metavar=('X', 'Z', 'R'),
                    help='where players appear: map position and radius (default: the middle, half the radius)')
    ap.add_argument('--to', help="the mission folder (map) it's for: missions/<to>/lobbies/")
    ap.add_argument('--list', action='store_true', help='only print what is there')
    a = ap.parse_args()

    world = World(a.world)
    if a.at:
        cx, cz = a.at
    elif a.name.lower() in world.names:
        cx, cz = world.names[a.name.lower()]
    else:
        sys.exit(f'give --at X Z (or a place name: {", ".join(sorted(world.names)[:20])}, ...)')
    base = world.height(cx, cz)

    pos = world.objects['m'][:, 3, :]
    near = np.where(np.hypot(pos[:, 0] - cx, pos[:, 2] - cz) < a.radius)[0]
    classes = land_classes(world.dayz)
    pieces, kinds = [], {}
    for i in near:
        model = world.models[world.objects['model'][i]]
        key = model.lower().lstrip('\\')
        plant = '\\plants\\' in key
        if plant and a.no_plants:
            continue
        m = world.objects['m'][i]
        y = m[3][1] - base
        # small things stood on the ground: stand them on the floor the same way
        if key not in classes and '\\rocks\\' not in key and '\\water\\' not in key and 'pond' not in key:
            y += a.floor_y - (world.height(m[3][0], m[3][2]) - base)
        piece = {'p3d': model, 'm': matrix(m[0], m[1], m[2], (m[3][0] - cx, y, m[3][2] - cz))}
        if key in classes:
            piece['cls'] = classes[key]
        pieces.append(piece)
        kind = piece.get('cls') or model.split('\\')[-1]
        kinds[kind] = kinds.get(kind, 0) + 1

    print(f'{a.world} at {cx:.0f} {cz:.0f} (ground {base:.1f} m), within {a.radius:g} m: {len(pieces)} objects')
    for k in sorted(kinds, key=lambda k: (not k.startswith('Land_'), k)):
        print(f'  {kinds[k]:4d}  {k}')
    if a.list:
        return

    fx, fz = model_size(world.dayz, a.floor_model)
    sx, sz = fx - 0.1, fz - 0.1          # a little overlap, so no cracks
    tiles = floor_tiles(cx, cz, a.radius, sx, sz, a.floor_model, a.floor_y)
    sx, sz, sr = a.spawn if a.spawn else (cx, cz, a.radius * 0.5)
    out = {
        'title': a.title or a.name,
        'source': f'{a.world} {cx:.1f} {cz:.1f}, {a.radius:g} m (made by tools/make_lobby.py)',
        'leash': round(a.radius + 10, 1),       # m from the middle: further than this, players are brought back
        'spawn': [round(sx - cx, 2), round(sz - cz, 2), round(sr, 2)],
        'pieces': tiles + pieces,
    }
    if not a.to or not os.path.isdir(os.path.join(HERE, 'missions', a.to)):
        sys.exit('give --to <map>, one of: ' + ', '.join(sorted(os.listdir(os.path.join(HERE, 'missions')))))
    path = os.path.join(HERE, 'missions', a.to, 'lobbies', a.name + '.json')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('{\n')
        for k in ('title', 'source', 'leash', 'spawn'):
            f.write(f'    {json.dumps(k)}: {json.dumps(out[k])},\n')
        f.write('    "pieces": [\n')
        f.write(',\n'.join('        ' + json.dumps(p) for p in out['pieces']))
        f.write('\n    ]\n}\n')
    print(f'+ {len(tiles)} floor slabs ({a.floor_model.split(chr(92))[-1]}, {fx:.2f} x {fz:.2f} m) -> {os.path.relpath(path, HERE)}')


if __name__ == '__main__':
    main()
