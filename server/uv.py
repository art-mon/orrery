import os
from datetime import datetime
from typing import Optional
from zoneinfo import ZoneInfo
import requests
import cache

LAT = os.getenv("LATITUDE",  "40.7128")
LON = os.getenv("LONGITUDE", "-74.0060")
TZ_NAME = os.getenv("TIMEZONE", "UTC")
TZ = ZoneInfo(TZ_NAME)

BASE = "https://api.open-meteo.com/v1/forecast"
THIRTY_MIN = 1800


def _band(uv: float) -> str:
    if uv < 3:  return "low"
    if uv < 6:  return "moderate"
    if uv < 8:  return "high"
    if uv < 11: return "very_high"
    return "extreme"


def _safe_exposure_minutes(uv: float, skin_type: int = 2) -> Optional[int]:
    """WHO-derived minutes to reach 1 MED for the given Fitzpatrick skin type
    (I-VI). Returns None when UV is effectively zero."""
    if uv <= 0.1:
        return None
    med_j_per_m2 = {1: 200, 2: 250, 3: 350, 4: 450, 5: 600, 6: 1000}.get(skin_type, 250)
    # UV index unit = 25 mW/m² of erythemally-weighted irradiance
    irradiance = uv * 0.025  # W/m²
    seconds = med_j_per_m2 / irradiance
    return max(1, round(seconds / 60))


def current(skin_type: int = 2) -> dict:
    cached = cache.get("uv", THIRTY_MIN)
    if cached and cached.get("skin_type") == skin_type:
        return cached

    try:
        r = requests.get(BASE, params={
            "latitude":  LAT,
            "longitude": LON,
            "hourly":    "uv_index",
            "daily":     "uv_index_max",
            "current":   "uv_index",
            "timezone":  TZ_NAME,
            "forecast_days": 1,
        }, timeout=10)
        r.raise_for_status()
        data = r.json()
    except requests.RequestException as e:
        return {"error": f"open-meteo request failed: {e}"}

    now_uv = float(data.get("current", {}).get("uv_index") or 0)
    hourly_times = data.get("hourly", {}).get("time", [])
    hourly_uv    = data.get("hourly", {}).get("uv_index", [])
    daily_max    = (data.get("daily", {}).get("uv_index_max") or [None])[0]

    hourly = [
        {"time": t, "uv": round(float(u), 1) if u is not None else None}
        for t, u in zip(hourly_times, hourly_uv)
    ]

    peak_hour = None
    if hourly:
        valid = [h for h in hourly if h["uv"] is not None]
        if valid:
            peak = max(valid, key=lambda h: h["uv"])
            peak_hour = peak["time"]

    result = {
        "uv":            round(now_uv, 1),
        "band":          _band(now_uv),
        "daily_max":     round(float(daily_max), 1) if daily_max is not None else None,
        "peak_time":     peak_hour,
        "safe_minutes":  _safe_exposure_minutes(now_uv, skin_type),
        "skin_type":     skin_type,
        "hourly":        hourly,
        "fetched_at":    datetime.now(TZ).isoformat(timespec="seconds"),
    }

    cache.set("uv", result)
    return result
