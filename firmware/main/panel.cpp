#include "panel.h"
#include "pins.h"

#include "hub75.h"

static Hub75Driver *s_panel = nullptr;

void panel_init(void) {
    Hub75Config cfg{};
    cfg.panel_width  = PANEL_W;
    cfg.panel_height = PANEL_H;
    cfg.scan_wiring  = Hub75ScanWiring::STANDARD_TWO_SCAN;  // panel marked "16s" = 1:16 scan
    cfg.shift_driver = Hub75ShiftDriver::FM6126A;           // try FM6126A first; fall back to GENERIC

    cfg.pins.r1  = PIN_R1;
    cfg.pins.g1  = PIN_G1;
    cfg.pins.b1  = PIN_B1;
    cfg.pins.r2  = PIN_R2;
    cfg.pins.g2  = PIN_G2;
    cfg.pins.b2  = PIN_B2;
    cfg.pins.a   = PIN_A;
    cfg.pins.b   = PIN_B;
    cfg.pins.c   = PIN_C;
    cfg.pins.d   = PIN_D;
    cfg.pins.e   = PIN_E;
    cfg.pins.lat = PIN_LAT;
    cfg.pins.oe  = PIN_OE;
    cfg.pins.clk = PIN_CLK;

    s_panel = new Hub75Driver(cfg);
    s_panel->begin();
    s_panel->clear();
}

void panel_clear(void) {
    if (s_panel) s_panel->clear();
}

void panel_set_brightness(uint8_t b) {
    if (s_panel) s_panel->set_brightness(b);
}

void panel_draw_test_pattern(void) {
    if (!s_panel) return;
    s_panel->clear();

    // White border
    s_panel->fill(0, 0, PANEL_W, 1, 255, 255, 255);
    s_panel->fill(0, PANEL_H - 1, PANEL_W, 1, 255, 255, 255);
    s_panel->fill(0, 0, 1, PANEL_H, 255, 255, 255);
    s_panel->fill(PANEL_W - 1, 0, 1, PANEL_H, 255, 255, 255);

    // RGB bars — verify channel ordering
    s_panel->fill(2,  2, 20, 8, 255, 0,   0);
    s_panel->fill(22, 2, 20, 8, 0,   255, 0);
    s_panel->fill(42, 2, 20, 8, 0,   0,   255);

    // Diagonal across both halves — verifies row addressing
    for (int i = 0; i < PANEL_H; ++i) {
        s_panel->set_pixel(i * 2, i, 255, 200, 0);
    }
}
