#pragma once

// HUB75 — matches docs/hardware.md
#define PIN_R1   4
#define PIN_G1   5
#define PIN_B1   6
#define PIN_R2   7
#define PIN_G2   15
#define PIN_B2   16
// Address pins live in the 1-21 range so they don't clash with the
// integrated octal PSRAM (which consumes GPIO 26-37 on N16R8 modules).
#define PIN_A    17
#define PIN_B    18
#define PIN_C    21
#define PIN_D    14
#define PIN_E    -1   // unused on 64x32 1:16
#define PIN_CLK  2
#define PIN_LAT  47
#define PIN_OE   48

// Rotary encoder — KY-040 (module has built-in 10 kΩ pullups on CLK/DT)
#define PIN_ENC_CLK    40
#define PIN_ENC_DT     39
#define PIN_ENC_SW     38

// I2C bus — BH1750 ambient light sensor (default address 0x23, ADDR floating)
#define PIN_I2C_SDA    41
#define PIN_I2C_SCL    42

// I2S audio — MAX98357A Class-D amplifier (see docs/hardware.md)
#define PIN_I2S_BCLK   9
#define PIN_I2S_WS     10
#define PIN_I2S_DATA   11

// Panel geometry
#define PANEL_W   64
#define PANEL_H   32
#define PANEL_CHAIN 1
