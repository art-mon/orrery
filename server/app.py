from dotenv import load_dotenv
load_dotenv()

from flask import Flask, jsonify
from flask_cors import CORS
import nasa
import image
import db
import weather
import uv

app = Flask(__name__)
CORS(app)


@app.route("/apod")
def route_apod():
    return jsonify(nasa.apod())


@app.route("/events")
def route_events():
    events = nasa.events()
    quakes = nasa.earthquakes()
    if not events.get("error") and not quakes.get("error"):
        events["events"] = (events.get("events") or []) + (quakes.get("events") or [])
    elif events.get("error") and not quakes.get("error"):
        events = quakes
    return jsonify(events)


@app.route("/asteroids")
def route_asteroids():
    return jsonify(nasa.asteroids())


@app.route("/apod/frame")
def route_apod_frame():
    apod = nasa.apod()
    if apod.get("media_type") != "image":
        return jsonify({"error": "today's APOD is not an image"}), 204
    return jsonify(image.apod_frame(apod["url"]))


@app.route("/weather")
def route_weather():
    return jsonify(weather.current())


@app.route("/weather/forecast")
def route_weather_forecast():
    return jsonify(weather.forecast_tomorrow())


@app.route("/uv")
def route_uv():
    from flask import request
    try:
        skin = int(request.args.get("skin", 2))
    except ValueError:
        skin = 2
    skin = max(1, min(6, skin))
    return jsonify(uv.current(skin))


@app.route("/daily")
def route_daily():
    def safe(fn):
        try:
            return fn()
        except Exception as e:
            return {"error": str(e)}

    return jsonify({
        "apod":      safe(nasa.apod),
        "events":    safe(lambda: {
            "events": (safe(nasa.events).get("events") or []) +
                      (safe(nasa.earthquakes).get("events") or [])
        }),
        "asteroids": safe(nasa.asteroids),
        "weather":            safe(weather.current),
        "forecast_today":     safe(weather.forecast_today),
        "forecast":           safe(weather.forecast_tomorrow),
        "forecast_day_after": safe(weather.forecast_day_after_tomorrow),
        "uv":                 safe(uv.current),
    })


@app.route("/history/apod")
def route_history_apod():
    return jsonify(db.get_apod_history())


@app.route("/history/events")
def route_history_events():
    return jsonify(db.get_event_history())


@app.route("/history/asteroids")
def route_history_asteroids():
    return jsonify(db.get_asteroid_history())


@app.route("/history/weather")
def route_history_weather():
    return jsonify(db.get_weather_history())


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001, debug=True)
