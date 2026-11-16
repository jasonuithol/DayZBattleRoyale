#!/usr/bin/env python3
"""The storm wall's texture: textures/stormwall_ca.png -> BattleRoyale/data/stormwall_ca.paa.

A see-through red glow, strongest at the ground and fading out towards the top, with a
bright line along the ground (the circle you can see on the terrain), a few faint
horizontal bands drifting up it, and thin vertical streaks so it reads as a surface rather
than fog. u runs along the wall (the texture tiles across panels), v up it (v 0 = top).

    python3 tools/make_stormwall_texture.py
"""
import math
import os
import subprocess

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 64, 512
RED = (255, 60, 30)
HOT = (255, 190, 120)   # the line along the ground


def main():
    img = Image.new("RGBA", (W, H))
    px = img.load()
    for y in range(H):
        up = 1 - (y + 0.5) / H            # 0 at the ground, 1 at the top
        fade = (1 - up) ** 1.6            # the glow, thinning upwards
        bands = 0.12 * max(0.0, math.sin(up * math.pi * 14)) ** 6
        base = max(0.0, 1 - up / 0.012)   # the ground line: the bottom ~6 px
        for x in range(W):
            streak = 0.06 * (0.5 + 0.5 * math.sin(x / W * math.pi * 2 * 4 + 1.3 * math.sin(x / W * math.pi * 2)))
            a = min(1.0, 0.08 + 0.42 * fade + bands * fade + streak * fade + 0.85 * base)
            t = min(1.0, base + 0.25 * fade)
            c = tuple(int(RED[k] + (HOT[k] - RED[k]) * t) for k in range(3))
            px[x, y] = (*c, int(a * 255 * (1 - up ** 8)))
    tex = os.path.join(ROOT, "textures")
    os.makedirs(tex, exist_ok=True)
    png = os.path.join(tex, "stormwall_ca.png")
    img.save(png)
    subprocess.run([os.path.join(ROOT, "tools", "img2paa.sh"), png, os.path.join(ROOT, "BattleRoyale", "data")], check=True)


if __name__ == "__main__":
    main()
