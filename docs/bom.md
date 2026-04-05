# orrery — Bill of Materials

Sourced from **Amazon.de** for prototyping. Prices are approximate — verify before ordering.
Switch to AliExpress / LCSC for production runs (expect 3–5× cheaper on most lines).

> **Per-unit target cost (Amazon.de):** ~€75–85
> **Per-unit target cost (AliExpress):** ~€20–30

---

## Core components

| # | Component | Variant / Notes | Qty | Unit price | Line total | Link |
|---|-----------|-----------------|-----|-----------|------------|------|
| 1 | **ESP32-S3 DevKitC-1** | **N16R8** — 16MB flash, 8MB Octal PSRAM, USB-C, 45 GPIO | 1 | ~€21 | ~€21 | [Amazon.de](https://www.amazon.de/ESP32-S3-WROOM-1-N16R8-ESP32-S3-DevKitC-1-Entwicklung-Bluetooth/dp/B0C6KN35L2) |
| 2 | **64×32 HUB75 LED Matrix** | P3 (3mm pitch) — good balance of size vs. readability at desk distance | 1 | ~€28 | ~€28 | [Amazon.de](https://www.amazon.de/RGB-Vollfarb-LED-Matrix-Panel-Bildanimation-Individuelle-RGB-Anzeigetafel-HUB75-Schnittstelle/dp/B0C4F5NHPF) |
| 3 | **5V 4A PSU** | 5.5×2.1mm barrel jack — panel draws up to 4A peak at full white | 1 | ~€9 | ~€9 | [Amazon.de](https://www.amazon.de/20W-Adapter-Netzteil-5mm-4A_5V_U-5-5x2-5x10-mm/dp/B0FCMG445F) |
| 4 | **MAX98357A I2S amplifier** | Class D, 3W mono, filterless — I2S direct from ESP32, no DAC needed | 1 | ~€8 | ~€8 | [Amazon.de](https://www.amazon.de/AZDelivery-filterloses-Breakout-Modul-Decoder-Modul-Anwendungsbereich/dp/B09PL7DSK5) |
| 5 | **Speaker** | 40mm, 4Ω, 3W — matches MAX98357A output | 1 | ~€5 | ~€5 | [Amazon.de](https://www.amazon.de/Adafruit-Speaker-Diameter-Ohm-Watt/dp/B07KKCSSRM) |
| 6 | **Tactile push buttons** | 6×6mm through-hole, assorted heights — 3 buttons needed | 1 kit | ~€7 | ~€7 | [Amazon.de](https://www.amazon.de/Jolicobo-Tactile-Button-Assortment-Arduino/dp/B07RFKJQ7S) |
| 7 | **HUB75 IDC ribbon cable** | 16-pin 2×8, 2.54mm pitch, ~30cm | 1 | ~€5 | ~€5 | [Amazon.de](https://www.amazon.de/-/en/Popesq%C2%AE-16-Pin-Cable-Ribbon-A1317/dp/B076Z19NJ4) |

**Subtotal: ~€83**

---

## Notes per line

**1 — ESP32-S3 DevKitC-1 (N16R8)**
Upgraded from N8R2. N16R8 = 16MB flash + 8MB Octal PSRAM (vs. 2MB Quad on N8R2).
The extra flash comfortably holds firmware + OTA slot + audio clip library.
The Octal PSRAM runs on a wider bus — better throughput for simultaneous HUB75 DMA + audio decode.
Note: GPIO 26–32 are consumed by the Octal PSRAM bus internally and unavailable — already avoided in pin table.

**2 — 64×32 P3 LED Matrix**
P3 = 3mm pixel pitch → 192mm × 96mm physical panel size.
Confirm listing says **HUB75** (not HUB08/12) and **1:16 scan** (not 1:32 — that halves brightness).

**3 — 5V 4A PSU**
Panel peaks at ~4A at full white. Real usage (dark scenes, partial brightness) is 0.5–1A.
A barrel jack (5.5×2.1mm) feeds the panel's screw terminal via a short pigtail.
ESP32 and MAX98357A take 5V from the same rail.
**Future upgrade:** Mean Well LRS-35-5 (5V/7A, ~€18) for a permanent installation, or
USB-C PD trigger board for a single-cable desk setup.

**4 — MAX98357A I2S Amplifier**
Takes I2S digital audio directly from the ESP32-S3 — no separate DAC needed.
Class D (switching), filterless — efficient and simple.
Pinout: BCLK, WS (LRCLK), DATA from ESP32 + SD pin for gain/mute control.
Drives the 4Ω speaker at up to 3W from a 5V supply.
Mono. Stereo would need two chips — overkill for a desk briefing device.

**5 — Speaker (40mm 4Ω 3W)**
40mm diameter fits behind or beside the panel in any enclosure design.
4Ω matches the MAX98357A's rated load for full 3W output.
Adequate for voice, tones, and ambient sound at desk volume.
The Adafruit unit has a JST connector which is convenient for prototyping.

**6 — Tactile buttons**
3 buttons: next scene (GPIO 33), brightness (GPIO 34), WiFi/setup (GPIO 35).
A kit gives spare heights for fitting through an enclosure lid.

**7 — HUB75 ribbon cable**
Connects ESP32 breakout to the panel's HUB75 input connector.
The panel ships with its own short cable between input and output (for daisy-chain) — that one is for cascading, not for connecting to the controller.

---

## Not in BOM (already have / not needed yet)

| Item | Reason |
|------|--------|
| USB-C cable | For flashing ESP32 — any standard cable works |
| Breadboard + jumper wires | For initial wiring before PCB |
| Soldering iron | Assumed available |
| 3D printed enclosure | Future — design TBD once panel + board are in hand |
| Custom PCB | Future — replaces DevKitC + breadboard for production units |

---

## Cheaper alternatives (AliExpress — longer lead time)

| Component | AliExpress price | Notes |
|-----------|-----------------|-------|
| ESP32-S3-WROOM-1-N16R8 module (bare) | ~€4–5 | No USB, needs own circuitry — for PCB only |
| 64×32 P3 HUB75 panel | ~€8–12 | Same panels, direct from factory |
| 5V 4A PSU | ~€3–5 | Unbranded — check CE marking |
| MAX98357A breakout | ~€1–2 | Bare module, no headers |
| 40mm 4Ω speaker | ~€1–2 | Loose, no connector |
| Tactile buttons (100×) | ~€1 | Loose, not kitted |
