# orrery — firmware

ESP-IDF project for the ESP32-S3 driving a 64×32 HUB75 panel.

Pin map and architecture: see [`docs/hardware.md`](../docs/hardware.md) and [`docs/firmware.md`](../docs/firmware.md).

## Milestone status

- [x] **M1** — Panel test pattern (this commit)
- [ ] M2 — Buttons + brightness control
- [ ] M3 — WiFi + fetch `daily.json` + one scene
- [ ] M4 — Scene rotation
- [ ] M5 — I2S audio (deferred — no MAX98357A on hand yet)
- [ ] M6 — BLE provisioning + NVS
- [ ] M7 — OTA + `version.json`

## Build / flash

Requires ESP-IDF ≥ 5.1.

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

First build pulls the HUB75 component from the IDF component registry — that's a few minutes only the first time.

## Wiring

Power the panel from a separate 5V supply (USB-C off the dev board is *not* enough current — 20W peak at full white). Tie the panel ground to the ESP32 ground.

If the test pattern shows wrong colours, the panel uses BGR ordering — swap the R/B pin pairs in [`main/pins.h`](main/pins.h) rather than editing the draw code.
