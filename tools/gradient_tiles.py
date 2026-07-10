#!/usr/bin/env python3
"""Generate the kbImageScramble gradient test-image series.

Each image is an n x n board (n = 4, 8, ..., 1024) with:
  - hue sweeping the full color wheel along the top-left -> bottom-right diagonal
  - lightness sweeping dark -> light along the bottom-left -> top-right diagonal
    (saturation stays high, so it runs near-black -> vivid -> near-white)

Not every possible color -- just a smooth, visually distinctive board where
it's obvious when a tile is out of place.

Output: web/public/gradients/grad-<n>.png
"""

import numpy as np
from PIL import Image
from pathlib import Path

SIZES = [4, 8, 16, 32, 64, 128, 256, 512, 1024]


def hsl_to_rgb(h, s, l):
    """Vectorized HSL -> RGB, all inputs/outputs in [0, 1]."""
    c = (1 - np.abs(2 * l - 1)) * s
    hp = (h % 1.0) * 6
    x = c * (1 - np.abs(hp % 2 - 1))
    z = np.zeros_like(c)
    sector = np.floor(hp).astype(int) % 6
    r = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4], [c, x, z, z, x], c)
    g = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4], [x, c, c, x, z], z)
    b = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4], [z, z, x, c, c], x)
    m = l - c / 2
    return r + m, g + m, b + m


def make(n):
    y, x = np.mgrid[0:n, 0:n].astype(np.float64)
    d = n - 1 if n > 1 else 1
    hue = (x + y) / (2 * d)                      # 0 at top-left -> 1 at bottom-right
    up = (x + (d - y)) / (2 * d)                 # 0 at bottom-left -> 1 at top-right
    light = 0.08 + 0.86 * up                     # near-black -> near-white
    r, g, b = hsl_to_rgb(hue, np.full_like(hue, 1.0), light)
    return (np.dstack([r, g, b]) * 255 + 0.5).astype(np.uint8)


def main():
    out_dir = Path(__file__).resolve().parent.parent / "web" / "public" / "gradients"
    out_dir.mkdir(exist_ok=True)
    total = 0
    for n in SIZES:
        img = make(n)
        path = out_dir / f"grad-{n}.png"
        Image.fromarray(img).save(path, optimize=True)
        total += path.stat().st_size
        print(f"{path.name}: {n}x{n}, {path.stat().st_size:,} bytes")
    print(f"total: {total / 1e3:.0f} KB")


if __name__ == "__main__":
    main()
