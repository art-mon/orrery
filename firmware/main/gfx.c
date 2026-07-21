#include "gfx.h"
#include "panel.h"
#include "pins.h"
#include "world.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

// ── Pixel font (variable width, 5 rows tall) ─────────────────────────────
// Direct port of display/index.html's FONT. Each row is encoded as a byte
// with the pattern in the low W bits, MSB on the left. width = number of
// columns. Glyph table is indexed by (uppercase) ASCII via lookup table.

typedef struct {
    uint8_t w;          // 1..5
    uint8_t rows[5];    // bit (w-1) = leftmost column
} pf_glyph_t;

#define ROW(b) (uint8_t)(b)
#define G(w_, r0, r1, r2, r3, r4) { (uint8_t)(w_), { ROW(r0), ROW(r1), ROW(r2), ROW(r3), ROW(r4) } }

// Order: special, digits, uppercase A-Z, punctuation.
static const pf_glyph_t G_DIG[10] = {
    G(3, 0b111, 0b101, 0b101, 0b101, 0b111), // 0
    G(3, 0b010, 0b110, 0b010, 0b010, 0b111), // 1
    G(3, 0b111, 0b001, 0b111, 0b100, 0b111), // 2
    G(3, 0b111, 0b001, 0b011, 0b001, 0b111), // 3
    G(3, 0b101, 0b101, 0b111, 0b001, 0b001), // 4
    G(3, 0b111, 0b100, 0b111, 0b001, 0b111), // 5
    G(3, 0b111, 0b100, 0b111, 0b101, 0b111), // 6
    G(3, 0b111, 0b001, 0b010, 0b010, 0b010), // 7
    G(3, 0b111, 0b101, 0b111, 0b101, 0b111), // 8
    G(3, 0b111, 0b101, 0b111, 0b001, 0b111), // 9
};
static const pf_glyph_t G_AZ[26] = {
    G(3, 0b010, 0b101, 0b111, 0b101, 0b101), // A
    G(3, 0b110, 0b101, 0b110, 0b101, 0b110), // B
    G(3, 0b011, 0b100, 0b100, 0b100, 0b011), // C
    G(3, 0b110, 0b101, 0b101, 0b101, 0b110), // D
    G(3, 0b111, 0b100, 0b110, 0b100, 0b111), // E
    G(3, 0b111, 0b100, 0b110, 0b100, 0b100), // F
    G(3, 0b011, 0b100, 0b101, 0b101, 0b011), // G
    G(4, 0b1001,0b1001,0b1111,0b1001,0b1001),// H
    G(3, 0b111, 0b010, 0b010, 0b010, 0b111), // I
    G(3, 0b001, 0b001, 0b001, 0b101, 0b010), // J
    G(3, 0b101, 0b110, 0b100, 0b110, 0b101), // K
    G(3, 0b100, 0b100, 0b100, 0b100, 0b111), // L
    G(5, 0b10001,0b11011,0b10101,0b10001,0b10001),// M
    G(4, 0b1001,0b1101,0b1011,0b1001,0b1001),// N
    G(3, 0b010, 0b101, 0b101, 0b101, 0b010), // O
    G(3, 0b110, 0b101, 0b110, 0b100, 0b100), // P
    G(3, 0b010, 0b101, 0b101, 0b111, 0b011), // Q
    G(3, 0b110, 0b101, 0b110, 0b101, 0b101), // R
    G(3, 0b011, 0b100, 0b010, 0b001, 0b110), // S
    G(3, 0b111, 0b010, 0b010, 0b010, 0b010), // T
    G(3, 0b101, 0b101, 0b101, 0b101, 0b010), // U
    G(3, 0b101, 0b101, 0b101, 0b010, 0b010), // V
    G(5, 0b10001,0b10001,0b10101,0b11011,0b10001),// W
    G(3, 0b101, 0b101, 0b010, 0b101, 0b101), // X
    G(3, 0b101, 0b101, 0b010, 0b010, 0b010), // Y
    G(3, 0b111, 0b001, 0b010, 0b100, 0b111), // Z
};

