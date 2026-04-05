# orrery — Hardware Block Diagram

ESP32-S3 driving a 64×32 HUB75 LED panel. The panel uses 1:16 multiplexed scanning
(top and bottom 16 rows driven simultaneously via R1/G1/B1 and R2/G2/B2), so only
4 address lines are needed — not 5. Total GPIO usage is 18 pins out of 45 available.

```mermaid
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
```

## Pin assignment table

| Signal | GPIO | Notes |
|--------|------|-------|
| R1     | 4    | Top-half red |
| G1     | 5    | Top-half green |
| B1     | 6    | Top-half blue |
| R2     | 7    | Bottom-half red |
| G2     | 15   | Bottom-half green |
| B2     | 16   | Bottom-half blue |
| A      | 36   | Row address bit 0 |
| B      | 37   | Row address bit 1 |
| C      | 38   | Row address bit 2 |
| D      | 39   | Row address bit 3 |
| CLK    | 2    | Shift register clock |
| LAT    | 47   | Latch — transfers shift register to outputs |
| OE     | 48   | Output enable (active low — controls brightness via PWM) |
| BTN_NEXT  | 33 | Scene advance |
| BTN_BRITE | 34 | Brightness cycle |
| BTN_WIFI  | 35 | Hold to enter BLE provisioning |
| UART TX   | 43 | Debug serial (S3 default) |
| UART RX   | 44 | Debug serial (S3 default) |

**Avoided GPIOs on ESP32-S3:**
- GPIO 0: strapping pin (boot mode — leave floating or pull high)
- GPIO 19/20: USB D−/D+ (needed if using USB-OTG for flashing)
- GPIO 26–32: internal flash / PSRAM bus on Octal PSRAM variants

Pin assignments match the defaults used by the
[ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)
library, which drives HUB75 via the I2S DMA peripheral — zero CPU overhead during refresh.
