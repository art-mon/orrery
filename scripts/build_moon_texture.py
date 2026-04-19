#!/usr/bin/env python3
"""
build_moon_texture.py — bake a small RGB moon texture from a full-res source
image (Clementine mineral map, LROC WAC mosaic, a curated mineral-moon photo,
etc.) for the display moon scene.

Input:  a source image living in images/ (see images/.gitignore — sources
        are never committed, only the baked texture is).
Output: data/moon_texture.json with
          {
            "size":    <N>,
            "rgb_b64": "<N*N*3 bytes, base64, row-major RGB>",
            "source":  "<filename>",
          }

The script auto-crops to the moon's disc (threshold on luminance, bounding
box → square around the centre) and Lanczos-downsamples to N×N. Defaults are
set for the 23-pixel rendered moon on the LED matrix: N = 48 gives plenty of
detail without being wasteful.

Usage:
    python scripts/build_moon_texture.py --input images/moon.jpg
    python scripts/build_moon_texture.py --input images/mineral.png --size 64
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


DEFAULT_SIZE      = 48
DISC_THRESHOLD    = 20   # 0-255 luminance; pixels above this count as "moon"
DEFAULT_INPUT_GLOB = "images/moon.*"


def find_disc_bbox(img: Image.Image, threshold: int = DISC_THRESHOLD) -> tuple[int, int, int, int]:
    """Return (left, top, right, bottom) bounding box of the moon's disc.

    Works on almost any moon photo: we threshold luminance and take the tight
    bounding box of above-threshold pixels. Falls back to the full image if
    the threshold somehow yields nothing.
    """
    gray = img.convert("L").point(lambda v: 255 if v >= threshold else 0)
    bbox = gray.getbbox()
    if bbox is None:
        print("  (no disc detected — using full image)")
        return (0, 0, img.width, img.height)
    return bbox


def square_crop_around(bbox: tuple[int, int, int, int], img_w: int, img_h: int) -> tuple[int, int, int, int]:
    """Expand bbox to a centred square that still fits inside the image."""
    l, t, r, b = bbox
    cx, cy = (l + r) // 2, (t + b) // 2
    side   = max(r - l, b - t)
    half   = side // 2
    # Clamp so the square stays inside the image bounds
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

    cropped = img.crop(sq)
    small   = cropped.resize((size, size), Image.LANCZOS)

    rgb_bytes = small.tobytes()  # row-major RGB, 3 bytes/pixel
    assert len(rgb_bytes) == size * size * 3

    return {
        "size":    size,
        "rgb_b64": base64.b64encode(rgb_bytes).decode("ascii"),
        "source":  input_path.name,
    }


def render_ascii_preview(payload: dict, cols: int = 48) -> str:
    """Quick terminal sanity check — luminance as ASCII."""
    size  = payload["size"]
    data  = base64.b64decode(payload["rgb_b64"])
    ramp  = " .:-=+*#%@"
    step  = max(1, size // cols)
    rows  = []
    for y in range(0, size, step):
        row = []
        for x in range(0, size, step):
            idx = (y * size + x) * 3
            r, g, b = data[idx], data[idx+1], data[idx+2]
            lum = (r * 30 + g * 59 + b * 11) // 100
            row.append(ramp[min(len(ramp)-1, lum * len(ramp) // 256)])
        rows.append("".join(row))
    return "\n".join(rows)


def resolve_input(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit)
        if not p.exists():
            sys.exit(f"ERROR: input not found: {p}")
        return p
    # Default: first non-hidden file in images/
    images_dir = Path("images")
    if not images_dir.is_dir():
        sys.exit("ERROR: images/ directory missing — create it and add a moon source image.")
    candidates = sorted(p for p in images_dir.iterdir() if p.is_file() and not p.name.startswith("."))
    if not candidates:
        sys.exit("ERROR: no source image found in images/. Drop a moon image there first.")
    if len(candidates) > 1:
        print(f"  (multiple images in images/ — using first: {candidates[0].name})")
    return candidates[0]


def main() -> None:
    ap = argparse.ArgumentParser(description="Bake a moon texture for the LED matrix display.")
    ap.add_argument("--input", help="Source image path (default: first file in images/)")
    ap.add_argument("--size",  type=int, default=DEFAULT_SIZE,
                    help=f"Output texture size (default {DEFAULT_SIZE})")
    ap.add_argument("--out",   default="data/moon_texture.json",
                    help="Output JSON path (default data/moon_texture.json)")
    args = ap.parse_args()

    input_path = resolve_input(args.input)
    payload    = bake(input_path, args.size)

    print("\nASCII preview:\n")
    print(render_ascii_preview(payload))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload) + "\n")
    print(f"\nWrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
