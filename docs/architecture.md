# orrery — System Architecture

End-to-end data pipeline: NASA and weather APIs are fetched hourly by GitHub Actions, committed as static JSON to the repo, then consumed by both the browser simulator and the ESP32 firmware.

```mermaid
graph LR
    subgraph apis [" External APIs "]
        NASA["🛸 NASA APIs<br/>APOD · EONET · NeoWs"]
        OWM["🌦 OpenWeather<br/>current + forecast"]
    end

    subgraph ci [" GitHub Actions  (hourly) "]
        GEN["generate.py"]
    end

    subgraph repo [" GitHub Pages (static) "]
        DJ["data/daily.json<br/>weather · events · asteroids · apod"]
        FJ["data/frame.json<br/>64×32 pixel array"]
    end

    subgraph clients [" Clients "]
        SIM["Browser simulator<br/>display/index.html"]
        ESP["ESP32 firmware<br/>C · WiFi"]
    end

    subgraph matrix [" Hardware "]
        LED["64×32 HUB75<br/>LED Matrix"]
    end

    NASA -->|HTTPS| GEN
    OWM  -->|HTTPS| GEN
    GEN  --> DJ
    GEN  --> FJ
    DJ   --> SIM
    FJ   --> SIM
    DJ   -->|HTTPS GET| ESP
    ESP  -->|HUB75 parallel| LED
```
