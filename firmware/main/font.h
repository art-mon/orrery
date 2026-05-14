#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal 5x7 ASCII bitmap font, column-major. Each glyph is 5 bytes,
// one byte per column, LSB = top row, bit 6 = bottom row. Bit 7 unused.
// Supports printable ASCII 0x20 (' ') through 0x7E ('~'); anything else
// renders as a blank glyph.
//
// Glyph cell is 5x7. The rendering helpers add a 1-column gap between
// characters, so each character advances the cursor by 6 px.

#define FONT_W 5
#define FONT_H 7
#define FONT_ADVANCE 6

// Look up the 5-byte column data for `c`. Always returns a valid pointer
// (blank glyph for unsupported chars).
const uint8_t *font_glyph(char c);

// Returns the pixel width of `s` as it would be rendered (including
// per-character gaps but no trailing gap).
int font_text_width(const char *s);

#ifdef __cplusplus
}
#endif
