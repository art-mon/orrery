#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Primitives ───────────────────────────────────────────────────────────
void gfx_hline(int y, int x0, int x1, uint8_t r, uint8_t g, uint8_t b);
void gfx_vline(int x, int y0, int y1, uint8_t r, uint8_t g, uint8_t b);
void gfx_rect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void gfx_circle(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b, int fill);

// Deterministic starfield. `seed` selects the pattern; same seed = same stars.
void gfx_stars(uint32_t seed, int count, uint8_t r, uint8_t g, uint8_t b);

// ── Variable-width pixel font (matches the simulator's FONT) ─────────────
// 5 rows tall, 1..5 cols wide depending on glyph. Used by gfx_text* and
// drawBig/drawMed.
int  gfx_text_width(const char *s);  // sum of glyph widths + 1px gaps
void gfx_text(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);
void gfx_text_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);

// 2× scale — used for clocks and big temperatures.
int  gfx_big_width(const char *s);
void gfx_big(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);
void gfx_big_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);

// Medium: 8px tall (mixed row heights 2/1/2/1/2), 1× character width.
int  gfx_med_width(const char *s);
void gfx_med(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);
void gfx_med_outlined(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b);

// Combo helper: time top-left, temp top-right, both outlined.
void gfx_time_weather(const char *time_str, const char *temp_str,
                      uint8_t tr, uint8_t tg, uint8_t tb,
                      uint8_t wr, uint8_t wg, uint8_t wb);

// Scrolling ticker (single-line). `scroll_px` is the cumulative offset,
// usually `(tick * frame_ms * speed_pps) / 1000`.
void gfx_ticker(int y, const char *s, uint32_t scroll_px,
                uint8_t r, uint8_t g, uint8_t b);

// Smooth scroll offset for `tick` (no half-step cadence). Use as the
// `scroll_px` argument to gfx_ticker for clean visual motion.
uint32_t gfx_ticker_scroll(uint32_t tick);

// ── Synthetic moon ────────────────────────────────────────────────────────
// `phase01` is 0..1 across the lunar cycle (0=new, 0.5=full, 1=new again).
// Draws a phase-accurate moon disc at (cx, cy) with radius `rad`. No real
// photo texture — uses a simple noise-free shaded disc. Good for tiny moons.
void gfx_moon(int cx, int cy, int rad, float phase01);

// Current moon phase in [0,1) at unix time `now`. Reference: 2000-01-06 18:14 UTC = new moon.
float gfx_moon_phase_now(time_t now);

// Earth sprite — uses the baked 64×64 RGB texture if loaded (world.h),
// otherwise paints a flat blue disc. `r` is radius in panel pixels;
// the texture is sampled with nearest-neighbour at scale half/r.
void gfx_earth_sprite(int cx, int cy, float r);

// Solar position helpers — return the subsolar lat/lon (degrees) at `now`,
// and the cosine-of-zenith ("solar factor") at a given lat/lon. SF > 0 means
// sun above the horizon at that point; 1 = directly overhead.
typedef struct { float lat; float lon; } gfx_subsolar_t;
gfx_subsolar_t gfx_subsolar_point(time_t now);
float gfx_solar_factor(float lon, float lat, gfx_subsolar_t sub);

// Weather-condition aware backgrounds, mirroring the simulator's
// drawWeatherSky / drawWeatherParticles. `cond` is the condition string
// from daily.json ("Clear", "Rain", "Snow", etc.); case-insensitive.
// The full-panel helpers cover x=[0,PANEL_W). The column helpers paint a
// vertical strip x=[x0, x0+col_w). `col_idx` seeds the noise so adjacent
// columns get different particle patterns.
void gfx_weather_sky(const char *cond);
void gfx_weather_particles(const char *cond, uint32_t tick);
void gfx_weather_sky_col(int x0, int col_w, const char *cond);
void gfx_weather_particles_col(int x0, int col_w, int col_idx,
                               const char *cond, uint32_t tick);

// Night mode toggle for the weather helpers above. When enabled, sky
// gradients switch to deep-navy palettes and the "clear" particle branch
// paints a twinkling starfield instead of returning early. Off by default;
// callers are expected to save/restore around their draws so unrelated
// scenes (e.g. the 3-day forecast strip) aren't affected.
void gfx_set_night(int night);
int  gfx_is_night(void);

#ifdef __cplusplus
}
#endif
