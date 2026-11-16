#!/usr/bin/env python3
"""Make the parachute's texture and material (all our own, drawn here; no game textures
are copied). Results go in BattleDayZ/data/:

  parachute_co.paa     colour: alternating deep orange and off-white cells, dark seam
                       tapes along the ribs, dark trim round the cell mouths and the
                       trailing edge, line tabs on the underside, ripstop grain; plus the
                       cell insides, the dark tip panels, the lines and the riser webbing
  parachute_nohq.paa   normal map: ripstop grid, raised seam tapes and stitching, panel
                       seams, puckering beside the seams, the webbing weave
  parachute.rvmat      the vanilla pitched medium tent's nylon material
                       (dz\\gear\\camping\\data\\tent_medium_pitched.rvmat, read from the
                       installed game), on our textures, with some translucency
                       (forcedDiffuse) so the underside isn't flat in the shade

The PNG sources stay in textures/ (the Blender preview uses them too). The layout of the
atlas is shared with the model: tools/parachute_layout.py.

Run: tools/make_parachute_textures.py   (needs numpy, Pillow, GE-Proton and DayZ Tools'
ImageToPAA; converts through tools/img2paa.sh, in its own Proton prefix)
"""
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parachute_layout as L  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STEAM = os.environ.get('STEAM_ROOT', os.path.expanduser('~/.steam/debian-installation'))
ADDONS = os.path.join(STEAM, 'steamapps/common/DayZ/Addons')
SRC = os.path.join(ROOT, 'textures')
DATA = os.path.join(ROOT, 'BattleDayZ', 'data')
N = L.ATLAS
RNG = np.random.default_rng(23)

# ---------------------------------------------------------------- vanilla rvmat ------


def cstr(f):
    out = b''
    while True:
        c = f.read(1)
        if c in (b'\0', b''):
            return out
        out += c


def pbo_file(pbo, name):
    """One uncompressed file out of a PBO, by its path inside it."""
    with open(pbo, 'rb') as f:
        entries = []
        while True:
            n = cstr(f)
            packing, _, _, _, size = struct.unpack('<IIIII', f.read(20))
            if n == b'' and packing == 0x56657273:
                while cstr(f) != b'':
                    cstr(f)
                continue
            if n == b'':
                break
            entries.append((n.decode('latin1').replace('\\', '/').lower(), size))
        for n, size in entries:
            data = f.read(size)
            if n == name.lower():
                return data
    raise SystemExit(f'{name} not found in {pbo}')


def derap(data):
    """A rapified (binary) config, as a dict: values, arrays (lists) and classes (dicts)."""
    if data[:4] != b'\0raP':
        raise SystemExit('not a rapified file')
    pos = 0

    def take(fmt):
        nonlocal pos
        v = struct.unpack_from(fmt, data, pos)[0]
        pos += struct.calcsize(fmt)
        return v

    def cint():
        v = s = 0
        while True:
            b = take('B')
            v |= (b & 0x7f) << s
            s += 7
            if not b & 0x80:
                return v

    def asciiz():
        nonlocal pos
        end = data.index(b'\0', pos)
        v = data[pos:end].decode('latin1')
        pos = end + 1
        return v

    def array():
        out = []
        for _ in range(cint()):
            t = take('B')
            out.append(asciiz() if t in (0, 4) else take('<f') if t == 1 else take('<i') if t == 2 else array())
        return out

    def body(offset):
        nonlocal pos
        saved, pos = pos, offset
        asciiz()                                  # the class it inherits from (none here)
        out = {}
        for _ in range(cint()):
            t = take('B')
            if t == 0:
                name = asciiz()
                out[name] = body(take('<I'))
            elif t == 1:
                sub = take('B')
                name = asciiz()
                out[name] = asciiz() if sub == 0 else take('<f') if sub == 1 else take('<i')
            elif t in (2, 5):
                if t == 5:
                    take('<I')
                name = asciiz()
                out[name] = array()
            else:
                asciiz()
        pos = saved
        return out

    return body(16)


def num(v):
    return f'{v:g}' if isinstance(v, float) else str(v)