static const pf_glyph_t G_SPACE  = G(3, 0,0,0,0,0);
static const pf_glyph_t G_COLON  = G(1, 0,1,0,1,0);
static const pf_glyph_t G_DOT    = G(1, 0,0,0,0,1);
static const pf_glyph_t G_DEG    = G(2, 0b11,0b11,0,0,0);
static const pf_glyph_t G_SLASH  = G(3, 0b001,0b001,0b010,0b100,0b100);
static const pf_glyph_t G_DASH   = G(3, 0,0,0b111,0,0);
static const pf_glyph_t G_LPAREN = G(2, 0b01,0b10,0b10,0b10,0b01);
static const pf_glyph_t G_RPAREN = G(2, 0b10,0b01,0b01,0b01,0b10);
static const pf_glyph_t G_BANG   = G(2, 0b01,0b01,0b01,0,0b01);
static const pf_glyph_t G_TILDE  = G(4, 0b0101,0b1010,0,0,0);
static const pf_glyph_t G_PCT    = G(3, 0b101,0b001,0b010,0b100,0b101);

static const pf_glyph_t *lookup(char c) {
    unsigned char u = (unsigned char)c;
    if (u >= 'a' && u <= 'z') u -= 32;
    if (u >= '0' && u <= '9') return &G_DIG[u - '0'];
    if (u >= 'A' && u <= 'Z') return &G_AZ[u - 'A'];
    switch (u) {
        case ' ': return &G_SPACE;
        case ':': return &G_COLON;
        case '.': return &G_DOT;
        case 0xB0: case '*': return &G_DEG;   // raw ° byte rarely arrives ASCII-stripped
        case '/': return &G_SLASH;
        case '-': return &G_DASH;
        case '(': return &G_LPAREN;
        case ')': return &G_RPAREN;
        case '!': return &G_BANG;
        case '~': return &G_TILDE;
        case '%': return &G_PCT;
        default:  return &G_SPACE;
    }
}

// ── Primitives ───────────────────────────────────────────────────────────

void gfx_hline(int y, int x0, int x1, uint8_t r, uint8_t g, uint8_t b) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; ++x) panel_pixel(x, y, r, g, b);
}

void gfx_vline(int x, int y0, int y1, uint8_t r, uint8_t g, uint8_t b) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; ++y) panel_pixel(x, y, r, g, b);
}

void gfx_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            panel_pixel(x + dx, y + dy, r, g, b);
}

void gfx_circle(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b, int fill) {
    for (int dy = -rad; dy <= rad; ++dy) {
        for (int dx = -rad; dx <= rad; ++dx) {
            float d = sqrtf((float)(dx * dx + dy * dy));
            int hit = fill ? (d <= rad) : (fabsf(d - rad) < 0.7f);
            if (hit) panel_pixel(cx + dx, cy + dy, r, g, b);
        }
    }
}

void gfx_stars(uint32_t seed, int count, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t s = seed;
    for (int i = 0; i < count; ++i) {
        s = s * 1664525u + 1013904223u;
        int x = (int)((s >> 0)  % PANEL_W);
        s = s * 1664525u + 1013904223u;
        int y = (int)((s >> 0)  % PANEL_H);
        s = s * 1664525u + 1013904223u;
        int br = (int)((s % 2) + 1);
        panel_pixel(x, y, (uint8_t)(r * br / 2),
                          (uint8_t)(g * br / 2),
                          (uint8_t)(b * br / 2));
    }
}

// ── Text rendering ───────────────────────────────────────────────────────

int gfx_text_width(const char *s) {
    int w = 0;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        w += gl->w + 1;
    }
    return w > 0 ? w - 1 : 0;
}

void gfx_text(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int cx = x;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        for (int row = 0; row < 5; ++row) {
            uint8_t bits = gl->rows[row];
            for (int col = 0; col < gl->w; ++col) {
                if (bits & (1u << (gl->w - 1 - col)))
                    panel_pixel(cx + col, y + row, r, g, b);
            }
        }
        cx += gl->w + 1;
    }
}

