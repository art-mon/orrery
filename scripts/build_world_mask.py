#!/usr/bin/env python3
"""
build_world_mask.py — precompute a 64×32 land/ocean mask using Natural Earth
land polygons, so the Earth-event scene (and future overlays like day/night,
weather) can treat land and ocean differently.

One-shot script. Produces data/world_mask.json containing:
    {
      "width": 64, "height": 32,
      "projection": "equirectangular",
      "land_b64": "<256 bytes base64 — bitfield, row-major, MSB=leftmost pixel>"
    }

Each bit = 1 if the pixel centre is on land, 0 if over ocean.
2048 pixels → 256 bytes.
"""

from __future__ import annotations

import base64
import json
import sys
from pathlib import Path

import requests

try:
    from shapely.geometry import Point, shape
    from shapely.ops import unary_union
except ImportError:
    sys.exit("ERROR: shapely not installed. Run:\n    pip install shapely")


W, H = 64, 32

# Natural Earth 110m land polygons — public domain, maintained by nvkelso
NE_LAND_URL = (
    "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/"
    "master/geojson/ne_110m_land.geojson"
)


def fetch_land_polygons() -> object:
    """Download Natural Earth 110m land GeoJSON and return a unified Shapely polygon."""
    print(f"Downloading land polygons from Natural Earth…")
    r = requests.get(NE_LAND_URL, timeout=30)
    r.raise_for_status()
    data = r.json()
    geoms = [shape(feat["geometry"]) for feat in data["features"]]
    print(f"  {len(geoms)} land features")
    return unary_union(geoms)


def pixel_to_lonlat(x: int, y: int) -> tuple[float, float]:
    """Centre of pixel (x, y) in equirectangular projection."""
    lon = (x + 0.5) / W * 360.0 - 180.0
    lat = 90.0 - (y + 0.5) / H * 180.0
    return lon, lat


def build_mask(land) -> bytearray:
    """Return 256 bytes: bit set = land, row-major, MSB = leftmost pixel of row."""
    bits = bytearray(W * H // 8)
    land_count = 0
    for y in range(H):
        for x in range(W):
            lon, lat = pixel_to_lonlat(x, y)
            if land.contains(Point(lon, lat)):
                byte_idx = (y * W + x) // 8
                bit_pos  = 7 - ((y * W + x) % 8)   # MSB-first
                bits[byte_idx] |= (1 << bit_pos)
                land_count += 1
    print(f"  {land_count} / {W*H} pixels on land ({land_count/(W*H)*100:.1f}%)")
    return bits


def render_ascii_preview(bits: bytearray) -> str:
    """Quick sanity check — print the mask as ASCII (# = land, . = ocean)."""
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            byte_idx = (y * W + x) // 8
            bit_pos  = 7 - ((y * W + x) % 8)
            row.append("#" if bits[byte_idx] & (1 << bit_pos) else ".")
        rows.append("".join(row))
    return "\n".join(rows)


def main() -> None:
    land = fetch_land_polygons()
    print("Rasterising to 64×32 …")
    bits = build_mask(land)
    print("\nASCII preview:\n")
    print(render_ascii_preview(bits))

    out = Path("data/world_mask.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "width":       W,
        "height":      H,
        "projection":  "equirectangular",
        "land_b64":    base64.b64encode(bytes(bits)).decode("ascii"),
        "source":      "Natural Earth 110m land (public domain)",
    }
    out.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"\nWrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
