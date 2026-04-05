"""
orrery — daily briefing generator
Builds a ~90s English script from daily data, converts to speech via
edge-tts (Microsoft Edge neural TTS — free, no API key required),
writes data/briefing.mp3 and data/briefing.txt.
"""

import asyncio
import math
from datetime import datetime
from pathlib import Path

# Voice options (all free, no key needed):
#   en-GB-RyanNeural    — British male, warm, radio-presenter quality  ← default
#   en-US-GuyNeural     — American male, clear and neutral
#   en-US-JennyNeural   — American female, friendly
#   en-AU-WilliamNeural — Australian male, distinctive
VOICE = "en-GB-RyanNeural"


def build_script(daily: dict) -> str:
    """Build a ~90s briefing script from daily.json data."""

    now        = datetime.now()
    day_names  = ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"]
    month_names = ["January","February","March","April","May","June",
                   "July","August","September","October","November","December"]
    date_str = f"{day_names[now.weekday()]}, {month_names[now.month - 1]} {now.day}"

    lines = [f"Good morning. Today is {date_str}."]

    # Weather
    w = daily.get("weather") or {}
    if w and not w.get("error"):
        temp  = round(w.get("temp_c", 0))
        cond  = w.get("condition", "").lower()
        city  = w.get("city", "your location")
        humid = w.get("humidity", "")
        wind  = round(w.get("wind_kmh", 0))
        lines.append(
            f"In {city}, it is {temp} degrees and {cond}. "
            f"Humidity at {humid} percent, wind at {wind} kilometres per hour."
        )

    # Tomorrow forecast
    f = daily.get("forecast") or {}
    if f and not f.get("error"):
        tmin  = round(f.get("temp_min_c", 0))
        tmax  = round(f.get("temp_max_c", 0))
        fcond = f.get("condition", "").lower()
        lines.append(
            f"Tomorrow expects {fcond}, with a low of {tmin} "
            f"and a high of {tmax} degrees."
        )

    lines.append("Now, looking up.")

    # Moon phase (matches display logic exactly)
    KNOWN_NEW_MOON_TS = 947182440  # Jan 6 2000 18:14 UTC
    LUNAR_CYCLE_S     = 29.53059 * 86400
    phase_frac = ((now.timestamp() - KNOWN_NEW_MOON_TS) % LUNAR_CYCLE_S) / LUNAR_CYCLE_S
    illum      = round((1 - math.cos(phase_frac * 2 * math.pi)) / 2 * 100)

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
        hazard = " It is classified as potentially hazardous." \
                 if rock.get("hazardous") else " No hazard."
        lines.append(
            f"One asteroid is making a close pass this week — {name}, "
            f"at {dist} thousand kilometres, approximately {diam} metres across.{hazard}"
        )
    else:
        lines.append("No asteroid close approaches are expected this week.")

    # Earth events
    events = (daily.get("events") or {}).get("events", [])
    if events:
        titles    = [e.get("title", "") for e in events[:3]]
        event_str = "; ".join(titles)
        plural    = "s" if len(events) > 1 else ""
        lines.append(
            f"Earth event{plural} currently tracked by NASA: {event_str}."
        )
    else:
        lines.append("No active Earth events are currently tracked.")

    lines.append("That is your orrery briefing. Have a good day.")

    return " ".join(lines)


async def _synthesise(script: str, out_path: Path) -> None:
    """Call edge-tts and save MP3."""
    import edge_tts
    communicate = edge_tts.Communicate(script, VOICE)
    await communicate.save(str(out_path))


def generate(daily: dict, out_dir: Path) -> bool:
    """Generate briefing.txt and briefing.mp3. Returns True on success."""
    try:
        import edge_tts  # noqa — just check it's installed
    except ImportError:
        print("  WARNING: edge-tts not installed — run: pip install edge-tts")
        return False

    script = build_script(daily)
    (out_dir / "briefing.txt").write_text(script, encoding="utf-8")
    print(f"  script: {len(script.split())} words")

    mp3_path = out_dir / "briefing.mp3"
    asyncio.run(_synthesise(script, mp3_path))

    size_kb = mp3_path.stat().st_size // 1024
    print(f"  audio: {size_kb} KB  ({VOICE})")
    return True
