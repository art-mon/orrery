# NASA Daily Dashboard — Ideas & Requirements

A daily briefing device that pulls NASA data, displays it on a 64×32 LED matrix,
and sparks curiosity. Multiple cheap identical devices, no per-device config.

---

## Hardware Target

- **MCU**: ESP32 (WiFi + Bluetooth for initial config)
- **Display**: 64×32 HUB75 RGB LED matrix
- **Input**: A few physical buttons
- **Form factor**: Custom PCB, cheap BOM, mass-identical units
- **Power**: TBD — wall adapter? USB-C? battery?

---

## Architecture

```
[NASA APIs] --> [Python server] --> [ESP32 devices] --> [LED matrix]
```

- Server fetches, caches, and serves compact JSON
- Devices poll server over WiFi — all get the same data
- Bluetooth used for initial WiFi credentials config (no hardcoded credentials)
- Server can run on a Raspberry Pi, VPS, home server, etc.

---

## Data Sources (so far)

| Source | What | Refresh |
|--------|------|---------|
| APOD | Astronomy picture of the day + explanation | Daily |
| EONET | Active natural events (wildfires, storms, volcanoes…) | 6h |
| NeoWs | Near-Earth asteroids this week | Daily |

---

## Display Ideas

- [ ] Scrolling text for APOD title / explanation snippet
- [ ] World map (pixel art) with EONET event markers
- [ ] Asteroid proximity bar / countdown to closest approach
- [ ] Day/night cycle visualization on the globe
- [ ] Weather-style icons for event types (flame = wildfire, spiral = storm…)
- [ ] Starfield screensaver when idle
- [ ] Color-coded hazard level for asteroids (green → yellow → red)
- [ ] Show APOD image downsampled to 64×32 pixels
- [ ] Clock mode between data screens
- [ ] Smooth transitions / animations between screens

---

## Interaction (Buttons)

- [ ] Cycle through screens (APOD / Events / Asteroids)
- [ ] "Tell me more" — scroll the full explanation text
- [ ] Brightness up/down
- [ ] Enter BT config mode (hold on boot?)
- [ ] Mute / sleep mode

---

## Wild Ideas

- [ ] **Sound**: small speaker — audio alerts, space ambient, radio-style daily briefing (TTS + music)
- [ ] **ISS tracker**: show current ISS position on the world map in real time
- [ ] **Solar flare alerts**: pull DONKI (Space Weather) API for geomagnetic storms
- [ ] **Mars weather**: Curiosity / Perseverance temperature + wind on Mars
- [ ] **Launch schedule**: next rocket launches (from a launch API)
- [ ] **Exoplanet of the week**: highlight a newly confirmed exoplanet
- [ ] **Moon phase**: animated moon phase display
- [ ] **Perseverance latest photo**: downsampled Mars rover image of the day
- [ ] **Constellation of the night**: what's visible tonight from your lat/lon
- [ ] **Space quote**: rotating quotes from astronauts, scientists
- [ ] **Satellite count**: how many objects are currently in orbit
- [ ] **"On this day in space"**: historical space events for today's date
- [ ] **Aurora alert**: Kp index — notify when northern lights are likely visible
- [ ] **Binary clock mode**: because why not
- [ ] **Gravity assist viz**: when a spacecraft is doing a flyby, show the trajectory

---

## Scene Schedule (draft)

| Time | Scene | Art | Info |
|------|-------|-----|------|
| 06:00–09:00 | Morning | Sunrise gradient, setting moon, stars fading | Time, temp, APOD title, ISS pass if overhead |
| 09:00–18:00 | Daytime | Sky + clouds, sun | Time, temp, EONET events or asteroid ticker |
| 18:00–22:00 | Evening | Sunset gradient → dusk | Time, temp, APOD title |
| 22:00–00:00 | Night | Moon phase art, starfield, silhouettes | Dim time + temp, moon phase label |
| 00:00–06:00 | Deep Night | Milky Way, crescent | Very dim time only, minimum brightness |
| Any time | Asteroid Alert | Deep space, Earth, trajectory arc | Name, miss distance, diameter, date, hazard badge |
| Any time | Earth Event | Pixel world map, pulsing markers | Event names scrolling |

- Asteroid and Event scenes can interrupt the normal schedule as overlays or on button press
- Brightness auto-dims at night

---

## Open Questions

- Where does the server live? (local Pi, home server, VPS, user's choice?)
- How do devices get the server address? (BT config, mDNS, hardcoded fallback?)
- OTA firmware updates? (ESP32 supports it, worth planning for)
- Multiple "channels" / themes? (e.g., space-only mode vs. earth-events mode)
- How do we handle no-WiFi / server-down gracefully on the device?
- Display brightness schedule? (dim at night)
- Should the server also serve pre-rendered pixel frames for the matrix?

---

## Nice to Have (later)

- Web UI to preview what the device is showing (mirror the matrix in browser)
- Admin page to push custom messages to all devices
- Per-device name/location (for constellation visibility, aurora alerts)
- Public API so others can run their own server instances

---

## Parking Lot (too early, but don't forget)

- PCB design considerations: decoupling caps on HUB75 data lines, level shifting (ESP32 is 3.3V, HUB75 expects 5V signals)
- Power budget: HUB75 64×32 at full white ≈ 2A @ 5V — needs a decent regulator
- Enclosure / mounting: wall-mount? desk stand? transparent front panel?

---

## Other Ideas

- The idea for the project is to be some kind of an art project, where I will develop some artwork (mostly with AI) based on the info to be displayed.
- It would also be cool if we could eventually also broadcast maybe a few tunes per day (as per request from the user) and some talking as a radio program.
- It won't be very modifyable.
  - it will be a personal project (mine) to share with my friends.
  - The Hardware will be very cheap so that I can even give it away initially.
  - The Software will be of good quality, but without that many features, so that I can focus on the art.
- The platform must support a bootloader since I might need to improve the SW functionalities.
- The art will be based on the information, mostly from NASA.
  - Moon information will be displayed by night and in the morning, with some art referencing the phase of the moon.
  - Weather information with some simple animation.
- The device will constantly show the hour, the weather, and have some art as a background.
  - It shouldn't be invasive.
  - It shouldn't have too many animations.
- It should have different heues during the different phases of the day.
  - not always dark and clear themes, but also different selection of colors.
  - Maybe based on the weather and on the season.
- it should also be possible to make some QR codes to cool websites.