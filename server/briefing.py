"""
orrery — daily briefing generator
Builds a ~90s English script from daily data, calls ElevenLabs TTS,
writes data/briefing.mp3 and data/briefing.txt.
"""

import os
import json
import requests
from pathlib import Path
from datetime import datetime
from typing import Optional

ELEVENLABS_API_KEY = os.getenv("ELEVENLABS_API_KEY")

# ElevenLabs voice — "Adam" (warm, clear, radio-presenter quality)
# Replace with any voice ID from your ElevenLabs account
VOICE_ID = "pNInz6obpgDQGcFmaJgB"

# Model — eleven_turbo_v2 is fast + cheap; eleven_multilingual_v2 for best quality
MODEL_ID = "eleven_turbo_v2"


def build_script(daily: dict) -> str:
    """Build a ~90s briefing script from daily.json data."""

    now = datetime.now()
    day_names  = ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"]
    month_names = ["January","February","March","April","May","June",
                   "July","August","September","October","November","December"]
    day_str   = day_names[now.weekday()]
    month_str = month_names[now.month - 1]
    date_str  = f"{day_str}, {month_str} {now.day}"

    lines = [f"Good morning. Today is {date_str}."]

    # Weather
    w = daily.get("weather") or {}
    if w and not w.get("error"):
        temp   = round(w.get("temp_c", 0))
        cond   = w.get("condition", "").lower()
        city   = w.get("city", "your location")
        humid  = w.get("humidity", "")
        wind   = round(w.get("wind_kmh", 0))
        lines.append(
            f"In {city}, it is {temp} degrees and {cond}. "
            f"Humidity at {humid} percent, wind at {wind} kilometres per hour."
        )

    # Tomorrow forecast
    f = daily.get("forecast") or {}
    if f and not f.get("error"):
        tmin = round(f.get("temp_min_c", 0))
        tmax = round(f.get("temp_max_c", 0))
        fcond = f.get("condition", "").lower()
        lines.append(
            f"Tomorrow expects {fcond}, with a low of {tmin} and a high of {tmax} degrees."
        )

    lines.append("Now, looking up.")

    # Moon phase
    # Compute phase from known epoch (matches display logic)
    KNOWN_NEW_MOON_TS = 947182440  # Jan 6 2000 18:14 UTC in seconds
    LUNAR_CYCLE = 29.53059 * 86400
    phase_days = ((now.timestamp() - KNOWN_NEW_MOON_TS) % LUNAR_CYCLE) / 86400
    phase_frac = phase_days / 29.53059
    illum = round((1 - __import__('math').cos(phase_frac * 2 * __import__('math').pi)) / 2 * 100)

    if   phase_frac < 0.02:  phase_name = "new moon"
    elif phase_frac < 0.23:  phase_name = "waxing crescent"
    elif phase_frac < 0.27:  phase_name = "first quarter"
    elif phase_frac < 0.48:  phase_name = "waxing gibbous"
    elif phase_frac < 0.52:  phase_name = "full moon"
    elif phase_frac < 0.73:  phase_name = "waning gibbous"
    elif phase_frac < 0.77:  phase_name = "last quarter"
    else:                     phase_name = "waning crescent"

    lines.append(
        f"Tonight's moon is a {phase_name}, {illum} percent illuminated."
    )

    # APOD
    apod = daily.get("apod") or {}
    if apod and not apod.get("error"):
        title = apod.get("title", "")
        expl  = apod.get("explanation", "")
        # First sentence of explanation only
        first_sentence = expl.split(".")[0].strip() + "." if expl else ""
        if title:
            lines.append(
                f"NASA's astronomy picture of the day is called \"{title}\". "
                + (first_sentence if first_sentence else "")
            )

    # Asteroids
    rocks = (daily.get("asteroids") or {}).get("asteroids", [])
    if rocks:
        rock = rocks[0]
        name = rock.get("name", "").replace("(","").replace(")","").strip()
        dist = round(rock.get("miss_distance_km", 0) / 1000)
        diam = round(rock.get("diameter_m", 0))
        hazardous = rock.get("hazardous", False)
        hazard_str = " It is classified as potentially hazardous." if hazardous else " No hazard."
        lines.append(
            f"One asteroid is making a close pass this week — {name}, "
            f"at {dist} thousand kilometres, approximately {diam} metres across.{hazard_str}"
        )
    else:
        lines.append("No asteroid close approaches are expected this week.")

    # Earth events
    events = (daily.get("events") or {}).get("events", [])
    if events:
        titles = [e.get("title","") for e in events[:3]]
        event_str = "; ".join(titles)
        lines.append(
            f"Earth event{'s' if len(events) > 1 else ''} currently tracked by NASA: {event_str}."
        )
    else:
        lines.append("No active Earth events are currently tracked.")

    lines.append("That is your orrery briefing. Have a good day.")

    return " ".join(lines)


def generate(daily: dict, out_dir: Path) -> bool:
    """Generate briefing.txt and briefing.mp3. Returns True on success."""

    if not ELEVENLABS_API_KEY:
        print("  WARNING: ELEVENLABS_API_KEY not set — skipping audio")
        return False

    script = build_script(daily)
    (out_dir / "briefing.txt").write_text(script, encoding="utf-8")
    print(f"  script: {len(script.split())} words")

    url = f"https://api.elevenlabs.io/v1/text-to-speech/{VOICE_ID}"
    headers = {
        "xi-api-key": ELEVENLABS_API_KEY,
        "Content-Type": "application/json",
    }
    payload = {
        "text": script,
        "model_id": MODEL_ID,
        "voice_settings": {
            "stability": 0.55,
            "similarity_boost": 0.80,
            "style": 0.20,
            "use_speaker_boost": True,
        },
    }

    resp = requests.post(url, headers=headers, json=payload, timeout=60)
    resp.raise_for_status()

    (out_dir / "briefing.mp3").write_bytes(resp.content)
    size_kb = len(resp.content) // 1024
    print(f"  audio: {size_kb} KB")
    return True
