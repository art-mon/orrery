#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void panel_init(void);
void panel_clear(void);
void panel_set_brightness(uint8_t b);
void panel_draw_test_pattern(void);

// Draw a single pixel (RGB888). Coordinates outside the panel are clipped.
void panel_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

// Draw an ASCII string in 5x7 font at (x, y) (top-left of the glyph cell).
// Returns the advance (final cursor x).
int  panel_draw_text(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif
