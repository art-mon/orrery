# orrery — Bill of Materials

Sourced from **Amazon.de** for prototyping. Prices are approximate — verify before ordering.
Switch to AliExpress / LCSC for production runs (expect 3–5× cheaper on most lines).

> **Per-unit target cost (Amazon.de):** ~€55–65
> **Per-unit target cost (AliExpress):** ~€15–25

---

## Core components

| # | Component | Variant / Notes | Qty | Unit price | Line total | Link |
|---|-----------|-----------------|-----|-----------|------------|------|
| 1 | **ESP32-S3 DevKitC-1** | N8R2 — 8MB flash, 2MB PSRAM, USB-C, 45 GPIO | 1 | ~€18 | ~€18 | [Amazon.de](https://www.amazon.de/Espressif-ESP32-S3-DevKitC-1-N8R2-Entwicklungsplatine/dp/B09D3S7T3M) |
| 2 | **64×32 HUB75 LED Matrix** | P3 (3mm pitch) — good balance of size vs. readability at desk distance | 1 | ~€28 | ~€28 | [Amazon.de](https://www.amazon.de/RGB-Vollfarb-LED-Matrix-Panel-Bildanimation-Individuelle-RGB-Anzeigetafel-HUB75-Schnittstelle/dp/B0C4F5NHPF) |
| 3 | **5V 4A PSU** | 5.5×2.1mm barrel jack — panel draws up to 4A peak at full white | 1 | ~€9 | ~€9 | [Amazon.de](https://www.amazon.de/20W-Adapter-Netzteil-5mm-4A_5V_U-5-5x2-5x10-mm/dp/B0FCMG445F) |
| 4 | **Tactile push buttons** | 6×6mm through-hole, assorted heights — 3 buttons needed (next/brightness/WiFi) | 1 kit | ~€7 | ~€7 | [Amazon.de](https://www.amazon.de/Jolicobo-Tactile-Button-Assortment-Arduino/dp/B07RFKJQ7S) |
| 5 | **HUB75 IDC ribbon cable** | 16-pin 2×8, 2.54mm pitch, ~30cm — connects ESP32 breakout to panel | 1 | ~€5 | ~€5 | [Amazon.de](https://www.amazon.de/-/en/Popesq%C2%AE-16-Pin-Cable-Ribbon-A1317/dp/B076Z19NJ4) |

**Subtotal: ~€67**

---

## Notes per line

**1 — ESP32-S3 DevKitC-1 (N8R2)**
The official Espressif dev board. N8R2 = 8MB flash + 2MB PSRAM. Enough for firmware + frame buffer.
Avoid the N8 (no PSRAM) — the HUB75 DMA library benefits from PSRAM for the pixel double-buffer.
The N16R8 variant (16MB flash, 8MB PSRAM) exists if we later add OTA or larger assets — ~€3 more.

**2 — 64×32 P3 LED Matrix**
P3 = 3mm pixel pitch → 192mm × 96mm physical panel size. Good for a desk object at 0.5–2m viewing distance.
P4 (4mm) is cheaper and larger but loses sharpness up close. P2.5 is sharper but more expensive and smaller.
Make sure the listing says **HUB75** (not HUB08 or HUB12) — those have different pinouts.
Check for **1:16 scan** in the spec — some cheap P3 panels are 1:32 scan which halves brightness.

**3 — 5V 4A PSU**
The panel alone can draw ~4A at full white across all 2048 LEDs. In practice (dark scenes, dimmed) it's
closer to 0.5–1A. The ESP32 adds ~0.25A. A 4A / 20W supply is the safe minimum; 5A is more comfortable.
Use a **barrel jack (5.5×2.1mm)** not USB-C — the panel's power connector is a screw terminal or JST,
and you'll want to split the 5V rail between the panel and the ESP32 3.3V LDO.

**4 — Tactile buttons**
Only 3 needed. A kit gives you spares and different stem heights for panel depth flexibility.
For the final PCB these will be replaced with proper panel-mount buttons.

**5 — HUB75 ribbon cable**
The panel ships with a short cable between input and output connectors (for daisy-chaining).
You need a separate cable from the ESP32 breakout board to the panel's input HUB75 connector.
16-pin IDC, 2.54mm pitch. ~30cm is enough for prototyping.

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
| ESP32-S3-WROOM-1 module (bare) | ~€3–4 | No USB, needs own circuitry — for PCB only |
| 64×32 P3 HUB75 panel | ~€8–12 | Same panels, direct from factory |
| 5V 4A PSU | ~€3–5 | Unbranded — check CE marking |
| Tactile buttons (100×) | ~€1 | Loose, not kitted |
