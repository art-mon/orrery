import os
import requests
import cache
import db

OWM_KEY = os.getenv("OPENWEATHER_API_KEY")
LAT     = os.getenv("LATITUDE",  "40.7128")
LON     = os.getenv("LONGITUDE", "-74.0060")
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


def forecast_tomorrow() -> dict:
    data = _fetch_forecast_raw()
    if "error" in data:
        return data
    from datetime import date, timedelta
    tomorrow = (date.today() + timedelta(days=1)).isoformat()
    entries = [e for e in data["list"] if e["dt_txt"].startswith(tomorrow)]
    if not entries:
        return {"error": "no forecast data for tomorrow"}
    return _summarize(entries, data["city"]["name"], tomorrow)


def forecast_today() -> dict:
    data = _fetch_forecast_raw()
    if "error" in data:
        return data
    from datetime import date
    today = date.today().isoformat()
    entries = [e for e in data["list"] if e["dt_txt"].startswith(today)]
    if not entries:
        return {"error": "no forecast data for today"}
    return _summarize(entries, data["city"]["name"], today)
