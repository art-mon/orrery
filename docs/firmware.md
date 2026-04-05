# orrery — Firmware State Machine

Firmware state machine for the ESP32. Scenes marked with * are conditional — they are skipped when no relevant data is available (mirrors the browser simulator logic).

```mermaid
stateDiagram-v2
    [*] --> Boot

    Boot --> WiFiConnect : power on
    Boot --> DisplayCached : no WiFi creds

    WiFiConnect --> FetchData : connected
    WiFiConnect --> BLEProvision : held BOOT btn
    WiFiConnect --> DisplayCached : timeout (30s)

    BLEProvision --> WiFiConnect : creds saved
    BLEProvision --> DisplayCached : cancel

    FetchData --> ParseJSON : 200 OK
    FetchData --> DisplayCached : error / offline

    ParseJSON --> DisplayScenes : valid data
    ParseJSON --> DisplayCached : parse error

    DisplayCached --> DisplayScenes : always continues

    DisplayScenes --> DisplayScenes : tick → next scene
    DisplayScenes --> FetchData : refresh interval (1h)
    DisplayScenes --> BLEProvision : held BOOT btn

    note right of DisplayScenes
        Scene rotation:
        Morning → Day → Tomorrow
        → Asteroid* → Event* → Night → APOD
        (* only if data present)
    end note
```
