import os
import requests
import cache
import db

NASA_KEY = os.getenv("NASA_API_KEY", "DEMO_KEY")
BASE = "https://api.nasa.gov"

ONE_DAY = 86400
SIX_HOURS = 21600


def _get(url: str, params: dict) -> dict:
    params["api_key"] = NASA_KEY
    r = requests.get(url, params=params, timeout=10)
    r.raise_for_status()
    return r.json()


def apod() -> dict:
    cached = cache.get("apod", ONE_DAY)
    if cached:
        return cached

    data = _get(f"{BASE}/planetary/apod", {})
    result = {
        "title": data.get("title"),
        "date": data.get("date"),
        "explanation": data.get("explanation"),
        "media_type": data.get("media_type"),  # "image" or "video"
        "url": data.get("url"),
        "hdurl": data.get("hdurl"),
    }
    cache.set("apod", result)
    db.upsert_apod(result)
    return result


def events() -> dict:
    cached = cache.get("events", SIX_HOURS)
    if cached:
        return cached

    data = _get(f"{BASE}/EONET/v3/events", {"status": "open", "limit": 20})
    result = {
        "events": [
            {
                "id": e.get("id"),
                "title": e.get("title"),
                "category": e["categories"][0]["title"] if e.get("categories") else None,
                "date": e["geometry"][0]["date"] if e.get("geometry") else None,
                "coordinates": e["geometry"][0]["coordinates"] if e.get("geometry") else None,
            }
            for e in data.get("events", [])
        ]
    }
    cache.set("events", result)
    db.upsert_events(result["events"])
    return result


def asteroids() -> dict:
    cached = cache.get("asteroids", ONE_DAY)
    if cached:
        return cached

    from datetime import date, timedelta
    today = date.today().isoformat()
    end = (date.today() + timedelta(days=7)).isoformat()

    data = _get(f"{BASE}/neo/rest/v1/feed", {"start_date": today, "end_date": end})

    close = []
    for day_list in data.get("near_earth_objects", {}).values():
        for neo in day_list:
            approach = neo["close_approach_data"][0] if neo.get("close_approach_data") else {}
            close.append({
                "name": neo.get("name"),
                "hazardous": neo.get("is_potentially_hazardous_asteroid", False),
                "diameter_m": round(
                    neo["estimated_diameter"]["meters"]["estimated_diameter_max"]
                ) if neo.get("estimated_diameter") else None,
                "close_approach_date": approach.get("close_approach_date"),
                "miss_distance_km": round(
                    float(approach["miss_distance"]["kilometers"])
                ) if approach.get("miss_distance") else None,
                "velocity_kmh": round(
                    float(approach["relative_velocity"]["kilometers_per_hour"])
                ) if approach.get("relative_velocity") else None,
            })

    # sort by closest approach date then miss distance
    close.sort(key=lambda x: (x["close_approach_date"] or "", x["miss_distance_km"] or 0))

    result = {"asteroids": close[:10]}  # top 10 closest this week
    cache.set("asteroids", result)
    db.upsert_asteroids(close)  # persist all, not just top 10
    return result
