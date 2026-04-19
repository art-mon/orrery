#!/usr/bin/env python3
"""
fetch_clouds.py — bake a 64×32 cloud mask from NASA GIBS MODIS Terra
true-color imagery, so the Earth-event scene can overlay real cloud
coverage from ~1 day ago over the globe.

MODIS Terra passes once per day; new imagery lands at GIBS with ~1
day latency, so we fetch yesterday's date (falling back to earlier
dates if the tile isn't ready yet).

Output:  data/world_clouds.json with
    {
      "width":      64,
      "height":     32,
      "date":       "YYYY-MM-DD",
      "source":     "NASA GIBS MODIS_Terra_CorrectedReflectance_TrueColor",
      "clouds_b64": "<256 bytes base64 — bitfield, row-major, MSB=leftmost>"
    }
"""

from __future__ import annotations

import base64
import io
import json
import sys
from datetime import date, timedelta
from pathlib import Path

import requests

try:
    from PIL import Image
except ImportError:
    sys.exit("ERROR: Pillow not installed. Run:\n    pip install Pillow")


W, H             = 64, 32
FETCH_W, FETCH_H = 512, 256      # oversample → downsample for stable averages

GIBS_WMS = "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi"
LAYER    = "MODIS_Terra_CorrectedReflectance_TrueColor"

# Brightness threshold for "this pixel is cloud-covered" in MODIS true-colour.
# Clouds are near-white: high min channel AND low chroma.
MIN_LUM    = 170    # min(r,g,b) must be above this
MAX_CHROMA = 45     # max - min must be below this

# True-color reflectance can't tell snow/ice from clouds, so permanently-icy
# latitudes read as 100% cloudy. Clamp to |lat| <= POLAR_CUTOFF so we don't
# render fake clouds over the Arctic/Antarctic year-round.
POLAR_CUTOFF = 65.0  # degrees; |lat| > this → force "not cloud"


def fetch_image(d: date) -> Image.Image:
    """Download a full-globe equirectangular PNG from GIBS for date d."""
    params = {
        "SERVICE": "WMS",
        "REQUEST": "GetMap",
        "VERSION": "1.3.0",
        "LAYERS":  LAYER,
        "CRS":     "EPSG:4326",
        "BBOX":    "-90,-180,90,180",    # lat_min,lon_min,lat_max,lon_max (1.3.0 axis order)
        "WIDTH":   str(FETCH_W),
        "HEIGHT":  str(FETCH_H),
        "FORMAT":  "image/png",
        "TIME":    d.isoformat(),
    }
    r = requests.get(GIBS_WMS, params=params, timeout=45)
    r.raise_for_status()
    img = Image.open(io.BytesIO(r.content)).convert("RGB")
    if img.width != FETCH_W or img.height != FETCH_H:
        raise ValueError(f"unexpected size {img.width}×{img.height}")
    return img


def build_mask(img: Image.Image) -> bytearray:
    """Threshold brightness, then downsample to 64×32 and pack as bitfield.

    We threshold at the original resolution (so thin clouds stay thin) then
    take "any-cloud-in-block" via nearest-neighbour after a max-filter-ish
    LANCZOS downscale on the binary image. Simpler path: resize RGB then
    threshold at target res — good enough for a 64×32 display.
    """
    small = img.resize((W, H), Image.LANCZOS)
    px    = small.load()
    bits  = bytearray(W * H // 8)
    cloud_count = 0
    for y in range(H):
        lat = 90.0 - (y + 0.5) / H * 180.0
        polar = abs(lat) > POLAR_CUTOFF
        for x in range(W):
            r, g, b = px[x, y]
            is_cloud = (not polar
                        and min(r, g, b) > MIN_LUM
                        and (max(r, g, b) - min(r, g, b)) < MAX_CHROMA)
            if is_cloud:
                byte_idx = (y * W + x) // 8
                bit_pos  = 7 - ((y * W + x) % 8)
                bits[byte_idx] |= (1 << bit_pos)
                cloud_count += 1
    pct = cloud_count / (W * H) * 100
    print(f"  {cloud_count}/{W*H} pixels cloud ({pct:.1f}%)")
    return bits


def render_ascii_preview(bits: bytearray) -> str:
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            byte_idx = (y * W + x) // 8
            bit_pos  = 7 - ((y * W + x) % 8)
            row.append("#" if bits[byte_idx] & (1 << bit_pos) else ".")
        rows.append("".join(row))
    return "\n".join(rows)


def pick_date_and_fetch() -> tuple[date, Image.Image]:
    """Try yesterday first, walk backwards a few days if GIBS hasn't posted yet."""
    target = date.today() - timedelta(days=1)
    last_err: Exception | None = None
    for back in range(5):
        d = target - timedelta(days=back)
        try:
            print(f"Fetching GIBS MODIS Terra true-color for {d}...")
            img = fetch_image(d)
            return d, img
        except Exception as e:
            print(f"  failed: {e}")
            last_err = e
    raise RuntimeError(f"GIBS fetch failed after 5 attempts: {last_err}")


def generate(out_dir: Path) -> Path:
    """Public entry point — called by server/generate.py."""
    d, img = pick_date_and_fetch()
    print(f"  got {img.width}×{img.height} image")
    bits = build_mask(img)

    out = out_dir / "world_clouds.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "width":      W,
        "height":     H,
        "date":       d.isoformat(),
        "source":     f"NASA GIBS {LAYER}",
        "clouds_b64": base64.b64encode(bytes(bits)).decode("ascii"),
    }
    out.write_text(json.dumps(payload) + "\n")
    print(f"Wrote {out} ({out.stat().st_size} bytes)")
    return out


def main() -> None:
    out_dir = Path(__file__).parent.parent / "data"
    out_dir.mkdir(parents=True, exist_ok=True)
    d, img = pick_date_and_fetch()
    print(f"  got {img.width}×{img.height} image")
    bits = build_mask(img)

    print("\nASCII preview:\n")
    print(render_ascii_preview(bits))

    out = out_dir / "world_clouds.json"
    payload = {
        "width":      W,
        "height":     H,
        "date":       d.isoformat(),
        "source":     f"NASA GIBS {LAYER}",
        "clouds_b64": base64.b64encode(bytes(bits)).decode("ascii"),
    }
    out.write_text(json.dumps(payload) + "\n")
    print(f"\nWrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