void gfx_text_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    static const int8_t off[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; ++i) gfx_text(x + off[i][0], y + off[i][1], s, 0, 0, 0);
    gfx_text(x, y, s, r, g, b);
}

// 2x scaled
int gfx_big_width(const char *s) {
    int w = 0;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        w += (gl->w + 1) * 2;
    }
    return w > 0 ? w - 2 : 0;
}

void gfx_big(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int cx = x;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        for (int row = 0; row < 5; ++row) {
            uint8_t bits = gl->rows[row];
            for (int col = 0; col < gl->w; ++col) {
                if (bits & (1u << (gl->w - 1 - col))) {
                    int px2 = cx + col * 2;
                    int py2 = y  + row * 2;
                    panel_pixel(px2,     py2,     r, g, b);
                    panel_pixel(px2 + 1, py2,     r, g, b);
                    panel_pixel(px2,     py2 + 1, r, g, b);
                    panel_pixel(px2 + 1, py2 + 1, r, g, b);
                }
            }
        }
        cx += (gl->w + 1) * 2;
    }
}

void gfx_big_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    static const int8_t off[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; ++i) gfx_big(x + off[i][0], y + off[i][1], s, 0, 0, 0);
    gfx_big(x, y, s, r, g, b);
}

// 8-pixel tall mixed-height variant.
static const uint8_t MED_ROW_H[5] = { 2, 1, 2, 1, 2 };

int gfx_med_width(const char *s) {
    int w = 0;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        w += gl->w + 1;
    }
    return w > 0 ? w - 1 : 0;
}

void gfx_med(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int cx = x;
    for (; *s; ++s) {
        const pf_glyph_t *gl = lookup(*s);
        int py = y;
        for (int row = 0; row < 5; ++row) {
            uint8_t bits = gl->rows[row];
            for (int rep = 0; rep < MED_ROW_H[row]; ++rep) {
                for (int col = 0; col < gl->w; ++col) {
                    if (bits & (1u << (gl->w - 1 - col)))
                        panel_pixel(cx + col, py, r, g, b);
                }
                py++;
            }
        }
        cx += gl->w + 1;
    }
}

void gfx_med_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    static const int8_t off[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; ++i) gfx_med(x + off[i][0], y + off[i][1], s, 0, 0, 0);
    gfx_med(x, y, s, r, g, b);
}

void gfx_time_weather(const char *time_str, const char *temp_str,
                      uint8_t tr, uint8_t tg, uint8_t tb,
                      uint8_t wr, uint8_t wg, uint8_t wb) {
    gfx_text_outlined(1, 1, time_str, tr, tg, tb);
    int wp = gfx_text_width(temp_str);
    gfx_text_outlined(PANEL_W - wp - 1, 1, temp_str, wr, wg, wb);
}

void gfx_ticker(int y, const char *s, uint32_t scroll_px,
                uint8_t r, uint8_t g, uint8_t b) {
    int w = gfx_text_width(s);
    if (w <= 0) return;
    int gap    = 16;
    int period = w + gap;
    int offset = -(int)(scroll_px % (uint32_t)period);
    for (int x = offset; x < PANEL_W; x += period) {
        gfx_text_outlined(x, y, s, r, g, b);
    }
}

// Returns a smooth, uniformly-spaced ticker scroll offset in pixels for a
// given frame `tick`. Stepping 1 pixel every `frames_per_pixel` frames
// gives an exact cadence — no irregular 2/3 frame holds from integer
// division of (tick * 33 ms * speed / 1000).
uint32_t gfx_ticker_scroll(uint32_t tick) {
    // 1 pixel every 2 frames at 30 FPS = 15 px/s — uniform cadence.
    return tick / 2;
}

// ── Synthetic moon ───────────────────────────────────────────────────────
// Simpler than the simulator (no real-photo texture). Renders a shaded
// disc with a soft terminator. Good enough at radius 4–8 px.

