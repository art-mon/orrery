"""
Standalone data generator — no Flask required.
Fetches all data sources and writes static JSON to data/ at the repo root.
Called by GitHub Actions on a schedule, or locally to test without the server.
"""
import json
import os
import sys
from datetime import datetime
from pathlib import Path
try:
    from zoneinfo import ZoneInfo
except ImportError:
    from backports.zoneinfo import ZoneInfo

from dotenv import load_dotenv
load_dotenv()

import nasa
import weather
import image
import briefing
import generate_bg

OUT = Path(__file__).parent.parent / "data"
OUT.mkdir(exist_ok=True)


def safe(fn, *args, **kwargs):
    try:
        return fn(*args, **kwargs)
    except Exception as e:
        print(f"  WARNING: {fn.__name__} failed — {e}")
        return {"error": str(e)}


def run():
    print("=== orrery data generator ===")

    print("APOD...")
    apod_data = safe(nasa.apod)

    print("Events...")
    events_data = safe(nasa.events)

    print("Asteroids...")
    asteroids_data = safe(nasa.asteroids)

    print("Weather (current)...")
    weather_data = safe(weather.current)

    print("Weather (forecast)...")
    forecast_data = safe(weather.forecast_tomorrow)

    daily = {
        "generated": datetime.now(ZoneInfo(os.getenv("TIMEZONE", "UTC"))).strftime("%Y-%m-%d"),  # local date, cache key
        "apod":      apod_data,
        "events":    events_data,
        "asteroids": asteroids_data,
        "weather":   weather_data,
        "forecast":  forecast_data,
    }

    (OUT / "daily.json").write_text(json.dumps(daily, indent=2))
    print("✓ data/daily.json")

    # APOD pixel frame
    if apod_data.get("media_type") == "image" and apod_data.get("url"):
        print("APOD frame (64×32)...")
        frame = safe(image.apod_frame, apod_data["url"])
        if "error" not in frame:
            (OUT / "frame.json").write_text(json.dumps(frame))
            print("✓ data/frame.json")
    else:
        print("  APOD is not an image today — skipping frame")

    # Daily briefing (TTS) — regenerated from live data on every run
    print("Briefing (edge-tts)...")
    safe(briefing.generate, daily, OUT)
    if (OUT / "briefing.mp3").exists():
        print("✓ data/briefing.mp3")
    if (OUT / "briefing.txt").exists():
        print("✓ data/briefing.txt")

    # AI background — regenerate on every run (week guard disabled for now)
    bg_path = OUT / "bg_weekly.json"
    print("Background (DALL-E 3)...")
    safe(generate_bg.generate, daily, OUT)
    if bg_path.exists():
        print("✓ data/bg_weekly.json")

    print("=== done ===")


if __name__ == "__main__":
    run()
