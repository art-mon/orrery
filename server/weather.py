import os
from datetime import date, datetime, timedelta, time as dtime
from zoneinfo import ZoneInfo
import requests
import cache
import db

OWM_KEY = os.getenv("OPENWEATHER_API_KEY")
LAT     = os.getenv("LATITUDE",  "40.7128")
LON     = os.getenv("LONGITUDE", "-74.0060")
TZ      = ZoneInfo(os.getenv("TIMEZONE", "UTC"))
BASE          = "https://api.openweathermap.org/data/2.5/weather"
BASE_FORECAST = "https://api.openweathermap.org/data/2.5/forecast"

THIRTY_MIN = 1800
THREE_HOURS = 10800


def current() -> dict:
    cached = cache.get("weather", THIRTY_MIN)
    if cached:
        return cached

    if not OWM_KEY:
        return {"error": "OPENWEATHER_API_KEY not set"}

    r = requests.get(BASE, params={
        "lat":   LAT,
        "lon":   LON,
        "appid": OWM_KEY,
        "units": "metric",
    }, timeout=10)
    r.raise_for_status()
    data = r.json()

    result = {
        "city":       data.get("name"),
        "temp_c":     round(data["main"]["temp"], 1),
        "feels_c":    round(data["main"]["feels_like"], 1),
        "humidity":   data["main"]["humidity"],
        "condition":  data["weather"][0]["main"],
        "description": data["weather"][0]["description"],
        "icon":       data["weather"][0]["icon"],
        "wind_kmh":   round(data["wind"]["speed"] * 3.6, 1),
    }

    cache.set("weather", result)
    db.insert_weather({
        "temp_c":    result["temp_c"],
        "condition": result["condition"],
        "icon":      result["icon"],
        "lat":       float(LAT),
        "lon":       float(LON),
    })
    return result


def _fetch_forecast_raw() -> dict:
    cached = cache.get("weather_forecast_raw", THREE_HOURS)
    if cached:
        return cached
    if not OWM_KEY:
        return {"error": "OPENWEATHER_API_KEY not set"}
    r = requests.get(BASE_FORECAST, params={
        "lat":   LAT,
        "lon":   LON,
        "appid": OWM_KEY,
        "units": "metric",
    }, timeout=10)
    r.raise_for_status()
    data = r.json()
    cache.set("weather_forecast_raw", data)
    return data


def _summarize(entries: list, city: str, day_iso: str) -> dict:
    temps      = [e["main"]["temp"] for e in entries]
    humidities = [e["main"]["humidity"] for e in entries]
    winds      = [e["wind"]["speed"] for e in entries]
    conditions = [e["weather"][0]["main"] for e in entries]
    dominant   = max(set(conditions), key=conditions.count)
    icon_entry = next((e for e in entries if "12:00" in e["dt_txt"]), entries[len(entries)//2])
    return {
        "date":        day_iso,
        "city":        city,
        "temp_min_c":  round(min(temps), 1),
        "temp_max_c":  round(max(temps), 1),
        "humidity":    round(sum(humidities) / len(humidities)),
        "condition":   dominant,
        "description": icon_entry["weather"][0]["description"],
        "icon":        icon_entry["weather"][0]["icon"],
        "wind_kmh":    round((sum(winds) / len(winds)) * 3.6, 1),
    }


def _entries_on_local_date(raw: dict, target_date: date) -> list:
    """Filter forecast entries whose *local-time* date matches target_date.

    The /forecast endpoint stamps dt_txt in UTC, but users perceive
    "today" / "tomorrow" in local time. Using the unix `dt` with the
    configured TIMEZONE avoids off-by-one-day errors near midnight
    UTC (e.g. Europe in the afternoon).
    """
    out = []
    for e in raw.get("list", []):
        local_dt = datetime.fromtimestamp(e["dt"], tz=TZ)
        if local_dt.date() == target_date:
            out.append(e)
    return out


def forecast_tomorrow() -> dict:
    data = _fetch_forecast_raw()
    if "error" in data:
        return data
    tomorrow = datetime.now(TZ).date() + timedelta(days=1)
    entries = _entries_on_local_date(data, tomorrow)
    if not entries:
        return {"error": "no forecast data for tomorrow"}
    return _summarize(entries, data["city"]["name"], tomorrow.isoformat())


def forecast_day_after_tomorrow() -> dict:
    data = _fetch_forecast_raw()
    if "error" in data:
        return data
    day = datetime.now(TZ).date() + timedelta(days=2)
    entries = _entries_on_local_date(data, day)
    if not entries:
        return {"error": "no forecast data for day after tomorrow"}
    return _summarize(entries, data["city"]["name"], day.isoformat())


def forecast_today() -> dict:
    """Summary of today's weather.

    Combines real observations already taken today (stored in the DB
    every time `current()` refreshes) with any remaining 3-hour
    forecast slots still stamped with today's date. Late in the day
    the forecast endpoint has no today-slots left, so the DB history
    carries the whole range — which is the realistic min/max anyway.
    """
    today_local = datetime.now(TZ).date()
    today       = today_local.isoformat()
    data        = _fetch_forecast_raw()
    city        = data.get("city", {}).get("name") if "error" not in data else None

    # Past: real observations from our own polling history (since local midnight)
    midnight_local = datetime.combine(today_local, dtime.min, tzinfo=TZ)
    past = db.get_weather_since(int(midnight_local.timestamp()))

    # Future: forecast slots whose local-time date is still today.
    future_entries = _entries_on_local_date(data, today_local) if "error" not in data else []

    temps      = [r["temp_c"] for r in past]
    humidities = []
    winds      = []
    conditions = [r["condition"] for r in past if r.get("condition")]
    icon       = past[-1]["icon"] if past else None

    for e in future_entries:
        temps.append(e["main"]["temp"])
        humidities.append(e["main"]["humidity"])
        winds.append(e["wind"]["speed"])
        conditions.append(e["weather"][0]["main"])

    # Fill in humidity/wind from current reading if we only have DB history
    cur = current()
    if "error" not in cur:
        humidities.append(cur["humidity"])
        winds.append(cur["wind_kmh"] / 3.6)   # back to m/s for averaging
        if not conditions:
            conditions.append(cur["condition"])
        if not temps:
            temps.append(cur["temp_c"])
        if icon is None:
            icon = cur.get("icon")
        if city is None:
            city = cur.get("city")

    if not temps:
        return {"error": "no weather data for today"}

    dominant = max(set(conditions), key=conditions.count) if conditions else "Unknown"
    return {
        "date":        today,
        "city":        city or "",
        "temp_min_c":  round(min(temps), 1),
        "temp_max_c":  round(max(temps), 1),
        "humidity":    round(sum(humidities) / len(humidities)) if humidities else 0,
        "condition":   dominant,
        "description": dominant.lower(),
        "icon":        icon or "",
        "wind_kmh":    round((sum(winds) / len(winds)) * 3.6, 1) if winds else 0,
        "samples":     len(temps),
    }