// 15×15 hand-crafted lunar near-side intensity texture (0 = outside disc).
// Ported verbatim from display/index.html's MOON_TEX.
static const uint8_t MOON_TEX[15 * 15] = {
      0,  0,  0,  0,  0,  0,  0,210,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,210,215,215,215,215,215,210,  0,  0,  0,  0,
      0,  0,  0,205,200,170,175,180,200,210,205,200,  0,  0,  0,
      0,  0,200,205,180,140,130,150,180,170,165,180,200,  0,  0,
      0,195,200,190,130,115,120,115,150,130,140,175,190,195,  0,
      0,195,185,165,120,125,130,155,140,125,135,170,180,190,  0,
      0,190,175,145,125,135,140,185,160,140,155,175,190,195,  0,
    200,180,155,140,130,140,175,200,175,150,140,155,185,200,215,
      0,190,170,155,140,150,165,175,160,140,135,155,180,200,  0,
      0,200,180,165,155,160,170,175,155,140,150,175,185,200,  0,
      0,205,195,185,175,170,175,180,240,230,175,185,200,210,  0,
      0,  0,205,195,185,180,185,245,250,245,180,190,210,  0,  0,
      0,  0,  0,210,205,200,195,215,230,215,195,205,  0,  0,  0,
      0,  0,  0,  0,215,210,210,210,210,210,215,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,210,  0,  0,  0,  0,  0,  0,  0,
};

static uint8_t moon_tex_sample(int dx, int dy, int rad) {
    // Map disc coord into 15x15 texture
    int tx = (int)(((float)(dx + rad) / (2.0f * rad + 1.0f)) * 15.0f);
    int ty = (int)(((float)(dy + rad) / (2.0f * rad + 1.0f)) * 15.0f);
    if (tx < 0)  tx = 0;
    if (tx > 14) tx = 14;
    if (ty < 0)  ty = 0;
    if (ty > 14) ty = 14;
    return MOON_TEX[ty * 15 + tx];
}

void gfx_moon(int cx, int cy, int rad, float phase01) {
    float halfPhase = (phase01 <= 0.5f) ? (phase01 * 2.0f) : ((phase01 - 0.5f) * 2.0f);
    int   waxing    = (phase01 <= 0.5f);
    float f          = (1.0f - cosf((float)M_PI * halfPhase)) * 0.5f;
    float shadowEdge = 1.0f - 2.0f * f;  // -1..+1 terminator x
    float lumScale   = 0.90f + 0.10f * f;

    for (int dy = -rad; dy <= rad; ++dy) {
        float hw = sqrtf((float)(rad * rad - dy * dy));
        for (int dx = -rad; dx <= rad; ++dx) {
            if (dx * dx + dy * dy > rad * rad) continue;
            float nx = hw > 0 ? (float)dx / hw : 0.0f;
            int   lit = waxing ? (nx > shadowEdge) : (nx < shadowEdge);

            // Base "moon" colour — warm cream, modulated by the photo tex
            // for radius ≥ 10; for tiny moons fall back to crater noise.
            float baseR, baseG, baseB;
            if (rad >= 10) {
                uint8_t t = moon_tex_sample(dx, dy, rad);
                if (t == 0) t = 150;   // fill any internal holes
                float n = (float)t / 215.0f;       // ~peak in the texture
                if (n > 1.0f) n = 1.0f;
                baseR = 230.0f * n;
                baseG = 220.0f * n;
                baseB = 190.0f * n;
            } else {
                baseR = 230.0f; baseG = 220.0f; baseB = 190.0f;
                float crater = 0.85f + 0.15f * sinf((dx * 1.7f) + (dy * 2.3f));
                baseR *= crater; baseG *= crater; baseB *= crater;
            }

            float rR, gG, bB;
            if (lit) {
                float td   = fabsf(nx - shadowEdge);
                float term = (td < 0.15f) ? 0.55f + (td / 0.15f) * 0.45f : 1.0f;
                float s    = term * lumScale;
                rR = baseR * s; gG = baseG * s; bB = baseB * s;
            } else {
                // Earthshine — cool dim glow on the dark side
                float s = 0.45f;
                rR = baseR * 0.75f * s;
                gG = baseG * 0.85f * s;
                bB = baseB * 1.05f * s;
            }
            uint8_t r8 = rR < 0 ? 0 : (rR > 255 ? 255 : (uint8_t)rR);
            uint8_t g8 = gG < 0 ? 0 : (gG > 255 ? 255 : (uint8_t)gG);
            uint8_t b8 = bB < 0 ? 0 : (bB > 255 ? 255 : (uint8_t)bB);
            panel_pixel(cx + dx, cy + dy, r8, g8, b8);
        }
    }
}

