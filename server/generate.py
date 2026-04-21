"""
Standalone data generator — no Flask required.
Fetches all data sources and writes static JSON to data/ at the repo root.
Called by GitHub Actions on a schedule, or locally to test without the server.
"""
import json
import os
import sys
from datetime import datetime, timedelta
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
import fetch_clouds

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

    print("Weather (today forecast)...")
    forecast_today_data = safe(weather.forecast_today)

    print("Weather (tomorrow forecast)...")
    forecast_data = safe(weather.forecast_tomorrow)

    print("Weather (day-after-tomorrow forecast)...")
    forecast_day_after_data = safe(weather.forecast_day_after_tomorrow)

    daily = {
        "generated": datetime.now(ZoneInfo(os.getenv("TIMEZONE", "UTC"))).strftime("%Y-%m-%d"),  # local date, cache key
        "apod":      apod_data,
        "events":    events_data,
        "asteroids": asteroids_data,
        "weather":   weather_data,
        "forecast_today":     forecast_today_data,
        "forecast":           forecast_data,
        "forecast_day_after": forecast_day_after_data,
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

    # AI background — regenerate once per ISO week (bg_weekly.json lives up to its name)
    bg_path  = OUT / "bg_weekly.json"
    tz       = ZoneInfo(os.getenv("TIMEZONE", "UTC"))
    today    = datetime.now(tz).date()
    this_wk  = today.isocalendar()[:2]    # (iso_year, iso_week)
    need_bg  = True
    if bg_path.exists():
        try:
            stored_raw = json.loads(bg_path.read_text()).get("generated", "")
            # "generated" is an ISO timestamp — take the date part (YYYY-MM-DD)
            stored_date = datetime.fromisoformat(stored_raw[:10]).date()
            if stored_date.isocalendar()[:2] == this_wk:
                need_bg = False
                yr, wk = this_wk
                print(f"Background already generated this week ({yr}-W{wk:02d}) — skipping")
        except Exception:
            pass
    if need_bg:
        print("Background (DALL-E 3)...")
        safe(generate_bg.generate, daily, OUT)
        if bg_path.exists():
            print("✓ data/bg_weekly.json")

    # Cloud coverage mask — refresh when the target date changes.
    # GIBS MODIS Terra has ~1 day latency, so target = yesterday. The
    # hourly workflow only downloads a new PNG once per day.
    clouds_path  = OUT / "world_clouds.json"
    target_cloud = (datetime.now(tz) - timedelta(days=1)).date().isoformat()
    need_clouds  = True
    if clouds_path.exists():
        try:
            stored = json.loads(clouds_path.read_text()).get("date", "")
            if stored >= target_cloud:
                need_clouds = False
                print(f"Clouds already at {stored} — skipping")
        except Exception:
            pass
    if need_clouds:
        print("Clouds (NASA GIBS MODIS Terra)...")
        safe(fetch_clouds.generate, OUT)
        if clouds_path.exists():
            print("✓ data/world_clouds.json")

    print("=== done ===")


if __name__ == "__main__":
    run()
