#!/usr/bin/env python3
"""
build_earth_texture.py — bake a small RGB Earth texture for the asteroid
scene's Earth sphere. Same pattern as build_moon_texture.py.

Input:  images/earth_nasa_americas_dxm.png (or any Earth-on-black photo)
Output: data/earth_texture.json with {size, rgb_b64, source}

Pixels outside the disc come out black (after the auto-crop), so the
display-side renderer can simply sample the square and rely on the
geometric circle mask.
"""

from __future__ import annotations

import argparse
import base64
import json
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("ERROR: Pillow not installed. Run:\n    pip install Pillow")


DEFAULT_SIZE   = 64
DISC_THRESHOLD = 18


def find_disc_bbox(img: Image.Image, threshold: int = DISC_THRESHOLD) -> tuple[int, int, int, int]:
    gray = img.convert("L").point(lambda v: 255 if v >= threshold else 0)
    bbox = gray.getbbox()
    if bbox is None:
        return (0, 0, img.width, img.height)
    return bbox


def square_crop_around(bbox: tuple[int, int, int, int], img_w: int, img_h: int) -> tuple[int, int, int, int]:
    l, t, r, b = bbox
    cx, cy = (l + r) // 2, (t + b) // 2
    side   = max(r - l, b - t)
    half   = side // 2
    half   = min(half, cx, img_w - cx, cy, img_h - cy)
    return (cx - half, cy - half, cx + half, cy + half)


def bake(input_path: Path, size: int) -> dict:
    print(f"Loading {input_path} …")
    img = Image.open(input_path).convert("RGB")
    print(f"  source {img.width}×{img.height} px")

    bbox = find_disc_bbox(img)
    print(f"  disc bbox: {bbox}")
    sq = square_crop_around(bbox, img.width, img.height)
    print(f"  square crop: {sq} ({sq[2]-sq[0]}×{sq[3]-sq[1]} px)")

    cropped = img.crop(sq).resize((size, size), Image.LANCZOS)

    # Zero out pixels outside the inscribed circle — keeps the sprite
    # clean even if the source has stray specks near the edges.
    pixels = cropped.load()
    c = (size - 1) / 2.0
    r = size / 2.0
    for y in range(size):
        for x in range(size):
            dx, dy = x - c, y - c
            if dx*dx + dy*dy > r*r:
                pixels[x, y] = (0, 0, 0)

    rgb_bytes = cropped.tobytes()
    assert len(rgb_bytes) == size * size * 3

    return {
        "size":    size,
        "rgb_b64": base64.b64encode(rgb_bytes).decode("ascii"),
        "source":  input_path.name,
    }


def render_ascii_preview(payload: dict, cols: int = 48) -> str:
    size = payload["size"]
    data = base64.b64decode(payload["rgb_b64"])
    ramp = " .:-=+*#%@"
    step = max(1, size // cols)
    rows = []
    for y in range(0, size, step):
        row = []
        for x in range(0, size, step):
            idx = (y * size + x) * 3
            r, g, b = data[idx], data[idx+1], data[idx+2]
            lum = (r * 30 + g * 59 + b * 11) // 100
            row.append(ramp[min(len(ramp)-1, lum * len(ramp) // 256)])
        rows.append("".join(row))
    return "\n".join(rows)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", default="images/earth_nasa_americas_dxm.png")
    ap.add_argument("--size",  type=int, default=DEFAULT_SIZE)
    ap.add_argument("--out",   default="data/earth_texture.json")
    args = ap.parse_args()

    payload = bake(Path(args.input), args.size)
    print("\nASCII preview:\n")
    print(render_ascii_preview(payload))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload) + "\n")
    print(f"\nWrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
