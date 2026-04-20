#!/usr/bin/env python3
"""
fetch_clouds.py — bake a 64×32 cloud *density* map from NASA GIBS MODIS
Terra true-color imagery, so the Earth-event scene can overlay real
cloud coverage from ~1 day ago over the globe.

MODIS Terra passes once per day; new imagery lands at GIBS with ~1
day latency, so we fetch yesterday's date (falling back to earlier
dates if the tile isn't ready yet).

Output: per-pixel grayscale alpha (0 = clear, 255 = solid overcast),
not a binary mask — this avoids the blocky "on/off" look at 64×32
and lets thin cirrus render as subtle whitening while heavy overcast
reads solid. Achieved by thresholding at the native 512×256
resolution, then Lanczos-downsampling the binary cloud image → the
resize naturally anti-aliases into smooth 0..255 density values.

    {
      "width":      64,
      "height":     32,
      "date":       "YYYY-MM-DD",
      "source":     "NASA GIBS MODIS_Terra_CorrectedReflectance_TrueColor",
      "alpha_b64":  "<2048 bytes base64 — one byte per pixel, row-major>"
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
# render fake clouds over the core Arctic/Antarctic ice caps.
# 75° keeps only the permanent ice masked; real sub-Arctic weather systems
# (northern Scandinavia, Siberia, Alaska, northern Canada) still register.
POLAR_CUTOFF = 75.0  # degrees; |lat| > this → force "not cloud"


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


def build_alpha(img: Image.Image) -> bytearray:
    """Threshold at full res, AVERAGE-downsample to 64×32, then gamma-curve.

    Lanczos overshoots/smears and leaves a baseline haze over genuinely
    clear sky — that washes out continents on the display. Instead:

      1. Threshold at 512×256 → binary cloud/clear mask.
      2. BOX resize (area average) → each 64×32 pixel gets the true
         fraction of its full-res block that was cloudy, 0..1.
      3. Gamma curve (pow 1.8) → partial coverage reads as *less* cloud,
         not more. A block that's 30% cloudy → 12% alpha instead of 30%;
         a block that's 100% cloudy stays at 1.0. This keeps clear
         sky genuinely clear while preserving solid overcast.
    """
    px = img.load()
    # Full-res binary mask
    mask = Image.new("L", (img.width, img.height), 0)
    mpx  = mask.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = px[x, y]
            if (min(r, g, b) > MIN_LUM
                    and (max(r, g, b) - min(r, g, b)) < MAX_CHROMA):
                mpx[x, y] = 255

    # Area-average downsample → clean "fraction of block that was cloudy"
    small = mask.resize((W, H), Image.BOX)
    spx   = small.load()

    GAMMA = 1.8

    out = bytearray(W * H)
    total_alpha = 0
    cloudy_pixels = 0
    for y in range(H):
        lat = 90.0 - (y + 0.5) / H * 180.0
        polar = abs(lat) > POLAR_CUTOFF
        for x in range(W):
            if polar:
                a = 0
            else:
                frac = spx[x, y] / 255.0
                a = int(round((frac ** GAMMA) * 255))
                if a < 0:   a = 0
                elif a > 255: a = 255
            out[y * W + x] = a
            total_alpha += a
            if a > 40: cloudy_pixels += 1
    avg = total_alpha / (W * H)
    pct = cloudy_pixels / (W * H) * 100
    print(f"  mean alpha {avg:.1f}/255, {cloudy_pixels}/{W*H} pixels with any cloud ({pct:.1f}%)")
    return out


def render_ascii_preview(alpha: bytearray) -> str:
    ramp = " .:-=+*#%@"
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            a = alpha[y * W + x]
            idx = min(len(ramp) - 1, a * len(ramp) // 256)
            row.append(ramp[idx])
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


def _write_payload(out_dir: Path, d: date, alpha: bytearray) -> Path:
    out = out_dir / "world_clouds.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "width":     W,
        "height":    H,
        "date":      d.isoformat(),
        "source":    f"NASA GIBS {LAYER}",
        "alpha_b64": base64.b64encode(bytes(alpha)).decode("ascii"),
    }
    out.write_text(json.dumps(payload) + "\n")
    print(f"Wrote {out} ({out.stat().st_size} bytes)")
    return out


def generate(out_dir: Path) -> Path:
    """Public entry point — called by server/generate.py."""
    d, img = pick_date_and_fetch()
    print(f"  got {img.width}×{img.height} image")
    alpha = build_alpha(img)
    return _write_payload(out_dir, d, alpha)


def main() -> None:
    out_dir = Path(__file__).parent.parent / "data"
    d, img = pick_date_and_fetch()
    print(f"  got {img.width}×{img.height} image")
    alpha = build_alpha(img)
    print("\nASCII preview:\n")
    print(render_ascii_preview(alpha))
    _write_payload(out_dir, d, alpha)


if __name__ == "__main__":
    main()