// ── Weather backgrounds ──────────────────────────────────────────────────

static int s_night = 0;
void gfx_set_night(int night) { s_night = night ? 1 : 0; }
int  gfx_is_night(void)       { return s_night; }

// Pseudo-random hash, matches the simulator's nhash(a, b)
static float nhash(int a, int b) {
    float v = sinf(a * 127.1f + b * 311.7f) * 43758.5f;
    return v - floorf(v);
}

static int cond_contains(const char *cond, const char *needle) {
    if (!cond) return 0;
    for (const char *p = cond; *p; ++p) {
        const char *n = needle;
        const char *s = p;
        while (*n && *s && tolower((unsigned char)*s) == tolower((unsigned char)*n)) { s++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

void gfx_weather_sky_col(int x0, int col_w, const char *cond) {
    int isClear = cond_contains(cond, "clear");
    int isRain  = cond_contains(cond, "rain") || cond_contains(cond, "drizzle");
    int isSnow  = cond_contains(cond, "snow");
    int isStorm = cond_contains(cond, "thunder");
    int x1 = x0 + col_w - 1;

    for (int y = 0; y < PANEL_H; ++y) {
        float t = (float)y / (PANEL_H - 1);
        int r, g, b;
        if (s_night) {
            if      (isClear) { r =  2 + (int)(t * 10); g =  4 + (int)(t * 12); b = 22 + (int)(t * 28); }
            else if (isRain)  { r =  5 + (int)(t *  8); g =  8 + (int)(t * 10); b = 20 + (int)(t * 18); }
            else if (isSnow)  { r = 20 + (int)(t * 30); g = 28 + (int)(t * 30); b = 55 + (int)(t * 30); }
            else if (isStorm) { r =  2 + (int)(t *  6); g =  2 + (int)(t *  6); b = 10 + (int)(t * 14); }
            else              { r = 10 + (int)(t * 12); g = 14 + (int)(t * 14); b = 28 + (int)(t * 20); }
        }
        else if (isClear) { r = 20 + (int)(t * 60);  g = 60 + (int)(t * 80);  b = 160 + (int)(t * 60); }
        else if (isRain)  { r = 30 + (int)(t * 20);  g = 40 + (int)(t * 20);  b = 70  + (int)(t * 30); }
        else if (isSnow)  { r = 80 + (int)(t * 80);  g = 90 + (int)(t * 80);  b = 120 + (int)(t * 80); }
        else if (isStorm) { r = 10 + (int)(t * 20);  g = 10 + (int)(t * 15);  b = 30  + (int)(t * 20); }
        else              { r = 40 + (int)(t * 30);  g = 50 + (int)(t * 30);  b = 80  + (int)(t * 40); }
        gfx_hline(y, x0, x1, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
}

void gfx_weather_sky(const char *cond) {
    gfx_weather_sky_col(0, PANEL_W, cond);
}

void gfx_weather_particles_col(int x0, int col_w, int col_idx,
                               const char *cond, uint32_t tick) {
    int isClear   = cond_contains(cond, "clear");
    int isDrizzle = cond_contains(cond, "drizzle");
    int isRain    = cond_contains(cond, "rain") && !isDrizzle;
    int isSnow    = cond_contains(cond, "snow");
    int isStorm   = cond_contains(cond, "thunder");

    const int H = PANEL_H;
    int x_end = x0 + col_w;

    if (isClear) {
        if (!s_night) return;
        // Sparse starfield — deterministic positions from column seed, count
        // scales with column width so the full-panel case gets ~48 stars.
        int stars = 8 + (col_w * 5) / 8;
        uint32_t s = 0xC0FFEEu ^ ((uint32_t)col_idx * 0x9E3779B1u);
        for (int i = 0; i < stars; ++i) {
            s = s * 1664525u + 1013904223u;
            int rx = x0 + (int)(s % (uint32_t)col_w);
            s = s * 1664525u + 1013904223u;
            int ry = (int)(s % (uint32_t)H);
            s = s * 1664525u + 1013904223u;
            int base = 90 + (int)(s % 90u);            // 90..179 base
            float pulse = 0.75f + 0.25f * sinf(tick * 0.08f + i * 1.9f);
            int br = (int)(base * pulse);
            if (br > 255) br = 255;
            uint8_t bb = (uint8_t)(br < 240 ? br + 15 : 255);
            panel_pixel(rx, ry, (uint8_t)br, (uint8_t)br, bb);
        }
        return;
    }

    if (isRain || isStorm) {
        for (int x = x0; x < x_end; ++x) {
            int lx = x - x0;
            if (isRain && nhash(lx + 400, col_idx) < 0.33f) continue;
            float speed = 2.0f + nhash(lx,       col_idx) * 3.0f;
            float phase = nhash(lx + 50,  col_idx) * H;
            int   len   = 2 + (int)(nhash(lx + 100, col_idx) * 3.0f);
            int   br0   = isRain
                ? 70  + (int)(nhash(lx, col_idx + 7) * 50.0f)
                : 120 + (int)(nhash(lx, col_idx + 7) * 80.0f);
            for (int s = 0; s < 2; ++s) {
                int top = (int)(phase + tick * speed + s * (H / 2)) % H;
                for (int l = 0; l < len; ++l) {
                    int ry = top + l;
                    if (ry >= H) break;
                    float fade = 1.0f - (float)l / len;
                    int   br   = (int)(br0 * fade);
                    panel_pixel(x, ry,
                                (uint8_t)((br * 35) / 100),
                                (uint8_t)((br * 55) / 100),
                                (uint8_t)br);
                }
            }
        }
        if (isStorm && (tick % 22) < 2) {
            int fx = x0 + 1 + (int)(nhash((int)tick, col_idx) * (col_w - 2));
            gfx_vline(fx, 0, H - 1, 230, 240, 255);
        }
    } else if (isDrizzle) {
        for (int x = x0; x < x_end; ++x) {
            int lx = x - x0;
            if (nhash(lx + 200, col_idx) < 0.5f) continue;
            float speed = 1.2f + nhash(lx, col_idx) * 1.5f;
            float phase = nhash(lx + 50, col_idx) * H;
            int   br    = 70 + (int)(nhash(lx, col_idx + 7) * 50.0f);
            int   top   = (int)(phase + tick * speed) % H;
            panel_pixel(x, top, (uint8_t)((br*40)/100), (uint8_t)((br*55)/100), (uint8_t)br);
            if (nhash(lx + 300, col_idx) > 0.35f) {
                int t2 = (int)(phase + tick * speed + H * 0.6f) % H;
                panel_pixel(x, t2, (uint8_t)((br*40)/100), (uint8_t)((br*55)/100), (uint8_t)br);
            }
        }
    } else if (isSnow) {
        for (int x = x0; x < x_end; ++x) {
            int lx = x - x0;
            for (int s = 0; s < 2; ++s) {
                float speed = 0.3f + nhash(lx + s * 77, col_idx) * 0.5f;
                float phase = nhash(lx + 50 + s * 33, col_idx) * H;
                float drift = sinf(tick * 0.04f + lx * 0.7f + s * 1.3f) * 1.5f;
                int   rx    = (int)(x + drift + 0.5f);
                int   ry    = (int)(phase + tick * speed) % H;
                if (rx < x0 || rx >= x_end) continue;
                int br = 180 + (int)(nhash(lx + s * 11, (int)(tick * 0.05f)) * 60.0f);
                panel_pixel(rx, ry, (uint8_t)br, (uint8_t)br, 255);
            }
        }
    } else {
        // Cloudy
        for (int y = 0; y < H; ++y) {
            for (int x = x0; x < x_end; ++x) {
                int lx = x - x0;
                float t2 = tick * 0.018f;
                float n  = sinf(lx * 0.28f + t2)              * sinf(y * 0.45f + t2 * 0.7f)
                         + sinf(lx * 0.16f - t2 * 0.8f + 1.9f) * sinf(y * 0.28f + 0.9f) * 0.7f;
                if (n > 0.2f) {
                    int br = 80 + (int)((n - 0.2f) * 160.0f);
                    int rc = br;
                    int gc = br + 8;
                    int bc = br + 25;
                    if (s_night) {
                        // Moonlit undersides — dim and cool
                        rc = (rc * 22) / 100;
                        gc = (gc * 26) / 100;
                        bc = (bc * 40) / 100;
                    }
                    if (rc > 255) rc = 255;
                    if (gc > 255) gc = 255;
                    if (bc > 255) bc = 255;
                    panel_pixel(x, y, (uint8_t)rc, (uint8_t)gc, (uint8_t)bc);
                }
            }
        }
    }
}

void gfx_weather_particles(const char *cond, uint32_t tick) {
    gfx_weather_particles_col(0, PANEL_W, 0, cond, tick);
}

void gfx_earth_sprite(int cx, int cy, float r) {
    if (r < 0.7f) { panel_pixel(cx, cy, 100, 140, 220); return; }
    int R = (int)ceilf(r);
    int tex_size = 0;
    const uint8_t *tex = world_earth_tex(&tex_size);
    if (!tex) {
        // Flat blue fallback
        for (int dy = -R; dy <= R; ++dy) {
            for (int dx = -R; dx <= R; ++dx) {
                if (dx*dx + dy*dy > r*r) continue;
                panel_pixel(cx + dx, cy + dy, 60, 110, 180);
            }
        }
        return;
    }
    float half  = (tex_size - 1) * 0.5f;
    float scale = half / r;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            if (dx*dx + dy*dy > r*r) continue;
            int sx = (int)(half + dx * scale + 0.5f);
            int sy = (int)(half + dy * scale + 0.5f);
            if (sx < 0 || sx >= tex_size || sy < 0 || sy >= tex_size) continue;
            const uint8_t *p = &tex[(sy * tex_size + sx) * 3];
            panel_pixel(cx + dx, cy + dy, p[0], p[1], p[2]);
        }
    }
}

gfx_subsolar_t gfx_subsolar_point(time_t now) {
    struct tm tm;
    gmtime_r(&now, &tm);
    int doy = tm.tm_yday + 1;   // 1..366
    float decl = 23.44f * sinf(2.0f * (float)M_PI * (doy - 80) / 365.25f);
    float ut   = tm.tm_hour + tm.tm_min / 60.0f + tm.tm_sec / 3600.0f;
    float sLon = fmodf((180.0f - ut * 15.0f) + 540.0f, 360.0f) - 180.0f;
    return (gfx_subsolar_t){ .lat = decl, .lon = sLon };
}

float gfx_solar_factor(float lon, float lat, gfx_subsolar_t sub) {
    float deg = (float)M_PI / 180.0f;
    float f1 = lat * deg, f2 = sub.lat * deg, dl = (lon - sub.lon) * deg;
    return sinf(f1) * sinf(f2) + cosf(f1) * cosf(f2) * cosf(dl);
}

float gfx_moon_phase_now(time_t now) {
    // Reference new moon: 2000-01-06 18:14 UTC (Unix 947182440)
    // Synodic month: 29.53059 days
    const double REF_NEW = 947182440.0;
    const double SYN     = 29.530588 * 86400.0;
    double phase = ((double)now - REF_NEW) / SYN;
    phase = phase - floor(phase);   // wrap to [0,1)
    return (float)phase;
}