def write_rvmat(path):
    tent = derap(pbo_file(os.path.join(ADDONS, 'gear_camping.pbo'), 'data/tent_medium_pitched.rvmat'))
    tex = {
        'Stage1': 'BattleDayZ\\data\\parachute_nohq.paa',
        # Stage2-4: the tent's own neutral detail, macro and ambient-shadow stages
        'Stage5': '#(argb,8,8,3)color(1,0.32,0.42,1,SMDI)',   # nylon: a soft, broad sheen
    }
    lines = []
    for key in ('ambient', 'diffuse', 'forcedDiffuse', 'emmisive', 'specular'):
        vals = tent[key]
        if key == 'forcedDiffuse':
            vals = [0.22, 0.2, 0.18, 1]       # light through the fabric: the underside glows
        if key in ('ambient', 'diffuse'):
            vals = [1.25, 1.25, 1.25, 1]      # the tent's 1.56 is for its dark green; ours is bright
        if key == 'specular':
            vals = [0.18, 0.18, 0.18, 1]
        lines.append(f'{key}[] = {{{", ".join(num(v) for v in vals)}}};')
    lines.append(f'specularPower = {num(tent["specularPower"])};')
    lines.append(f'PixelShaderID = "{tent["PixelShaderID"]}";')
    lines.append(f'VertexShaderID = "{tent["VertexShaderID"]}";')
    for i in range(1, 8):
        name = f'Stage{i}'
        stage = tent[name]
        texture = tex.get(name, stage['texture'])
        if name == 'Stage2':
            texture = '#(argb,8,8,3)color(0.5,0.5,0.5,1,DT)'   # the tent's is its own _co
        uvt = stage['uvTransform']
        lines += [f'class {name}', '{', f'\ttexture = "{texture}";', '\tuvSource = "tex";',
                  '\tclass uvTransform', '\t{']
        for k in ('aside', 'up', 'dir', 'pos'):
            vals = uvt[k]
            if k == 'dir':
                vals = [0, 0, 1]
            lines.append(f'\t\t{k}[] = {{{", ".join(num(v) for v in vals)}}};')
        lines += ['\t};', '};']
    with open(path, 'w', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')


# ---------------------------------------------------------------- drawing helpers ----


def region_grid(region):
    """Pixel box of a region and, for every pixel in it, its (fu, fv) as region_uv takes."""
    x0, y0, x1, y1 = L.region_px(region)
    u0, v0, u1, v1 = region
    cols = (np.arange(x0, x1) + 0.5) / N
    rows = 1 - (np.arange(y0, y1) + 0.5) / N
    fu = (cols - u0 - L.PAD) / (u1 - u0 - 2 * L.PAD)
    fv = (rows - v0 - L.PAD) / (v1 - v0 - 2 * L.PAD)
    FU, FV = np.meshgrid(fu, fv)
    return (x0, y0, x1, y1), FU, FV


def smooth_noise(shape, cell, rng, blur=None):
    """Value noise: random values every `cell` px, smoothly interpolated."""
    h, w = shape
    small = rng.random((h // cell + 2, w // cell + 2)).astype(np.float32)
    img = Image.fromarray((small * 255).astype(np.uint8)).resize(
        ((w // cell + 2) * cell, (h // cell + 2) * cell), Image.BICUBIC)
    if blur:
        img = img.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(img, dtype=np.float32)[:h, :w] / 255.0 - 0.5


def band(d, half_width, soft=0.6):
    """1 inside |d| < half_width, fading over `soft` px-ish (d and widths in the same units)."""
    return np.clip((half_width - np.abs(d)) / soft + 0.5, 0, 1)


def ripstop(shape, period=7):
    h, w = shape
    yy, xx = np.mgrid[0:h, 0:w]
    return ((xx % period == 0) | (yy % period == 0)).astype(np.float32)


class Skin:
    """One skin region: span metres across (u), metres along the profile (v)."""

    def __init__(self, region, length, from_te):
        self.box, FU, FV = region_grid(region)
        self.s = FU * L.SPAN - L.SPAN / 2                 # metres from the centre, + right
        self.a = FV * length                              # metres along the profile
        self.length = length
        self.cell_f = FU * L.CELLS
        self.cell = np.clip(np.floor(self.cell_f), 0, L.CELLS - 1).astype(int)
        # Distance (m) to the nearest rib seam, tips included
        self.d_seam = np.abs(self.cell_f - np.round(self.cell_f)) * (L.SPAN / L.CELLS)
        self.mpp_u = L.SPAN / ((region[2] - region[0]) * N)     # metres per pixel
        self.mpp_v = length / ((region[3] - region[1]) * N)
        self.shape = FU.shape


def cell_colours(cells):
    pal = np.array([L.cell_colour(i) for i in range(L.CELLS)], dtype=np.float32)
    return pal[cells]


def draw_skin(col, hgt, sk, lip_at_end, x_of, stations_arc, tabs):
    """Paint a skin into the colour and height atlases.
    lip_at_end: the mouth lip is at the far (True) or near end of v.
    stations_arc: arc positions (m) of the spanwise panel seams. tabs: arc positions of
    the line attachment tabs (underside)."""
    x0, y0, x1, y1 = sk.box
    rgb = cell_colours(sk.cell)
    h = np.zeros(sk.shape, np.float32)
    mm = sk.mpp_u                                   # ~4.4 mm a pixel across, ~4.9 along

    # Puckering beside the rib seams: soft ridges running off the seam, fading out
    phase = smooth_noise(sk.shape, 64, RNG) * 6
    wave = np.sin(sk.a / 0.045 * 2 * np.pi * 0.5 + phase + sk.d_seam * 9)
    pucker = wave * np.exp(-sk.d_seam / 0.07)
    h += 0.45 * pucker
    shade = 1 - 0.05 * np.clip(pucker, 0, 1) - 0.035 * np.exp(-sk.d_seam / 0.035)

    # Spanwise panel seams (the canopy is sewn from panels a metre or so long)
    seam_mask = np.zeros(sk.shape, np.float32)
    for a in stations_arc:
        seam_mask = np.maximum(seam_mask, band(sk.a - a, 0.005, mm))
    shade *= 1 - 0.16 * seam_mask
    h += 0.5 * seam_mask

    # Grain: ripstop grid, fibre noise, gentle dye mottling
    grid = ripstop(sk.shape)
    fibre = RNG.normal(0, 1, sk.shape).astype(np.float32)
    mottle = smooth_noise(sk.shape, 96, RNG)
    shade *= 1 + 0.03 * grid - 0.012 + 0.018 * fibre + 0.06 * mottle
    h += 0.18 * grid + 0.05 * fibre

    rgb *= shade[..., None]

    # Rib seam tapes, with a line of stitching each side
    tape = band(sk.d_seam, 0.0125, mm)
    stitch_line = band(np.abs(sk.d_seam - 0.0175), 0.0011, mm)
    dashes = (np.sin(sk.a / 0.009 * 2 * np.pi) > -0.2).astype(np.float32)
    stitch = stitch_line * dashes
    trim = np.array(L.TRIM, np.float32)
    trim_tex = trim * (1 + 0.05 * fibre[..., None] + 0.08 * mottle[..., None])
    rgb = rgb * (1 - tape[..., None]) + trim_tex * tape[..., None]
    rgb *= (1 - 0.35 * stitch)[..., None]
    h += 0.9 * tape - 0.35 * stitch

    # Mouth-lip and trailing-edge trim
    lip_d = (sk.length - sk.a) if lip_at_end else sk.a
    te_d = sk.a if lip_at_end else (sk.length - sk.a)
    edge = np.maximum(band(lip_d - 0.035, 0.035, mm), band(te_d - 0.02, 0.02, mm))
    rgb = rgb * (1 - edge[..., None]) + trim_tex * edge[..., None]
    h += 0.7 * edge
    for d in (lip_d - 0.075, te_d - 0.045):     # stitching along the trim
        st = band(d, 0.0011, mm) * dashes
        rgb *= (1 - 0.3 * st)[..., None]

    # Line tabs on the underside: dark reinforcing patches where the lines attach
    if tabs:
        on_rib = np.round(sk.cell_f)
        inner = (on_rib > 0) & (on_rib < L.CELLS)
        tab = np.zeros(sk.shape, np.float32)
        for a in tabs:
            tab = np.maximum(tab, band(sk.d_seam, 0.03, mm) * band(sk.a - a, 0.04, mm))
        tab *= inner
        rgb = rgb * (1 - tab[..., None]) + trim_tex * 0.85 * tab[..., None]
        h += 0.6 * tab

    col[y0:y1, x0:x1] = rgb
    hgt[y0:y1, x0:x1] = h


def draw_interior(col, hgt):
    (x0, y0, x1, y1), FU, FV = region_grid(L.INTERIOR)
    cell_f = FU * L.CELLS
    cell = np.clip(np.floor(cell_f), 0, L.CELLS - 1).astype(int)
    rgb = cell_colours(cell)
    d_seam = np.abs(cell_f - np.round(cell_f)) * (L.SPAN / L.CELLS)
    # Light comes in through the mouth and through the fabric: dimmer further back
    depth = 1 - 0.55 * np.clip(FV, 0, 1) ** 0.8
    shade = 0.42 * depth * (1 - 0.25 * np.exp(-d_seam / 0.06))
    fibre = RNG.normal(0, 1, FU.shape).astype(np.float32)
    shade *= 1 + 0.02 * fibre
    col[y0:y1, x0:x1] = rgb * shade[..., None]
    hgt[y0:y1, x0:x1] = 0.04 * fibre


def draw_flat(col, hgt, region, colour, grain=0.03, weave=False):
    (x0, y0, x1, y1), FU, FV = region_grid(region)
    shape = FU.shape
    fibre = RNG.normal(0, 1, shape).astype(np.float32)
    mottle = smooth_noise(shape, 48, RNG)
    h = 0.05 * fibre
    shade = 1 + grain * fibre + 0.05 * mottle
    if weave:   # twill webbing: diagonal ribs
        yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
        tw = np.sin((xx + yy) * 2 * np.pi / 6)
        shade *= 1 + 0.12 * tw
        h += 0.6 * tw
    else:
        grid = ripstop(shape)
        shade *= 1 + 0.04 * grid
        h += 0.18 * grid
    col[y0:y1, x0:x1] = np.array(colour, np.float32) * shade[..., None]
    hgt[y0:y1, x0:x1] = h


def draw_tip(col, hgt):
    """End ribs (v 0.45 up: the outermost cell's colour, edged with trim) above the
    stabilisers (v below 0.4: dark)."""
    draw_flat(col, hgt, L.TIP, L.TRIM, grain=0.035)
    (x0, y0, x1, y1), FU, FV = region_grid(L.TIP)
    rib = FV >= 0.42
    mpp = 1.0 / (y1 - y0)
    # Distance to the rib panel's border, in region fractions (u is the chord)
    edge = np.minimum.reduce([FU, 1 - FU, (FV - 0.45) / 0.5 * 0.15, (0.95 - FV) / 0.5 * 0.15])
    border = band(edge, 0.012, mpp) + (edge < 0)
    fibre = RNG.normal(0, 1, FU.shape).astype(np.float32)
    mottle = smooth_noise(FU.shape, 48, RNG)
    orange = np.array(L.cell_colour(0), np.float32) * (1 + 0.018 * fibre + 0.05 * mottle)[..., None] \
        * (1 + 0.03 * ripstop(FU.shape))[..., None]
    region = col[y0:y1, x0:x1]
    m = (rib * (1 - np.clip(border, 0, 1)))[..., None]
    col[y0:y1, x0:x1] = region * (1 - m) + orange * m


def normal_map(height, strength):
    """Height (rows top to bottom) to a normal map, x right and y up in texture space."""
    gy, gx = np.gradient(height)
    nx = -gx * strength
    ny = gy * strength            # rows grow downwards, texture v grows upwards
    nz = np.ones_like(nx)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = np.stack([nx / n, ny / n, nz / n], -1) * 0.5 + 0.5
    return (np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8)


def main():
    os.makedirs(SRC, exist_ok=True)
    os.makedirs(DATA, exist_ok=True)
    col = np.zeros((N, N, 3), np.float32)
    col[:] = (90, 90, 95)
    hgt = np.zeros((N, N), np.float32)

    seams_x = (0.07, 0.30, 0.56, 0.80)
    top = Skin(L.TOP, L.TOP_LENGTH, True)
    draw_skin(col, hgt, top, True, None, [L.top_arc_at(x, True) for x in seams_x], [])
    bottom = Skin(L.BOTTOM, L.BOTTOM_LENGTH, False)
    draw_skin(col, hgt, bottom, False, None,
              [L.bottom_arc_at(x) for x in seams_x if x > L.BOTTOM_LIP_X + 0.05],
              [L.bottom_arc_at(x) for x in L.LINE_X])
    draw_interior(col, hgt)
    draw_tip(col, hgt)
    draw_flat(col, hgt, L.LINE, (58, 58, 62), grain=0.06)
    draw_flat(col, hgt, L.RISER, (34, 34, 37), grain=0.05, weave=True)

    co = Image.fromarray(np.clip(col + 0.5, 0, 255).astype(np.uint8), 'RGB')
    co_png = os.path.join(SRC, 'parachute_co.png')
    co.save(co_png)
    nohq = Image.fromarray(normal_map(hgt, 1.6), 'RGB')
    nohq_png = os.path.join(SRC, 'parachute_nohq.png')
    nohq.save(nohq_png)

    subprocess.run([os.path.join(ROOT, 'tools', 'img2paa.sh'), co_png, nohq_png, DATA], check=True)
    write_rvmat(os.path.join(DATA, 'parachute.rvmat'))
    print('textures:', ', '.join(sorted(f for f in os.listdir(DATA) if f.startswith('parachute'))))


main()
