"""
orrery — daily briefing generator
Builds a ~90s English script from daily data + NASA RSS headlines,
using GPT-4o for natural writing (falls back to template if key missing).
Converts to speech via edge-tts (free, no API key required).
Writes data/briefing.mp3 and data/briefing.txt.
"""

import asyncio
import math
import os
import xml.etree.ElementTree as ET
from datetime import datetime
from pathlib import Path
try:
    from zoneinfo import ZoneInfo
except ImportError:
    from backports.zoneinfo import ZoneInfo

import requests

# Voice options (all free, no key needed):
#   en-GB-RyanNeural    — British male, warm, radio-presenter quality  ← default
#   en-US-GuyNeural     — American male, clear and neutral
#   en-US-JennyNeural   — American female, friendly
#   en-AU-WilliamNeural — Australian male, distinctive
VOICE = "en-GB-RyanNeural"

NASA_RSS_URLS = [
    "https://www.nasa.gov/rss/dyn/breaking_news.rss",
    "https://www.nasa.gov/rss/dyn/lg_image_of_the_day.rss",
]

SYSTEM_PROMPT = """\
You write the daily morning broadcast for a personal space observatory \
dashboard called Orrery.

Rules:
- Warm, curious British radio-presenter tone
- 60 to 90 seconds when read aloud (roughly 150–200 words)
- Lead with the most interesting space or astronomy news
- Weave in weather, moon phase, and Earth events naturally — \
  but keep space as the main subject
- Good news bias: frame things with wonder and optimism
- Natural flowing sentences only — no bullet points, no headers, \
  no markdown, no lists
- Close with one short poetic or reflective observation about the universe
- Do not start with "Good morning" — find a more original opening \
  related to what is happening in the sky today
"""


# ── NASA RSS ──────────────────────────────────────────────────────────────────

def fetch_nasa_rss(max_items: int = 5) -> list[dict]:
    """Fetch top headlines from NASA RSS feeds. Returns list of {title, summary}."""
    items = []
    for url in NASA_RSS_URLS:
        try:
            r = requests.get(url, timeout=8)
            r.raise_for_status()
            root = ET.fromstring(r.content)
            ns   = {"media": "http://search.yahoo.com/mrss/"}
            for item in root.findall(".//item"):
                title   = (item.findtext("title") or "").strip()
                summary = (item.findtext("description") or "").strip()
                # strip HTML tags crudely
                import re
                summary = re.sub(r"<[^>]+>", "", summary).strip()
                if title:
                    items.append({"title": title, "summary": summary[:200]})
                if len(items) >= max_items:
                    break
        except Exception as e:
            print(f"  RSS fetch failed ({url}): {e}")
        if len(items) >= max_items:
            break
    return items


# ── Data → context string ─────────────────────────────────────────────────────

def build_context(daily: dict, rss: list[dict]) -> str:
    """Format all available data into a plain-text context block for GPT."""
    tz         = ZoneInfo(os.getenv("TIMEZONE", "UTC"))
    now        = datetime.now(tz)
    day_names  = ["Monday","Tuesday","Wednesday","Thursday","Friday",
                  "Saturday","Sunday"]
    month_names = ["January","February","March","April","May","June",
                   "July","August","September","October","November","December"]
    date_str = f"{day_names[now.weekday()]}, {month_names[now.month-1]} {now.day}"

    lines = [f"TODAY — {date_str}\n"]

    # Weather
    w = daily.get("weather") or {}
    if w and not w.get("error"):
        lines.append(
            f"WEATHER — {w.get('city','')}: {round(w.get('temp_c',0))}°C, "
            f"{w.get('condition','').lower()}. "
            f"Wind {round(w.get('wind_kmh',0))} km/h. "
            f"Humidity {w.get('humidity','')}%."
        )

    # Forecast
    f = daily.get("forecast") or {}
    if f and not f.get("error"):
        lines.append(
            f"TOMORROW — {f.get('condition','').lower()}, "
            f"{round(f.get('temp_min_c',0))}° / {round(f.get('temp_max_c',0))}°C."
        )

    # Moon phase
    KNOWN_NEW_MOON_TS = 947182440
    LUNAR_CYCLE_S     = 29.53059 * 86400
    phase_frac = ((now.timestamp() - KNOWN_NEW_MOON_TS) % LUNAR_CYCLE_S) / LUNAR_CYCLE_S
    illum      = round((1 - math.cos(phase_frac * 2 * math.pi)) / 2 * 100)
    if   phase_frac < 0.02: phase_name = "new moon"
    elif phase_frac < 0.23: phase_name = "waxing crescent"
    elif phase_frac < 0.27: phase_name = "first quarter"
    elif phase_frac < 0.48: phase_name = "waxing gibbous"
    elif phase_frac < 0.52: phase_name = "full moon"
    elif phase_frac < 0.73: phase_name = "waning gibbous"
    elif phase_frac < 0.77: phase_name = "last quarter"
    else:                    phase_name = "waning crescent"
    lines.append(f"MOON — {phase_name.title()}, {illum}% illuminated.")

    # APOD
    apod = daily.get("apod") or {}
    if apod and not apod.get("error"):
        title = apod.get("title", "")
        expl  = apod.get("explanation", "")
        # first two sentences of explanation
        sentences = [s.strip() for s in expl.split(".") if s.strip()]
        excerpt   = ". ".join(sentences[:2]) + "." if sentences else ""
        lines.append(f"\nAPOD — \"{title}\"\n{excerpt}")

    # Asteroids
    rocks = (daily.get("asteroids") or {}).get("asteroids", [])
    if rocks:
        rock   = rocks[0]
        name   = rock.get("name","").replace("(","").replace(")","").strip()
        dist   = round(rock.get("miss_distance_km", 0) / 1000)
        diam   = round(rock.get("diameter_m", 0))
        hazard = "Potentially hazardous." if rock.get("hazardous") else "No hazard."
        lines.append(
            f"\nASTEROIDS — {name} passing at {dist},000 km. "
            f"{diam} m diameter. {hazard}"
        )
        if len(rocks) > 1:
            lines[-1] += f" ({len(rocks)-1} more approaches this week.)"
    else:
        lines.append("\nASTEROIDS — No close approaches this week.")

    # Earth events
    events = (daily.get("events") or {}).get("events", [])
    if events:
        titles = [e.get("title","") for e in events[:4]]
        lines.append("\nEARTH EVENTS — " + "; ".join(titles) + ".")
    else:
        lines.append("\nEARTH EVENTS — None currently tracked.")

    # NASA RSS
    if rss:
        lines.append("\nNASA NEWS —")
        for item in rss:
            lines.append(f"• {item['title']}")
            if item.get("summary"):
                lines.append(f"  {item['summary']}")

    return "\n".join(lines)


