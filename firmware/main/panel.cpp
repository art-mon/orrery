#include "panel.h"
#include "pins.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

static MatrixPanel_I2S_DMA *s_panel = nullptr;

void panel_init(void) {
    HUB75_I2S_CFG::i2s_pins pins = {
        PIN_R1, PIN_G1, PIN_B1,
        PIN_R2, PIN_G2, PIN_B2,
        PIN_A,  PIN_B,  PIN_C, PIN_D, PIN_E,
        PIN_LAT, PIN_OE, PIN_CLK,
    };
    HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, PANEL_CHAIN, pins);
    cfg.clkphase = false;

    s_panel = new MatrixPanel_I2S_DMA(cfg);
    s_panel->begin();
    s_panel->setBrightness8(60);
    s_panel->clearScreen();
}

void panel_clear(void) {
    if (s_panel) s_panel->clearScreen();
}

void panel_set_brightness(uint8_t b) {
    if (s_panel) s_panel->setBrightness8(b);
}

void panel_draw_test_pattern(void) {
    if (!s_panel) return;
    s_panel->clearScreen();

    // Border in white
    for (int x = 0; x < PANEL_W; ++x) {
        s_panel->drawPixel(x, 0, s_panel->color565(255, 255, 255));
        s_panel->drawPixel(x, PANEL_H - 1, s_panel->color565(255, 255, 255));
    }
    for (int y = 0; y < PANEL_H; ++y) {
        s_panel->drawPixel(0, y, s_panel->color565(255, 255, 255));
        s_panel->drawPixel(PANEL_W - 1, y, s_panel->color565(255, 255, 255));
    }

    // Three colour bars to check RGB ordering
    s_panel->fillRect(2,  2,  20, 8, s_panel->color565(255, 0,   0));
    s_panel->fillRect(22, 2,  20, 8, s_panel->color565(0,   255, 0));
    s_panel->fillRect(42, 2,  20, 8, s_panel->color565(0,   0,   255));

    // Diagonal — verifies addressing on both halves
    for (int i = 0; i < PANEL_H; ++i) {
        s_panel->drawPixel(i * 2, i, s_panel->color565(255, 200, 0));
    }
}
