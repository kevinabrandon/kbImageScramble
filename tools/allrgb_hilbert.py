#!/usr/bin/env python3
"""Generate a 4096x4096 "allRGB" image: every 24-bit color exactly once,
arranged as a smooth gradient by walking the RGB cube with a 3D Hilbert
curve and laying that color sequence along a 2D Hilbert curve in the image.

Both curves preserve locality, so pixels that are neighbors on screen get
colors that are neighbors in color space.

Uses Skilling's transform (J. Skilling, "Programming the Hilbert curve",
AIP Conf. Proc. 707, 2004), vectorized over all 2^24 points with numpy.

Output: web/public/allrgb.png (+ a 1024px preview alongside).
"""

import numpy as np
from PIL import Image
from pathlib import Path

N_PIXELS = 1 << 24  # 256^3 == 4096^2


def hilbert_axes(h, bits, dims):
    """Skilling's TransposeToAxes, vectorized: Hilbert index -> coordinates.

    h: uint64 array of Hilbert indices (dims*bits significant bits)
    returns list of `dims` uint32 coordinate arrays, each in [0, 2^bits)
    """
    # Un-interleave the index into the "transposed" form: bit (bits-1-row)
    # of X[dim] is bit (dims*bits - 1 - (row*dims + dim)) of h.
    X = [np.zeros(h.shape, dtype=np.uint32) for _ in range(dims)]
    for row in range(bits):
        for dim in range(dims):
            src = dims * bits - 1 - (row * dims + dim)
            X[dim] |= (((h >> src) & 1) << (bits - 1 - row)).astype(np.uint32)

    # Gray decode by H ^ (H/2)
    t = X[dims - 1] >> 1
    for i in range(dims - 1, 0, -1):
        X[i] ^= X[i - 1]
    X[0] ^= t

    # Undo excess work
    Q = 2
    while Q != (1 << bits):
        P = np.uint32(Q - 1)
        for i in range(dims - 1, -1, -1):
            invert = (X[i] & Q) != 0
            t = (X[0] ^ X[i]) & P
            X[0] = np.where(invert, X[0] ^ P, X[0] ^ t)
            X[i] = np.where(invert, X[i], X[i] ^ t)
        Q <<= 1
    return X


def main():
    h = np.arange(N_PIXELS, dtype=np.uint64)

    print("3D Hilbert walk of the RGB cube (order 8)...")
    r, g, b = hilbert_axes(h, bits=8, dims=3)

    print("2D Hilbert walk of the 4096x4096 image (order 12)...")
    x, y = hilbert_axes(h, bits=12, dims=2)
    del h

    print("Placing colors...")
    img = np.zeros((4096, 4096, 3), dtype=np.uint8)
    img[y, x, 0] = r
    img[y, x, 1] = g
    img[y, x, 2] = b

    print("Verifying every color appears exactly once...")
    keys = (
        img[..., 0].astype(np.uint32) << 16
        | img[..., 1].astype(np.uint32) << 8
        | img[..., 2].astype(np.uint32)
    ).ravel()
    counts = np.bincount(keys, minlength=N_PIXELS)
    assert counts.min() == 1 and counts.max() == 1, "allRGB property violated!"
    print(f"OK: {N_PIXELS:,} pixels, {N_PIXELS:,} distinct colors, each exactly once")

    out = Path(__file__).resolve().parent.parent / "web" / "public" / "allrgb.png"
    print(f"Writing {out} ...")
    Image.fromarray(img).save(out, optimize=True)

    preview = out.with_name("allrgb-preview1024.png")
    print(f"Writing {preview} ...")
    Image.fromarray(img).resize((1024, 1024), Image.LANCZOS).save(preview)

    print(f"Done: {out.stat().st_size / 1e6:.1f} MB png")


if __name__ == "__main__":
    main()
