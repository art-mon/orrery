import json
import os
import time
from typing import Optional

CACHE_DIR = os.path.join(os.path.dirname(__file__), ".cache")
os.makedirs(CACHE_DIR, exist_ok=True)


def _path(key: str) -> str:
    return os.path.join(CACHE_DIR, f"{key}.json")


def get(key: str, max_age_seconds: int) -> Optional[dict]:
    path = _path(key)
    if not os.path.exists(path):
        return None
    if time.time() - os.path.getmtime(path) > max_age_seconds:
        return None
    with open(path) as f:
        return json.load(f)


def set(key: str, data: dict) -> None:
    with open(_path(key), "w") as f:
        json.dump(data, f)
