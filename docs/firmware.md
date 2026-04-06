# orrery — Firmware Design

## State machine

Scenes marked `*` are conditional — skipped when no relevant data is available,
mirroring the browser simulator logic.

```mermaid
stateDiagram-v2
    [*] --> Boot

    Boot --> BLEProvision : no WiFi creds in NVS
    Boot --> WiFiConnect  : creds present

    WiFiConnect --> FetchData    : connected
    WiFiConnect --> DisplayCached : timeout (30s)

    FetchData --> ParseJSON    : 200 OK
    FetchData --> DisplayCached : error / offline

    ParseJSON --> AudioSync    : valid data
    ParseJSON --> DisplayCached : parse error

    AudioSync --> DisplayScenes : mp3 current or downloaded
    AudioSync --> DisplayScenes : download failed (use cached)

    DisplayCached --> DisplayScenes : always continues

    DisplayScenes --> DisplayScenes : tick → next scene
    DisplayScenes --> OTACheck    : midnight trigger
    DisplayScenes --> BLEProvision : WiFi button held 5s

    OTACheck --> OTADownload  : newer version found
    OTACheck --> DisplayScenes : up to date

    OTADownload --> Boot  : success → reboot into new slot
    OTADownload --> DisplayScenes : download failed → continue

    BLEProvision --> WiFiConnect : creds saved
    BLEProvision --> DisplayScenes : cancelled

    note right of AudioSync
        Reads daily.json "generated" date.
        Compares to date stored in NVS.
        If different: GET briefing.mp3 → LittleFS.
        Update NVS date. ~350KB, once per day.
    end note

    note right of DisplayScenes
        Scene rotation:
        Morning → Day → Tomorrow
        → Asteroid* → Event* → Night → APOD
        (* skipped if no data)
        Radio scene: plays briefing.mp3 from LittleFS.
    end note

    note right of OTACheck
        Runs once per day at ~02:00 local.
        Checks data/version.json on GitHub Pages.
        Rolls back after 1 crash on new slot.
    end note

    note right of BLEProvision
        First boot: automatic (no creds).
        Re-config: hold GPIO 35 for 5s.
        Display shows SETUP + BLE name.
    end note
```

---

## Infrastructure decisions

| Concern | Decision | Notes |
|---------|----------|-------|
| **Initial flashing** | UF2 drag-and-drop | Device appears as USB drive; no terminal needed for recipients |
| **Provisioning — first boot** | Auto if NVS has no credentials | No button press needed out of the box |
| **Provisioning — re-config** | Hold GPIO 35 (WiFi button) for 5s | Works during normal display operation |
| **Provisioning protocol** | ESP-IDF `wifi_provisioning` over BLE | Espressif phone app or custom app; credentials written to NVS |
| **Display during provisioning** | LED matrix shows `SETUP` + device BLE name | Future: QR code for pairing |
| **OTA check schedule** | Once per day at ~02:00 local time | Low-traffic window; uses `esp_timer` or SNTP-synced RTC |
| **OTA manifest** | `data/version.json` on GitHub Pages | Same host as `daily.json`; no separate server needed |
| **OTA binary host** | GitHub Releases asset (`.bin`) | Tagged release triggers CI build |
| **OTA rollback** | After 1 crash on new slot | ESP-IDF `esp_ota_mark_app_invalid_rollback_and_reboot()` |
| **CI trigger** | Git tag push (e.g. `v1.2.0`) | Separate workflow from data-fetch; produces `firmware.bin` |
| **Credentials storage** | ESP-IDF NVS (non-volatile storage) | Survives firmware updates; encrypted NVS optional later |

---

## Partition table

Dual-OTA layout on the ESP32-S3 N16R8 (16MB flash). OTA slots are sized at
1.5MB each to leave headroom for future growth; the remaining ~13MB is a
LittleFS filesystem partition used to cache audio assets.

```
# Name       Type  SubType   Offset    Size        Notes
nvs          data  nvs       0x9000    0x6000      WiFi creds, config, OTA state (24KB)
otadata      data  ota       0xf000    0x2000      Tracks which OTA slot is active (8KB)
phy_init     data  phy       0x11000   0x1000      RF calibration data (4KB)
ota_0        app   ota_0     0x20000   0x180000    Slot A — 1.5MB
ota_1        app   ota_1     0x1A0000  0x180000    Slot B — 1.5MB
storage      data  spiffs    0x320000  0xCE0000    LittleFS — ~13MB (audio assets)
```

Memory budget (estimated):
| Region | Size | Contents |
|--------|------|----------|
| OTA slot | 1.5MB | ESP-IDF + wifi_provisioning + OTA + HUB75 driver + JSON + scene logic |
| LittleFS | ~13MB | `briefing.mp3` (~350KB), `frame.bin` (future), room to grow |

**Slot switching:**
- `ota_0` is the factory slot (first flash via UF2)
- OTA downloads new firmware into `ota_1`, validates, reboots
- `otadata` partition records which slot to boot from
- If `ota_1` crashes on first boot → bootloader rolls back to `ota_0`

---

## Audio storage

The daily briefing MP3 is downloaded once and cached in the LittleFS
`storage` partition. It is never streamed live — playback reads from flash.

**Flow:**
1. After a successful `FetchData`, firmware reads the `generated` field from
   `daily.json` (e.g. `"2026-04-06"`) and compares it to the date stored in NVS.
2. If the date differs (new day), firmware downloads `data/briefing.mp3` from
   GitHub Pages and writes it to `/storage/briefing.mp3` on LittleFS.
3. NVS is updated with the new date so the file is not re-downloaded until
   tomorrow.
4. On button press (or scheduled morning play), the firmware reads the cached
   MP3 from LittleFS and streams it to the MAX98357A over I2S.

**Why this approach:**
- No mid-sentence network dropout risk
- Instant start (no buffer fill delay)
- Works while WiFi is reconnecting or momentarily offline
- ~350KB is well within the 13MB partition

**Cache invalidation key in `daily.json`:**
```json
{
  "generated": "2026-04-06",
  ...
}
```

---

## OTA version manifest

A tiny JSON file committed to the repo alongside `daily.json`:

```json
{
  "version": "1.0.0",
  "url": "https://github.com/art-mon/orrery/releases/download/v1.0.0/firmware.bin",
  "min_flash_kb": 768,
  "notes": "Initial release"
}
```

Firmware compares the `version` string against its own build-time version.
If newer, downloads from `url` directly into the inactive OTA slot.

---

## CI — firmware build workflow (future)

Triggered by a git tag (`v*`). Separate from the hourly data-fetch workflow.

```
on: push tags: ["v*"]

steps:
  1. Checkout repo
  2. Install ESP-IDF (idf-component-manager cache)
  3. idf.py build
  4. Upload firmware/build/orrery.bin as GitHub Release asset
  5. Update data/version.json with new tag + asset URL
  6. Commit version.json back to main [skip ci]
```

This means releasing a new firmware version is just:
```bash
git tag v1.2.0 && git push origin v1.2.0
```
Devices pick it up overnight.
