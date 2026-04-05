"""
orrery — programmatic diagram generator
Outputs:
  docs/architecture.md   — system + data pipeline (Mermaid)
  docs/hardware.md       — ESP32 + HUB75 block diagram (Mermaid)
  docs/firmware.md       — firmware state machine (Mermaid)
  docs/hardware.svg      — ESP32 + HUB75 pin schematic (schemdraw)
"""

from pathlib import Path
import textwrap

OUT = Path(__file__).parent


# ─── helpers ──────────────────────────────────────────────────────────────────

def mermaid_doc(title: str, description: str, diagram: str) -> str:
    """Wrap a Mermaid diagram in a markdown document."""
    return f"# {title}\n\n{description}\n\n```mermaid\n{textwrap.dedent(diagram).strip()}\n```\n"


# ─── 1. Architecture ──────────────────────────────────────────────────────────

ARCH_DIAGRAM = """
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
"""

ARCH_DESC = (
    "End-to-end data pipeline: NASA and weather APIs are fetched hourly by GitHub Actions, "
    "committed as static JSON to the repo, then consumed by both the browser simulator and "
    "the ESP32 firmware."
)


# ─── 2. Hardware block diagram ────────────────────────────────────────────────

HW_DIAGRAM = """
graph TD
    subgraph psu [" Power "]
        USB["USB-C / 5V barrel"]
        REG["3.3V LDO"]
    end

    subgraph esp [" ESP32-S3 "]
        CPU["Xtensa LX7 240 MHz"]
        WIFI["WiFi 802.11 b/g/n"]
        BT["Bluetooth 5"]
        NVRAM["NVS Flash<br/>WiFi creds · config"]
        BTN["GPIO 33 34 35<br/>next · brightness · WiFi setup"]
        UART["GPIO 43 44<br/>UART debug TX RX"]
    end

    subgraph hub75 [" HUB75 connector (13 pins) "]
        RGB["R1 G1 B1  →  GPIO 4 5 6<br/>R2 G2 B2  →  GPIO 7 15 16<br/>6× color data (top + bottom half)"]
        ADDR["A B C D  →  GPIO 36 37 38 39<br/>4× row select  ·  2⁴ = 16 rows"]
        CTRL["CLK → GPIO 2  ·  LAT → GPIO 47  ·  OE → GPIO 48<br/>3× timing control"]
    end

    subgraph panel [" 64×32 LED Panel "]
        LEDS["2048 RGB LEDs<br/>~20W peak @ full white"]
    end

    USB --> REG
    USB -->|"5V direct"| LEDS
    REG -->|"3.3V"| CPU
    CPU --> WIFI
    CPU --> BT
    CPU --> NVRAM
    CPU --> BTN
    CPU --> UART
    CPU -->|"6 pins"| RGB
    CPU -->|"4 pins"| ADDR
    CPU -->|"3 pins"| CTRL
    RGB  --> panel
    ADDR --> panel
    CTRL --> panel
"""

HW_DESC = (
    "ESP32-S3 driving a 64×32 HUB75 LED panel. The panel uses 1:16 multiplexed scanning "
    "(top and bottom 16 rows driven simultaneously), so only 4 address lines are needed. "
    "Total GPIO usage is 18 pins out of 45 available."
)


# ─── 3. Firmware state machine ────────────────────────────────────────────────

FW_DIAGRAM = """
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
"""

FW_DESC = (
    "Firmware state machine for the ESP32. Scenes marked with * are conditional — "
    "they are skipped when no relevant data is available (mirrors the browser simulator logic)."
)


# ─── write markdown files ─────────────────────────────────────────────────────

(OUT / "architecture.md").write_text(mermaid_doc("orrery — System Architecture", ARCH_DESC, ARCH_DIAGRAM))
(OUT / "hardware.md").write_text(mermaid_doc("orrery — Hardware Block Diagram", HW_DESC, HW_DIAGRAM))
(OUT / "firmware.md").write_text(mermaid_doc("orrery — Firmware State Machine", FW_DESC, FW_DIAGRAM))

print("✓ architecture.md")
print("✓ hardware.md")
print("✓ firmware.md")


# ─── 4. Hardware pin schematic (schemdraw) ────────────────────────────────────

try:
    import schemdraw
    import schemdraw.elements as elm
    import matplotlib
    matplotlib.use("Agg")  # headless

    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=9, inches_per_unit=0.5)

        # Right-side pins: HUB75 signals
        right_pins = [
            elm.IcPin(name="R1 G1 B1", pin="17-19", side="right", slot="1/6"),
            elm.IcPin(name="R2 G2 B2", pin="20-22", side="right", slot="2/6"),
            elm.IcPin(name="A B C D E", pin="1-5",  side="right", slot="3/6"),
            elm.IcPin(name="CLK",       pin="6",    side="right", slot="4/6"),
            elm.IcPin(name="LAT",       pin="7",    side="right", slot="5/6"),
            elm.IcPin(name="OE",        pin="8",    side="right", slot="6/6"),
        ]
        # Left-side pins: power + buttons
        left_pins = [
            elm.IcPin(name="3.3V",      pin="3V3",  side="left", slot="1/4"),
            elm.IcPin(name="GND",       pin="GND",  side="left", slot="2/4"),
            elm.IcPin(name="BTN NEXT",  pin="9",    side="left", slot="3/4"),
            elm.IcPin(name="BTN BRITE", pin="10",   side="left", slot="4/4"),
        ]

        ic = d.add(elm.Ic(pins=right_pins + left_pins, edgepadH=0.5, edgepadW=1.0)
                   .label("ESP32-S3", loc="center", fontsize=11))

        d.save(str(OUT / "hardware_pins.svg"))
        print("✓ hardware_pins.svg")

except Exception as e:
    print(f"⚠  schemdraw pin schematic skipped: {e}")
    print("   (see hardware.md for the block diagram)")
