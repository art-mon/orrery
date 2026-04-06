"""
orrery — weekly AI background generator
Calls DALL-E 3, crops to 2:1, downscales to 64×32,
saves as data/bg_weekly.json (same pixel format as frame.json).

Runs once per week — generate.py checks the stored week number
and skips if it already ran this ISO week.
Style rotates through 7 curated artistic references.
Current weather condition adjusts the prompt mood.
"""

import io
import json
import os
import requests
from datetime import datetime
from pathlib import Path
from PIL import Image

try:
    from openai import OpenAI
except ImportError:
    OpenAI = None

W, H = 64, 32

# ── 7 weekly styles ───────────────────────────────────────────────────────────

STYLES = [
    {
        "name": "Roger Dean",
        "prompt": (
            "floating alien rock formations, impossible gravity, massive organic shapes "
            "in jade and burnt sienna, layered atmospheric depth planes, "
            "extraordinary colour palette, a small classical observatory dome visible "
            "on the highest formation, Croatian highland setting"
        ),
    },
    {
        "name": "Moebius",
        "prompt": (
            "clean architectural line work, flat colour fills, vast empty sky, "
            "tiny observatory dome dwarfed by scale, extremely graphic quality, "
            "desert alien landscape, Heavy Metal magazine but serene, "
            "Croatian highland setting"
        ),
    },
    {
        "name": "Hyper Light Drifter",
        "prompt": (
            "neon ruins, abstract geometric landscape, glowing atmosphere, "
            "extremely bold saturated colour, ancient civilisation aesthetic, "
            "observatory dome among abstract crumbling structures, "
            "Croatian highland, LED matrix aesthetic, vivid"
        ),
    },
    {
        "name": "Hiroshi Nagai",
        "prompt": (
            "flat graphic illustration, geometric architecture under vivid sky, "
            "Japanese city pop 1980s aesthetic, very disciplined palette, "
            "observatory dome on hillside, Croatian highland valley, "
            "bold flat colour planes, retro"
        ),
    },
    {
        "name": "Beksiński",
        "prompt": (
            "dark organic architecture fused with landscape, haunting abstract texture, "
            "bold immediately-readable forms, observatory dome integrated into "
            "surreal ancient structures, Croatian highland, extraordinary atmosphere, "
            "painterly surrealism"
        ),
    },
    {
        "name": "Simon Stålenhag",
        "prompt": (
            "retrofuturistic pastoral landscape, a massive monolithic observatory "
            "structure sitting quietly in misty Croatian highland fields, "
            "melancholic and cinematic, Scandinavian light quality, "
            "scale contrast between vast landscape and single imposing object"
        ),
    },
    {
        "name": "Eyvind Earle",
        "prompt": (
            "geometric Art Deco landscape, perfectly flat razor-sharp silhouettes, "
            "decorative repetition of stylised tree and hill forms, "
            "deep saturated colour fields, woodcut quality, "
            "observatory dome as a perfect geometric half-circle shape, "
            "Croatian highland stylised beyond realism"
        ),
    },
]

# ── Weather mood modifiers ────────────────────────────────────────────────────

WEATHER_MOODS = {
    "clear":     "warm golden light, vivid saturated palette, high contrast",
    "clouds":    "diffuse soft light, muted palette, pewter and slate tones",
    "rain":      "wet brooding atmosphere, dark palette, silver-grey diffuse light",
    "thunder":   "dramatic oppressive sky, near-black palette, single shaft of gold light",
    "snow":      "pale cool light, desaturated blue-white palette, ethereal stillness",
    "mist":      "very low contrast, merged horizon, sfumato, atmospheric haze",
    "drizzle":   "grey-green palette, soft diffuse melancholic light",
}


def get_style(week: int) -> dict:
    return STYLES[week % len(STYLES)]


def get_weather_mood(daily: dict) -> str:
    cond = ((daily.get("weather") or {}).get("condition") or "").lower()
    for key, mood in WEATHER_MOODS.items():
        if key in cond:
            return mood
    return WEATHER_MOODS["clear"]


def build_prompt(style: dict, weather_mood: str) -> str:
    return (
        f"Background landscape artwork. "
        f"{style['prompt']}. "
        f"Lighting and atmosphere: {weather_mood}. "
        f"Wide 2:1 landscape format. No text, no letters, no people, no characters. "
        f"Bold large forms designed for extreme downscaling to low resolution."
    )


def generate_image(prompt: str, client) -> Image.Image:
    response = client.images.generate(
        model="dall-e-3",
        prompt=prompt,
        size="1792x1024",   # closest available to 2:1
        quality="standard",
        style="vivid",
        n=1,
        response_format="url",
    )
    url = response.data[0].url
    img_data = requests.get(url, timeout=30).content
    return Image.open(io.BytesIO(img_data))


def crop_and_resize(img: Image.Image) -> Image.Image:
    """Centre-crop to exact 2:1, then downscale to 64×32 via LANCZOS."""
    w, h = img.size
    target_h = w // 2          # 1792 → 896
    top = (h - target_h) // 2  # centre crop vertically
    img = img.crop((0, top, w, top + target_h))
    return img.resize((W, H), Image.LANCZOS)


def to_frame(img: Image.Image) -> list:
    pixels = []
    for y in range(H):
        for x in range(W):
            r, g, b = img.getpixel((x, y))[:3]
            pixels.append([r, g, b])
    return pixels


def generate(daily: dict, out_dir: Path) -> bool:
    if OpenAI is None:
        print("  openai not installed — run: pip install openai requests")
        return False

    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        print("  OPENAI_API_KEY not set — skipping")
        return False

    client  = OpenAI(api_key=api_key)
    week    = datetime.now().isocalendar()[1]
    style   = get_style(week)
    mood    = get_weather_mood(daily)
    prompt  = build_prompt(style, mood)

    print(f"  style : {style['name']}  (week {week})")
    print(f"  mood  : {mood}")

    img     = generate_image(prompt, client)
    img     = crop_and_resize(img)
    pixels  = to_frame(img)

    payload = {
        "pixels":    pixels,
        "style":     style["name"],
        "week":      week,
        "generated": datetime.now().date().isoformat(),
    }
    (out_dir / "bg_weekly.json").write_text(json.dumps(payload))
    print(f"  ✓ data/bg_weekly.json  ({style['name']})")
    return True