# ── GPT script writer ─────────────────────────────────────────────────────────

def build_script_gpt(daily: dict, rss: list[dict]) -> str:
    """Use GPT-4o to write the broadcast script. Raises on failure."""
    from openai import OpenAI
    client  = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))
    context = build_context(daily, rss)

    response = client.chat.completions.create(
        model    = "gpt-4o",
        messages = [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": context},
        ],
        temperature = 0.7,
        max_tokens  = 400,
    )
    return response.choices[0].message.content.strip()


# ── Template fallback ─────────────────────────────────────────────────────────

def build_script_template(daily: dict) -> str:
    """Simple template fallback used when OpenAI key is unavailable."""
    tz         = ZoneInfo(os.getenv("TIMEZONE", "UTC"))
    now        = datetime.now(tz)
    day_names  = ["Monday","Tuesday","Wednesday","Thursday","Friday",
                  "Saturday","Sunday"]
    month_names = ["January","February","March","April","May","June",
                   "July","August","September","October","November","December"]
    date_str = f"{day_names[now.weekday()]}, {month_names[now.month-1]} {now.day}"

    lines = [f"Good morning. Today is {date_str}."]

    w = daily.get("weather") or {}
    if w and not w.get("error"):
        lines.append(
            f"In {w.get('city','your location')}, it is "
            f"{round(w.get('temp_c',0))} degrees and {w.get('condition','').lower()}."
        )

    KNOWN_NEW_MOON_TS = 947182440
    LUNAR_CYCLE_S     = 29.53059 * 86400
    phase_frac = ((now.timestamp() - KNOWN_NEW_MOON_TS) % LUNAR_CYCLE_S) / LUNAR_CYCLE_S
    illum      = round((1 - math.cos(phase_frac * 2 * math.pi)) / 2 * 100)
    if   phase_frac < 0.02: phase_name = "new moon"
    elif phase_frac < 0.23: phase_name = "waxing crescent"
    elif phase_frac < 0.27: phase_name = "first quarter"
    elif phase_frac < 0.48: phase_name = "waxing gibbous"
    elif phase_frac < 0.52: phase_name = "full moon"
    elif phase_frac < 0.73: phase_name = "waning gibbous"
    elif phase_frac < 0.77: phase_name = "last quarter"
    else:                    phase_name = "waning crescent"
    lines.append(f"Tonight's moon is a {phase_name}, {illum} percent illuminated.")

    apod = daily.get("apod") or {}
    if apod and not apod.get("error"):
        title = apod.get("title","")
        expl  = apod.get("explanation","")
        first = expl.split(".")[0].strip() + "." if expl else ""
        if title:
            lines.append(f"NASA's picture of the day is \"{title}\". {first}")

    rocks = (daily.get("asteroids") or {}).get("asteroids", [])
    if rocks:
        rock = rocks[0]
        name = rock.get("name","").replace("(","").replace(")","").strip()
        dist = round(rock.get("miss_distance_km",0) / 1000)
        lines.append(f"Asteroid {name} passes at {dist} thousand kilometres this week.")

    lines.append("That is your orrery briefing. Have a good day.")
    return " ".join(lines)


# ── TTS ───────────────────────────────────────────────────────────────────────

async def _synthesise(script: str, out_path: Path) -> None:
    import edge_tts
    communicate = edge_tts.Communicate(script, VOICE)
    await communicate.save(str(out_path))


# ── Entry point ───────────────────────────────────────────────────────────────

def generate(daily: dict, out_dir: Path) -> bool:
    """Generate briefing.txt and briefing.mp3. Returns True on success."""
    try:
        import edge_tts  # noqa
    except ImportError:
        print("  WARNING: edge-tts not installed — run: pip install edge-tts")
        return False

    # Fetch NASA RSS headlines
    print("  Fetching NASA RSS...")
    rss = fetch_nasa_rss()
    print(f"  {len(rss)} NASA headlines fetched")

    # Build script — GPT if key available, else template
    if os.getenv("OPENAI_API_KEY"):
        try:
            print("  Writing script (GPT-4o)...")
            script = build_script_gpt(daily, rss)
        except Exception as e:
            print(f"  WARNING: GPT script failed ({e}) — using template")
            script = build_script_template(daily)
    else:
        print("  No OPENAI_API_KEY — using template script")
        script = build_script_template(daily)

    (out_dir / "briefing.txt").write_text(script, encoding="utf-8")
    print(f"  script: {len(script.split())} words")

    mp3_path = out_dir / "briefing.mp3"
    asyncio.run(_synthesise(script, mp3_path))

    size_kb = mp3_path.stat().st_size // 1024
    print(f"  audio: {size_kb} KB  ({VOICE})")
    return True
