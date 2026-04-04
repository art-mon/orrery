import sqlite3
import json
import os
import time
from typing import Optional

DB_PATH = os.path.join(os.path.dirname(__file__), "nasa.db")


def _conn():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init():
    with _conn() as conn:
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS apod (
                date        TEXT PRIMARY KEY,
                title       TEXT,
                explanation TEXT,
                media_type  TEXT,
                url         TEXT,
                hdurl       TEXT,
                fetched_at  INTEGER
            );

            CREATE TABLE IF NOT EXISTS events (
                id          TEXT PRIMARY KEY,
                title       TEXT,
                category    TEXT,
                coordinates TEXT,
                first_seen  TEXT,
                last_seen   TEXT,
                status      TEXT DEFAULT 'open'
            );

            CREATE TABLE IF NOT EXISTS asteroids (
                name                TEXT,
                close_approach_date TEXT,
                miss_distance_km    INTEGER,
                diameter_m          INTEGER,
                hazardous           INTEGER,
                velocity_kmh        INTEGER,
                fetched_at          INTEGER,
                PRIMARY KEY (name, close_approach_date)
            );

            CREATE TABLE IF NOT EXISTS weather (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER,
                temp_c    REAL,
                condition TEXT,
                icon      TEXT,
                lat       REAL,
                lon       REAL
            );
        """)


# ─── APOD ─────────────────────────────────────────────────────────────────────

def upsert_apod(data: dict):
    with _conn() as conn:
        conn.execute("""
            INSERT INTO apod (date, title, explanation, media_type, url, hdurl, fetched_at)
            VALUES (:date, :title, :explanation, :media_type, :url, :hdurl, :fetched_at)
            ON CONFLICT(date) DO UPDATE SET
                title       = excluded.title,
                explanation = excluded.explanation,
                url         = excluded.url,
                hdurl       = excluded.hdurl,
                fetched_at  = excluded.fetched_at
        """, {**data, "fetched_at": int(time.time())})


def get_apod(date: str) -> Optional[dict]:
    with _conn() as conn:
        row = conn.execute("SELECT * FROM apod WHERE date = ?", (date,)).fetchone()
        return dict(row) if row else None


def get_apod_history(limit: int = 30) -> list:
    with _conn() as conn:
        rows = conn.execute(
            "SELECT date, title, media_type FROM apod ORDER BY date DESC LIMIT ?", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


# ─── EONET Events ─────────────────────────────────────────────────────────────

def upsert_events(events: list):
    """Insert new events, update last_seen on existing ones."""
    today = time.strftime("%Y-%m-%d")
    with _conn() as conn:
        for e in events:
            coords = json.dumps(e.get("coordinates"))
            conn.execute("""
                INSERT INTO events (id, title, category, coordinates, first_seen, last_seen, status)
                VALUES (:id, :title, :category, :coordinates, :today, :today, 'open')
                ON CONFLICT(id) DO UPDATE SET
                    last_seen = :today,
                    status    = 'open'
            """, {
                "id":          e["id"],
                "title":       e["title"],
                "category":    e.get("category"),
                "coordinates": coords,
                "today":       today,
            })


def get_event_history(limit: int = 50) -> list:
    with _conn() as conn:
        rows = conn.execute(
            "SELECT id, title, category, first_seen, last_seen, status FROM events ORDER BY last_seen DESC LIMIT ?",
            (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


# ─── Asteroids ────────────────────────────────────────────────────────────────

def upsert_asteroids(asteroids: list):
    now = int(time.time())
    with _conn() as conn:
        for a in asteroids:
            conn.execute("""
                INSERT INTO asteroids
                    (name, close_approach_date, miss_distance_km, diameter_m, hazardous, velocity_kmh, fetched_at)
                VALUES
                    (:name, :close_approach_date, :miss_distance_km, :diameter_m, :hazardous, :velocity_kmh, :fetched_at)
                ON CONFLICT(name, close_approach_date) DO UPDATE SET
                    miss_distance_km = excluded.miss_distance_km,
                    fetched_at       = excluded.fetched_at
            """, {**a, "hazardous": int(a.get("hazardous", False)), "fetched_at": now})


def get_asteroid_history(limit: int = 50) -> list:
    with _conn() as conn:
        rows = conn.execute(
            "SELECT * FROM asteroids ORDER BY close_approach_date DESC, miss_distance_km ASC LIMIT ?",
            (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


# ─── Weather ──────────────────────────────────────────────────────────────────

def insert_weather(data: dict):
    with _conn() as conn:
        conn.execute("""
            INSERT INTO weather (timestamp, temp_c, condition, icon, lat, lon)
            VALUES (:timestamp, :temp_c, :condition, :icon, :lat, :lon)
        """, {**data, "timestamp": int(time.time())})


def get_weather_history(limit: int = 48) -> list:
    with _conn() as conn:
        rows = conn.execute(
            "SELECT * FROM weather ORDER BY timestamp DESC LIMIT ?", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]


# Init on import
init()
