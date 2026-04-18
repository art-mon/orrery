#!/usr/bin/env python3
"""
video_to_sprites.py — convert a short video into a 64×32 sprite-sheet JSON
the LED matrix dashboard can render directly.

Usage:
    python scripts/video_to_sprites.py \
        --input  videos/galaxy_source.mp4 \
        --output data/galaxy_sprite.json \
        --frames 30 \
        --fps    10

Pipeline:
    1. Read the video with imageio-ffmpeg (pure-pip, no system ffmpeg needed)
    2. Pick N evenly-spaced frames
    3. Centre-crop each frame to 2:1 aspect
    4. Resize to 64×32 with Lanczos
    5. Dump {"frames": N, "fps": FPS, "width": 64, "height": 32,
             "pixels": [[[r,g,b], ...] × 2048] × N}
"""

from __future__ import annotations

import argparse
import json
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


def frame_to_pixels(frame) -> list[list[int]]:
    """Return flat row-major [[r,g,b], ...] list of length MATRIX_W*MATRIX_H."""
    img = Image.fromarray(frame).convert("RGB")
    img = crop_to_2to1(img)
    img = img.resize((MATRIX_W, MATRIX_H), Image.LANCZOS)
    pixels = list(img.getdata())
    return [[r, g, b] for (r, g, b) in pixels]


def extract_frames(video_path: Path, n_frames: int) -> list:
    """Return n_frames evenly-spaced RGB frames from the video."""
    # Count total frames (fast metadata read)
    meta = iio.immeta(video_path, plugin="pyav") if False else None
    # Read all frames lazily — for short clips this is fine
    frames = list(iio.imiter(video_path))
    total = len(frames)
    if total == 0:
        sys.exit(f"ERROR: could not decode any frames from {video_path}")

    if n_frames >= total:
        print(f"Video has {total} frames — using all of them (requested {n_frames})")
        return frames

    # Evenly-spaced indices
    indices = [int(i * total / n_frames) for i in range(n_frames)]
    return [frames[i] for i in indices]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input",  required=True, type=Path, help="Source video path")
    p.add_argument("--output", required=True, type=Path, help="Destination JSON path")
    p.add_argument("--frames", type=int, default=30, help="Number of frames in sprite (default 30)")
    p.add_argument("--fps",    type=int, default=10, help="Playback fps for the sprite (default 10)")
    args = p.parse_args()

    if not args.input.exists():
        sys.exit(f"ERROR: input video not found: {args.input}")

    print(f"Reading {args.input} …")
    frames = extract_frames(args.input, args.frames)
    print(f"Got {len(frames)} frames. Processing …")

    sprite_pixels = [frame_to_pixels(f) for f in frames]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "frames": len(sprite_pixels),
        "fps":    args.fps,
        "width":  MATRIX_W,
        "height": MATRIX_H,
        "pixels": sprite_pixels,
    }
    args.output.write_text(json.dumps(payload))
    size_kb = args.output.stat().st_size / 1024
    print(f"Wrote {args.output} ({size_kb:.1f} KB, {len(sprite_pixels)} frames @ {args.fps} fps)")


if __name__ == "__main__":
    main()
