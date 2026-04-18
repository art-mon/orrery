#!/usr/bin/env python3
"""
video_to_sprites.py — convert a short video into a 64×32 RGB565 sprite
the LED matrix dashboard (web + ESP32) can render directly.

Output is a pair of files:
    data/<name>.json  — metadata ({frames, fps, width, height, format})
    data/<name>.bin   — raw RGB565 bytes, big-endian, row-major,
                        length = frames × width × height × 2

RGB565 matches the native pixel format of the ESP32-HUB75-MatrixPanel-DMA
library, so the firmware can mmap / fread the .bin straight into the DMA
buffer with zero conversion.

Usage:
    python scripts/video_to_sprites.py \\
        --input  videos/galaxy_source.mp4 \\
        --name   galaxy_sprite \\
        --frames 30 \\
        --fps    10
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

from PIL import Image

try:
    import imageio.v3 as iio
except ImportError:
    sys.exit(
        "ERROR: imageio not installed. Run:\n"
        "    pip install -r server/requirements.txt\n"
        "(imageio + imageio-ffmpeg are listed there)"
    )


MATRIX_W = 64
MATRIX_H = 32


def crop_to_2to1(img: Image.Image) -> Image.Image:
    """Centre-crop to 2:1 aspect ratio (matches 64×32)."""
    w, h = img.size
    target_ratio = MATRIX_W / MATRIX_H  # 2.0
    src_ratio = w / h

    if src_ratio > target_ratio:
        # Too wide → crop sides
        new_w = int(h * target_ratio)
        x0 = (w - new_w) // 2
        return img.crop((x0, 0, x0 + new_w, h))
    elif src_ratio < target_ratio:
        # Too tall → crop top/bottom
        new_h = int(w / target_ratio)
        y0 = (h - new_h) // 2
        return img.crop((0, y0, w, y0 + new_h))
    return img


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    """Pack 3×8-bit channels into 16-bit RGB565 (5 red, 6 green, 5 blue)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def frame_to_rgb565_bytes(frame) -> bytes:
    """Return raw RGB565 bytes (big-endian), length = W*H*2."""
    img = Image.fromarray(frame).convert("RGB")
    img = crop_to_2to1(img)
    img = img.resize((MATRIX_W, MATRIX_H), Image.LANCZOS)
    buf = bytearray(MATRIX_W * MATRIX_H * 2)
    pos = 0
    for (r, g, b) in img.getdata():
        v = rgb888_to_rgb565(r, g, b)
        # Big-endian: high byte first (matches HUB75 lib expectations)
        buf[pos]     = (v >> 8) & 0xFF
        buf[pos + 1] = v & 0xFF
        pos += 2
    return bytes(buf)


def extract_frames(video_path: Path, n_frames: int) -> list:
    """Return n_frames evenly-spaced RGB frames from the video."""
    frames = list(iio.imiter(video_path))
    total = len(frames)
    if total == 0:
        sys.exit(f"ERROR: could not decode any frames from {video_path}")

    if n_frames >= total:
        print(f"Video has {total} frames — using all of them (requested {n_frames})")
        return frames

    indices = [int(i * total / n_frames) for i in range(n_frames)]
    return [frames[i] for i in indices]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input",  required=True, type=Path, help="Source video path")
    p.add_argument("--name",   required=True,
                   help="Output base name (e.g. galaxy_sprite) → data/<name>.json + .bin")
    p.add_argument("--out-dir", type=Path, default=Path("data"),
                   help="Output directory (default: data/)")
    p.add_argument("--frames", type=int, default=30, help="Number of frames in sprite (default 30)")
    p.add_argument("--fps",    type=int, default=10, help="Playback fps for the sprite (default 10)")
    args = p.parse_args()

    if not args.input.exists():
        sys.exit(f"ERROR: input video not found: {args.input}")

    print(f"Reading {args.input} …")
    frames = extract_frames(args.input, args.frames)
    print(f"Got {len(frames)} frames. Converting to RGB565 …")

    all_bytes = bytearray()
    for f in frames:
        all_bytes.extend(frame_to_rgb565_bytes(f))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    bin_path  = args.out_dir / f"{args.name}.bin"
    json_path = args.out_dir / f"{args.name}.json"

    bin_path.write_bytes(bytes(all_bytes))

    meta = {
        "frames": len(frames),
        "fps":    args.fps,
        "width":  MATRIX_W,
        "height": MATRIX_H,
        "format": "rgb565_be",       # big-endian RGB565
        "bin":    bin_path.name,
    }
    json_path.write_text(json.dumps(meta, indent=2) + "\n")

    bin_kb = bin_path.stat().st_size / 1024
    print(f"Wrote {bin_path} ({bin_kb:.1f} KB, {len(frames)} frames × {MATRIX_W}×{MATRIX_H} × 2 bytes)")
    print(f"Wrote {json_path} (metadata)")


if __name__ == "__main__":
    main()
