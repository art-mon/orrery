#include "scenes.h"
#include "panel.h"
#include "pins.h"
#include "gfx.h"
#include "clock.h"
#include "world.h"
#include "apod.h"
#include "encoder.h"
#include "audio.h"
#include "wifi_creds.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define FRAME_MS  33     // ~30 FPS

// ─── helpers ─────────────────────────────────────────────────────────────

static void format_clock(struct tm *tm, char *out, size_t n) {
    snprintf(out, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

static int contains_ci(const char *s, const char *needle) {
    if (!s) return 0;
    for (const char *p = s; *p; ++p) {
        const char *n = needle;
        const char *t = p;
        while (*n && *t && tolower((unsigned char)*t) == tolower((unsigned char)*n)) { t++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

static void cond_palette(const char *cond,
                         uint8_t *trgb, uint8_t *flrgb,
                         uint8_t *hurgb, uint8_t *wdrgb,
                         uint8_t *nowrgb) {
    if      (contains_ci(cond, "clear")) {
        uint8_t a[5][3] = { {255,220,80},{230,180,60},{80,170,255},{100,220,200},{255,210,100} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    } else if (contains_ci(cond, "thunder")) {
        uint8_t a[5][3] = { {200,200,255},{160,160,230},{80,130,255},{150,140,255},{180,180,255} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    } else if (contains_ci(cond, "rain") || contains_ci(cond, "drizzle")) {
        uint8_t a[5][3] = { {120,190,255},{100,160,230},{80,170,255},{80,210,220},{130,190,255} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    } else if (contains_ci(cond, "snow")) {
        uint8_t a[5][3] = { {220,235,255},{180,210,255},{140,190,255},{200,225,255},{210,230,255} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    } else if (contains_ci(cond, "mist") || contains_ci(cond, "fog") || contains_ci(cond, "haze")) {
        uint8_t a[5][3] = { {200,210,215},{170,180,185},{140,175,200},{160,195,195},{190,200,210} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    } else {
        // Cloudy / default
        uint8_t a[5][3] = { {190,205,230},{155,170,200},{100,155,215},{110,190,185},{180,195,220} };
        memcpy(trgb, a[0], 3); memcpy(flrgb, a[1], 3); memcpy(hurgb, a[2], 3);
        memcpy(wdrgb, a[3], 3); memcpy(nowrgb, a[4], 3);
    }
}

// ─── scene_now (Weather: NOW) ────────────────────────────────────────────

static void scene_now(const daily_data_t *d, uint32_t tick) {
    const char *cond = d->has_weather ? d->condition : "";

    gfx_weather_sky(cond);
    gfx_weather_particles(cond, tick);

    uint8_t tcol[3], flcol[3], hucol[3], wdcol[3], nowcol[3];
    cond_palette(cond, tcol, flcol, hucol, wdcol, nowcol);

    // Top bar: "NOW" left, clock right
    gfx_text_outlined(1, 1, "NOW", nowcol[0], nowcol[1], nowcol[2]);

    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8]; format_clock(&tm, clk, sizeof(clk));
        int  cw = gfx_text_width(clk);
        gfx_text_outlined(PANEL_W - cw - 1, 1, clk, 170, 170, 170);
    }

    if (!d->has_weather) {
        const char *msg = "NO DATA";
        int w = gfx_text_width(msg);
        gfx_text_outlined((PANEL_W - w) / 2, 14, msg, 120, 120, 120);
        return;
    }

    // Medium-size temperature, e.g. "15*C"
    char temps[12];
    snprintf(temps, sizeof(temps), "%d*C", (int)(d->temp_c + (d->temp_c >= 0 ? 0.5f : -0.5f)));
    const int tx = 10, ty = 9;
    int tw = gfx_med_width(temps);
    gfx_med_outlined(tx, ty, temps, tcol[0], tcol[1], tcol[2]);

    // Feels-like to the right, vertically aligned within the med text
    if (d->feels_c != 0.0f || d->temp_c != 0.0f) {  // best-effort presence check
        char fl[16];
        snprintf(fl, sizeof(fl), "(FL %d*)", (int)(d->feels_c + (d->feels_c >= 0 ? 0.5f : -0.5f)));
        gfx_text_outlined(tx + tw + 6, ty + 2, fl, flcol[0], flcol[1], flcol[2]);
    }

    // Humidity + wind, two columns at y=20 (labels) and y=26 (values)
    int col2 = PANEL_W / 2 + 1;
    gfx_text_outlined(9,    20, "HUM",  hucol[0], hucol[1], hucol[2]);
    gfx_text_outlined(col2, 20, "WIND", wdcol[0], wdcol[1], wdcol[2]);

    char hbuf[8], wbuf[10];
    snprintf(hbuf, sizeof(hbuf), "%d%%", d->humidity);
    snprintf(wbuf, sizeof(wbuf), "%dKMH", (int)(d->wind_kmh + 0.5f));
    gfx_text_outlined(9,    26, hbuf, hucol[0], hucol[1], hucol[2]);
    gfx_text_outlined(col2, 26, wbuf, wdcol[0], wdcol[1], wdcol[2]);
}

// ─── scene_forecast (3-day mini columns) ────────────────────────────────

static const char *DAYS_OF_WEEK[7] = { "SUN","MON","TUE","WED","THU","FRI","SAT" };

static void scene_forecast(const daily_data_t *d, uint32_t tick) {
    struct tm tm;
    int dow = 0;
    if (clock_now(&tm)) dow = tm.tm_wday;

    const int COL_W = 20;
    const int X0[3] = { 1, 22, 43 };

    for (int i = 0; i < 3; ++i) {
        const daily_forecast_t *f = &d->fc[i];
        const char *cond = f->valid ? f->condition : "";
        int x0 = X0[i];

        gfx_weather_sky_col(x0, COL_W, cond);

        const char *day = DAYS_OF_WEEK[(dow + i) % 7];

        if (!f->valid) {
            int dw = gfx_text_width(day);
            gfx_text_outlined(x0 + (COL_W - dw) / 2, 2, day, 210, 220, 240);
            int xw = gfx_text_width("--");
            gfx_text_outlined(x0 + (COL_W - xw) / 2, 13, "--", 80, 80, 80);
            continue;
        }

        // Particles first, then text on top so labels stay readable
        gfx_weather_particles_col(x0, COL_W, i, cond, tick);

        int dw = gfx_text_width(day);
        gfx_text_outlined(x0 + (COL_W - dw) / 2, 2, day, 210, 220, 240);

        char tmax[8], tmin[8];
        snprintf(tmax, sizeof(tmax), "%d*",
                 (int)(f->temp_max_c + (f->temp_max_c >= 0 ? 0.5f : -0.5f)));
        snprintf(tmin, sizeof(tmin), "%d*",
                 (int)(f->temp_min_c + (f->temp_min_c >= 0 ? 0.5f : -0.5f)));
        int wmax = gfx_text_width(tmax);
        int wmin = gfx_text_width(tmin);
        gfx_text_outlined(x0 + (COL_W - wmax) / 2, 17, tmax, 255, 160, 60);
        gfx_text_outlined(x0 + (COL_W - wmin) / 2, 25, tmin, 100, 180, 255);
    }

    // Dividers between columns
    gfx_vline(21, 0, PANEL_H - 1, 50, 60, 90);
    gfx_vline(42, 0, PANEL_H - 1, 50, 60, 90);
}

// ─── scene_moon (full-screen moon at night) ──────────────────────────────

#define MOON_CX 38
#define MOON_CY 16
#define MOON_R  15

// Hand-placed twinkling stars (left side mostly; right slim strip).
static const uint8_t NIGHT_STARS[][2] = {
    {2,2},{8,4},{14,1},{20,6},{5,9},{11,11},{17,13},{22,10},
    {3,16},{9,18},{15,20},{20,17},{6,23},{12,27},{19,25},{2,29},
    {23,22},{17,29},{21,2},{7,14},
    {58,3},{62,9},{60,18},{57,26},{61,29},
};
#define NUM_NIGHT_STARS (sizeof(NIGHT_STARS) / sizeof(NIGHT_STARS[0]))

static const char *moon_phase_name(float days) {
    if (days <  1.85f) return "NEW MOON";
    if (days <  7.38f) return "WAXING CRESCENT";
    if (days <  9.22f) return "FIRST QUARTER";
    if (days < 14.77f) return "WAXING GIBBOUS";
    if (days < 16.61f) return "FULL MOON";
    if (days < 22.15f) return "WANING GIBBOUS";
    if (days < 24.00f) return "LAST QUARTER";
    return "WANING CRESCENT";
}

// Cloud field — soft horizontal wisps drifting sideways.
static float cloud_alpha(int x, int y, uint32_t tick) {
    float off1 = tick * 0.12f;
    float off2 = tick * 0.28f;
    float n1 = 0.5f
        + 0.35f * sinf(y * 0.28f + 0.3f) * cosf((x + off1) * 0.09f)
        + 0.25f * sinf(y * 0.22f - 0.7f) * cosf((x + off1) * 0.15f + 1.7f);
    float n2 = 0.5f
        + 0.30f * sinf(y * 0.32f + 2.1f) * cosf((x + off2) * 0.13f + 3.1f)
        + 0.20f * sinf(y * 0.38f + 0.4f) * cosf((x + off2) * 0.21f + 0.4f);
    float n = n1 * 0.6f + n2 * 0.4f;
    float a = (n - 0.55f) / 0.20f;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    return a;
}

// ── Shooting star state ──
typedef struct {
    bool   active;
    float  x, y, vx, vy;
    int    life;
} shooting_star_t;
static shooting_star_t s_shoot = { 0 };
static uint32_t        s_next_shoot = 450;

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static void update_shooting(uint32_t tick) {
    if (s_shoot.active) {
        s_shoot.x += s_shoot.vx;
        s_shoot.y += s_shoot.vy;
        s_shoot.life--;
        if (s_shoot.life <= 0 || s_shoot.x < -4 || s_shoot.x > PANEL_W + 4
                              || s_shoot.y < -4 || s_shoot.y > PANEL_H + 4) {
            s_shoot.active = false;
            s_next_shoot = tick + 560 + (uint32_t)(frand() * 565);
        }
    } else if (tick >= s_next_shoot) {
        int from_left = frand() < 0.5f;
        if (from_left) {
            s_shoot = (shooting_star_t){ .active=true, .x=-2,
                                         .y=2 + frand()*8, .vx=2.0f, .vy=0.6f, .life=14 };
        } else {
            s_shoot = (shooting_star_t){ .active=true, .x=4 + frand()*20,
                                         .y=-2, .vx=1.7f, .vy=0.9f, .life=14 };
        }
    }
}

static void draw_shooting(void) {
    if (!s_shoot.active) return;
    static const uint8_t TRAIL[5][3] = {
        {255,255,255}, {200,215,245}, {130,160,210}, {70,95,150}, {35,50,90}
    };
    for (int i = 0; i < 5; ++i) {
        int tx = (int)(s_shoot.x - s_shoot.vx * i + 0.5f);
        int ty = (int)(s_shoot.y - s_shoot.vy * i + 0.5f);
        panel_pixel(tx, ty, TRAIL[i][0], TRAIL[i][1], TRAIL[i][2]);
    }
}

// ── Spacecraft state ──
typedef struct {
    bool   active;
    float  x, vx;
    int    y;
} spacecraft_t;
static spacecraft_t s_ship = { 0 };
static uint32_t     s_next_ship = 300;

// 0=transparent, 1=body (white), 2=panel (grey), 3=nav (red blink)
static const uint8_t SHIP_SPRITE[3][7] = {
    {0,0,2,1,2,0,0},
    {2,2,1,1,1,2,2},
    {0,0,2,3,2,0,0},
};

static void update_ship(uint32_t tick) {
    if (s_ship.active) {
        s_ship.x += s_ship.vx;
        if (s_ship.x < -10 || s_ship.x > PANEL_W + 10) {
            s_ship.active = false;
            s_next_ship = tick + 750 + (uint32_t)(frand() * 750);
        }
    } else if (tick >= s_next_ship) {
        int ltr = frand() < 0.5f;
        s_ship = (spacecraft_t){ .active=true,
                                 .x=ltr ? -8 : PANEL_W + 7,
                                 .vx=ltr ? 0.35f : -0.35f,
                                 .y=8 + (int)(frand() * 16) };
    }
}

static void draw_ship(uint32_t tick) {
    if (!s_ship.active) return;
    int blink_on = (tick % 12) < 6;
    int bx = (int)(s_ship.x + 0.5f);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 7; ++col) {
            uint8_t cell = SHIP_SPRITE[row][col];
            if (!cell) continue;
            int dx = bx + col - 3;
            int dy = s_ship.y + row - 1;
            uint8_t r, g, b;
            if      (cell == 1) { r=220; g=225; b=235; }
            else if (cell == 2) { r=120; g=130; b=145; }
            else /* 3 */        { if (blink_on) { r=255; g=60; b=60; } else { r=70; g=20; b=20; } }
            panel_pixel(dx, dy, r, g, b);
        }
    }
}

static void scene_moon(const daily_data_t *d, uint32_t tick) {
    // Pure black sky
    gfx_rect(0, 0, PANEL_W, PANEL_H, 0, 0, 0);

    // Twinkling stars
    for (size_t i = 0; i < NUM_NIGHT_STARS; ++i) {
        int x = NIGHT_STARS[i][0];
        int y = NIGHT_STARS[i][1];
        float ph = sinf(tick * 0.08f + x * 0.4f + y * 0.6f);
        int v = (int)(95 + ph * 75);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        int b = v + 20;
        if (b > 255) b = 255;
        panel_pixel(x, y, (uint8_t)v, (uint8_t)v, (uint8_t)b);
    }

    // Moon — hero of the scene
    time_t now = 0; time(&now);
    float phase01 = gfx_moon_phase_now(now);
    gfx_moon(MOON_CX, MOON_CY, MOON_R, phase01);

    // Cloud composite: paint cloud pixels above an alpha threshold.
    // Tint toward "moonlit" near the moon, cool-blue elsewhere.
    for (int y = 0; y < PANEL_H; ++y) {
        for (int x = 0; x < PANEL_W; ++x) {
            float a = cloud_alpha(x, y, tick);
            if (a < 0.45f) continue;
            int dx = x - MOON_CX, dy = y - MOON_CY;
            float dist = sqrtf((float)(dx*dx + dy*dy));
            float halo = 1.0f - dist / (float)(MOON_R + 8);
            if (halo < 0) halo = 0;
            float lit = halo * halo;
            int cR = (int)(50  + (195 - 50)  * lit);
            int cG = (int)(70  + (210 - 70)  * lit);
            int cB = (int)(110 + (240 - 110) * lit);
            // Density-driven brightness above threshold
            float density = (a - 0.45f) / 0.55f;
            if (density < 0) density = 0;
            cR = (int)(cR * (0.55f + 0.45f * density));
            cG = (int)(cG * (0.55f + 0.45f * density));
            cB = (int)(cB * (0.55f + 0.45f * density));
            panel_pixel(x, y, (uint8_t)cR, (uint8_t)cG, (uint8_t)cB);
        }
    }

    // Shooting star + spacecraft on top
    update_shooting(tick);
    update_ship(tick);
    draw_shooting();
    draw_ship(tick);

    // Time top-left
    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8];
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        gfx_text_outlined(1, 1, clk, 140, 125, 195);
    }

    // Bottom ticker: phase name + % lit
    float days = phase01 * 29.530588f;
    const char *name = moon_phase_name(days);
    int lit_pct = (int)(50.0f * (1.0f - cosf(phase01 * 2.0f * (float)M_PI)));
    char line[64];
    snprintf(line, sizeof(line), "MOON  ~  %s  ~  %d%% LIT", name, lit_pct);
    gfx_ticker(PANEL_H - 6, line, gfx_ticker_scroll(tick), 160, 160, 120);

    (void)d;  // moon scene doesn't need daily data
}

// ─── scene_event (Earth events — global map with day/night) ──────────────

static uint32_t imul32(uint32_t a, uint32_t b) { return a * b; }

// Hash → 32-bit, decent distribution; same form as the simulator (Math.imul + xorshift)
static uint32_t pix_hash(int x, int y) {
    uint32_t h = imul32((uint32_t)x, 374761393u) + imul32((uint32_t)y, 668265263u);
    h = imul32(h ^ (h >> 13), 1274126177u);
    return h ^ (h >> 16);
}

static uint32_t pix_hash2(int x, int y) {
    uint32_t h = imul32((uint32_t)x, 2246822519u) + imul32((uint32_t)y, 3266489917u);
    h = imul32(h ^ (h >> 15), 2246822507u);
    return h ^ (h >> 13);
}

static void event_marker_color(const char *cat,
                               uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = 255; *g = 160; *b = 60;  // default orange
    if (!cat) return;
    if      (contains_ci(cat, "earthquake")) { *r=255; *g=220; *b=50;  }
    else if (contains_ci(cat, "volcano"))    { *r=255; *g=80;  *b=0;   }
    else if (contains_ci(cat, "storm"))      { *r=180; *g=180; *b=255; }
    else if (contains_ci(cat, "fire"))       { *r=255; *g=60;  *b=0;   }
    else if (contains_ci(cat, "flood"))      { *r=60;  *g=160; *b=255; }
    else if (contains_ci(cat, "ice") ||
             contains_ci(cat, "snow"))       { *r=200; *g=230; *b=255; }
}

static void scene_event(const daily_data_t *d, uint32_t tick) {
    time_t now = 0; time(&now);
    gfx_subsolar_t sub = gfx_subsolar_point(now);

    // Per-row hoist: lat is constant across x
    for (int y = 0; y < PANEL_H; ++y) {
        float lat = 90.0f - (y + 0.5f) / PANEL_H * 180.0f;
        for (int x = 0; x < PANEL_W; ++x) {
            float lon = (x + 0.5f) / PANEL_W * 360.0f - 180.0f;
            float sf  = gfx_solar_factor(lon, lat, sub);

            float dayMix;
            if      (sf >=  0.20f) dayMix = 1.0f;
            else if (sf >= -0.10f) dayMix = (sf + 0.10f) / 0.30f;
            else                   dayMix = 0.0f;
            int warm = (sf > -0.05f && sf < 0.20f) ? 1 : 0;

            float r = 0, g = 0, b = 0;
            bool  land = world_is_land(x, y);
            uint8_t cloud_d = world_cloud(x, y);

            if (land) {
                uint32_t h = pix_hash(x, y);
                // Dark-side city lights — skip Antarctica (lat<-60), suppress under heavy cloud
                if (dayMix < 0.05f && lat > -60.0f && cloud_d < 115 && (h & 1u) == 0) {
                    float flick = 0.75f + 0.25f * sinf(tick * 0.35f + (h & 0xff) * 0.1f);
                    r = 255.0f * flick;
                    g = 175.0f * flick;
                    b =  60.0f * flick;
                } else {
                    float dayR = 60.0f  + warm * 35.0f;
                    float dayG = 135.0f - warm * 15.0f;
                    float dayB = 75.0f  - warm * 10.0f;
                    float nR = 25.0f, nG = 45.0f, nB = 85.0f;
                    r = dayR * dayMix + nR * (1.0f - dayMix);
                    g = dayG * dayMix + nG * (1.0f - dayMix);
                    b = dayB * dayMix + nB * (1.0f - dayMix);
                }
            } else {
                float dayR = 15.0f, dayG = 45.0f, dayB = 95.0f;
                float nR = 3.0f, nG = 8.0f, nB = 22.0f;
                r = dayR * dayMix + nR * (1.0f - dayMix);
                g = dayG * dayMix + nG * (1.0f - dayMix);
                b = dayB * dayMix + nB * (1.0f - dayMix);

                uint32_t sh = pix_hash2(x, y);
                if ((sh & 7u) == 0u) {
                    float wave  = 0.5f + 0.5f * sinf(tick * 0.12f + (sh & 0x3ff) * 0.01f);
                    float boost = 6.0f + wave * 16.0f;
                    r += boost * 0.3f;
                    g += boost * 0.6f;
                    b += boost * 1.0f;
                }
                if (dayMix > 0.40f && ((sh >> 4) & 15u) == 0u) {
                    float flick    = 0.55f + 0.45f * sinf(tick * 0.125f + (sh & 0xff) * 0.08f);
                    float strength = flick * dayMix;
                    r += 55.0f  * strength;
                    g += 95.0f  * strength;
                    b += 125.0f * strength;
                }
                if (sf > 0.55f) {
                    float gl = (sf - 0.55f) / 0.45f;
                    r += gl * 40.0f;
                    g += gl * 60.0f;
                    b += gl * 75.0f;
                }
            }

            // Cloud overlay — blend white-ish/blue-grey by day/night
            if (cloud_d > 5) {
                float density = cloud_d / 255.0f;
                float MAX_DAY = 0.40f, MAX_NIGHT = 0.20f;
                float ceiling = MAX_NIGHT + (MAX_DAY - MAX_NIGHT) * dayMix;
                float a = density * ceiling;
                float cR = 195.0f * dayMix + 30.0f * (1.0f - dayMix);
                float cG = 210.0f * dayMix + 40.0f * (1.0f - dayMix);
                float cB = 230.0f * dayMix + 60.0f * (1.0f - dayMix);
                r = r * (1.0f - a) + cR * a;
                g = g * (1.0f - a) + cG * a;
                b = b * (1.0f - a) + cB * a;
            }

            int ir = (int)(r + 0.5f);
            int ig = (int)(g + 0.5f);
            int ib = (int)(b + 0.5f);
            if (ir > 255) ir = 255;
            if (ig > 255) ig = 255;
            if (ib > 255) ib = 255;
            if (ir < 0) ir = 0;
            if (ig < 0) ig = 0;
            if (ib < 0) ib = 0;
            panel_pixel(x, y, (uint8_t)ir, (uint8_t)ig, (uint8_t)ib);
        }
    }

    // Event markers — pulsing rings
    int pulse = (int)((tick * 15) / 100) % 4;
    for (int i = 0; i < d->event_count; ++i) {
        float lon = d->events[i].lon, lat = d->events[i].lat;
        int mx = (int)((lon + 180.0f) / 360.0f * PANEL_W);
        int my = (int)((90.0f - lat)  / 180.0f * PANEL_H);
        uint8_t mr, mg, mb;
        event_marker_color(d->events[i].category, &mr, &mg, &mb);
        if (pulse < 3) gfx_circle(mx, my, pulse, mr, mg, mb, /*fill=*/0);
        panel_pixel(mx, my, 255, 255, 200);
    }

    // Time bottom-left (above ticker)
    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8];
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        gfx_text_outlined(1, PANEL_H - 14, clk, 255, 255, 255);
    }

    // Ticker — events list or "ALL QUIET"
    char line[256];
    if (d->event_count > 0) {
        size_t off = (size_t)snprintf(line, sizeof(line), "EARTH EVENTS  ~  ");
        for (int i = 0; i < d->event_count && off + 32 < sizeof(line); ++i) {
            off += (size_t)snprintf(line + off, sizeof(line) - off,
                                    "%s%s",
                                    d->events[i].title,
                                    (i < d->event_count - 1) ? "  ~  " : "");
        }
    } else {
        snprintf(line, sizeof(line), "EARTH EVENTS  ~  ALL QUIET");
    }
    gfx_ticker(25, line, gfx_ticker_scroll(tick), 255, 160, 60);
}

// ─── scene_asteroid (Earth pan + flyby + split-flap callouts) ────────────

#define ASTEROID_SCENE_TICKS  (18 * 1000 / FRAME_MS)   // 18 s — cinematic pacing

static float ease_inout(float x) {
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    return (x < 0.5f) ? 2.0f * x * x : 1.0f - powf(-2.0f * x + 2.0f, 2.0f) * 0.5f;
}

static char roll_char(int slot, uint32_t tk) {
    static const char DECK[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    float r = sinf((slot + 1) * 37.3f + tk * 7.1f) * 43758.5453f;
    float f = r - floorf(r);
    if (f < 0) f += 1.0f;
    int idx = (int)(f * (sizeof(DECK) - 1));
    if (idx < 0) idx = 0;
    if (idx >= (int)sizeof(DECK) - 1) idx = sizeof(DECK) - 2;
    return DECK[idx];
}

#define FLAP_ROLL  5    // ticks a slot rolls before locking
#define FLAP_STAG  2    // ticks of offset between adjacent slots
#define FLAP_HOLD  35   // ~1.2 s holding the label before flipping to value

// Returns rendered string for a label→value split-flap pair given the
// current scene tick. Writes into `out`, up to cap bytes (incl NUL).
static void split_flap(char *out, size_t cap,
                       const char *label, const char *value,
                       uint32_t vt, uint32_t start_tick) {
    int Llen = (int)strlen(label);
    int Vlen = (int)strlen(value);
    int lab_ticks = Llen ? (Llen - 1) * FLAP_STAG + FLAP_ROLL : 0;
    int val_ticks = Vlen ? (Vlen - 1) * FLAP_STAG + FLAP_ROLL : 0;
    uint32_t lab_end   = start_tick + lab_ticks;
    uint32_t hold_end  = lab_end + FLAP_HOLD;
    uint32_t val_start = hold_end;
    uint32_t val_end   = val_start + val_ticks;

    const char *target = NULL;
    uint32_t phase_start = 0;
    bool show_full = false;

    if (vt < start_tick)          { out[0] = '\0'; return; }
    else if (vt < lab_end)        { target = label; phase_start = start_tick; }
    else if (vt < hold_end)       { strlcpy(out, label, cap); return; }
    else if (vt < val_end)        { target = value; phase_start = val_start; }
    else                          { strlcpy(out, value, cap); return; }

    (void)show_full;
    size_t pos = 0;
    int n = (int)strlen(target);
    for (int i = 0; i < n && pos < cap - 1; ++i) {
        char ch = target[i];
        if (ch == ' ') { out[pos++] = ' '; continue; }
        uint32_t s_start = phase_start + i * FLAP_STAG;
        uint32_t s_lock  = s_start + FLAP_ROLL;
        if (vt < s_start)      out[pos++] = ' ';
        else if (vt < s_lock)  out[pos++] = roll_char(i, vt);
        else                   out[pos++] = ch;
    }
    out[pos] = '\0';
}

static void scene_asteroid(const daily_data_t *d, uint32_t vt) {
    // Pure black sky + stars
    gfx_rect(0, 0, PANEL_W, PANEL_H, 0, 0, 0);
    gfx_stars(30, 25, 180, 180, 180);

    bool have = d->has_asteroids;
    long miss_km = have ? d->closest_miss_km : 384400L;

    // Timing — scene fraction over ASTEROID_SCENE_TICKS
    const uint32_t TMAX = ASTEROID_SCENE_TICKS;
    float t = (float)vt / (float)TMAX;
    if (t > 1.0f) t = 1.0f;

    const float T_OPEN    = 0.22f;
    const float T_FLY_END = 0.70f;
    const int   EARTH_DELAY_TICKS = (int)(0.10f * TMAX);
    float earth_delay = (float)EARTH_DELAY_TICKS / (float)TMAX;
    uint32_t open_t = EARTH_DELAY_TICKS + (uint32_t)(TMAX * T_OPEN);

    // Earth pan/zoom — start centred small, drift down-left and grow
    float o = ease_inout((t - earth_delay) / T_OPEN);
    int cx0 = 32, cy0 = 16, cx1 = -2, cy1 = 46;
    float r0 = 11.0f, r1 = 32.0f;
    int   ecx = (int)(cx0 + (cx1 - cx0) * o + 0.5f);
    int   ecy = (int)(cy0 + (cy1 - cy0) * o + 0.5f);
    float er  = r0 + (r1 - r0) * o;
    gfx_earth_sprite(ecx, ecy, er);

    // Asteroid flyby — runs in parallel
    if (have && t > 0.0f) {
        float u  = t / T_FLY_END;
        if (u > 1.0f) u = 1.0f;
        float ue = ease_inout(u);
        float sx = PANEL_W + 1, sy = 16;
        float ex = 32,           ey = -6;
        bool hazardous = false;   // we don't currently parse this flag — assume safe
        uint8_t bodyR = hazardous ? 255 : 255;
        uint8_t bodyG = hazardous ? 80  : 220;
        uint8_t bodyB = hazardous ? 40  : 160;
        int coolR = 30, coolG = 50, coolB = 90;

        // Trail
        const int N = 80;
        for (int i = 0; i <= N; ++i) {
            float tu = (i / (float)N) * ue;
            int tx = (int)(sx + (ex - sx) * tu + 0.5f);
            int ty = (int)(sy + (ey - sy) * tu + 0.5f);
            if (tx < 0 || tx >= PANEL_W || ty < 0 || ty >= PANEL_H) continue;
            float f  = ue > 0 ? tu / ue : 1.0f;
            float f2 = f * f;
            float fade = 0.35f + 0.65f * f;
            int r = (int)((bodyR * f2 + coolR * (1 - f2)) * fade);
            int g = (int)((bodyG * f2 + coolG * (1 - f2)) * fade);
            int b = (int)((bodyB * f2 + coolB * (1 - f2)) * fade);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            panel_pixel(tx, ty, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        }

        // Head + back pixel (gives the rock a sense of direction)
        float dxv = ex - sx, dyv = ey - sy;
        float vlen = sqrtf(dxv*dxv + dyv*dyv);
        if (vlen < 1e-3f) vlen = 1e-3f;
        float vx = dxv / vlen, vy = dyv / vlen;
        int ax = (int)(sx + (ex - sx) * ue + 0.5f);
        int ay = (int)(sy + (ey - sy) * ue + 0.5f);
        int bx = (int)(ax - vx + 0.5f);
        int by = (int)(ay - vy + 0.5f);
        if (bx >= 0 && bx < PANEL_W && by >= 0 && by < PANEL_H)
            panel_pixel(bx, by, (uint8_t)(bodyR * 0.55f),
                                (uint8_t)(bodyG * 0.55f),
                                (uint8_t)(bodyB * 0.55f));
        if (ax >= 0 && ax < PANEL_W && ay >= 0 && ay < PANEL_H)
            panel_pixel(ax, ay, 255, 245, 210);

        // Closest-approach tick — perpendicular from asteroid to Earth limb
        if (u > 0.45f && u < 0.95f) {
            float dx = ax - ecx, dy = ay - ecy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > er + 1) {
                float nx = dx / dist, ny = dy / dist;
                int lx = (int)(ecx + nx * (er + 1) + 0.5f);
                int ly = (int)(ecy + ny * (er + 1) + 0.5f);
                panel_pixel(lx, ly, 180, 200, 230);
                int mxm = (int)(ecx + nx * ((dist + er + 1) * 0.5f) + 0.5f);
                int mym = (int)(ecy + ny * ((dist + er + 1) * 0.5f) + 0.5f);
                panel_pixel(mxm, mym, 110, 130, 170);
            }
        }
    }

    // Time top-left
    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8];
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        gfx_text_outlined(1, 1, clk, 180, 180, 180);
    }

    // Intro title "CLOSE APPROACHES" — roll-in / hold / roll-out
    {
        const char *INTRO = "CLOSE APPROACHES";
        const int I_IN = 6, I_HOLD = 22, I_OUT = 6;
        int intro_end = I_IN + I_HOLD + I_OUT;
        if ((int)vt < intro_end) {
            char buf[24];
            int n = (int)strlen(INTRO);
            if (n > 23) n = 23;
            if ((int)vt < I_IN) {
                for (int i = 0; i < n; ++i) buf[i] = (INTRO[i] == ' ') ? ' ' : roll_char(i, vt);
            } else if ((int)vt < I_IN + I_HOLD) {
                memcpy(buf, INTRO, n);
            } else {
                uint32_t tk = vt - (I_IN + I_HOLD);
                for (int i = 0; i < n; ++i) buf[i] = (INTRO[i] == ' ') ? ' ' : roll_char(i + 13, tk);
            }
            buf[n] = '\0';
            int w = gfx_text_width(buf);
            int x = (PANEL_W - w) / 2;
            if (x < 0) x = 0;
            gfx_text_outlined(x, PANEL_H - 6, buf, 255, 200, 120);
        }
    }

    // Split-flap callouts (DISTANCE / DIAMETER / NAME) — start when intro ends
    if (have) {
        char km_str[16], diam_str[16], name_str[24];
        if (miss_km >= 10000)
            snprintf(km_str, sizeof(km_str), "%ldK KM", miss_km / 1000);
        else
            snprintf(km_str, sizeof(km_str), "%ld KM", miss_km);
        snprintf(diam_str, sizeof(diam_str), "~%dM", d->closest_diameter_m);

        // Sanitize name: drop parens, uppercase
        const char *src = d->closest_name;
        size_t np = 0;
        for (; *src && np < sizeof(name_str) - 1; ++src) {
            if (*src == '(' || *src == ')') continue;
            char c = *src;
            if (c >= 'a' && c <= 'z') c -= 32;
            name_str[np++] = c;
        }
        name_str[np] = '\0';
        // Trim trailing space
        while (np > 0 && name_str[np - 1] == ' ') name_str[--np] = '\0';

        uint32_t start = open_t;
        char s1[32], s2[32], s3[32];
        split_flap(s1, sizeof(s1), "DISTANCE",       km_str,   vt, start);
        split_flap(s2, sizeof(s2), "DIAMETER",       diam_str, vt, start);
        split_flap(s3, sizeof(s3), "ASTEROID NAME",  name_str, vt, start);

        if (s1[0]) gfx_text_outlined(PANEL_W - gfx_text_width(s1) - 1, 13,        s1, 255, 200, 120);
        if (s2[0]) gfx_text_outlined(PANEL_W - gfx_text_width(s2) - 1, 20,        s2, 200, 220, 255);
        if (s3[0]) gfx_text_outlined(PANEL_W - gfx_text_width(s3) - 1, PANEL_H-6, s3, 255, 160,  80);
    } else {
        const char *msg = "NO CLOSE APPROACHES";
        gfx_text_outlined(PANEL_W - gfx_text_width(msg) - 1, PANEL_H - 6,
                          msg, 140, 160, 200);
    }
}

// ─── scene_uv (UV index — rays falling onto Earth) ──────────────────────

#define UV_PLANET_CX  32
#define UV_PLANET_CY  84
#define UV_PLANET_R   60
#define UV_ATM_R      62
#define UV_HALO_MAX   4
#define UV_TOP_HALO   3

typedef struct { float upper; const char *name; uint8_t r, g, b; } uv_band_t;
static const uv_band_t UV_BANDS[] = {
    {   3.0f, "LOW",      90, 200, 110 },
    {   6.0f, "MOD",     240, 220,  80 },
    {   8.0f, "HIGH",    240, 150,  60 },
    {  11.0f, "V.HIGH",  230,  80,  80 },
    { 1e9f,   "EXTREME", 200, 110, 220 },
};

static const uv_band_t *uv_band_for(float v) {
    for (size_t i = 0; i < sizeof(UV_BANDS)/sizeof(UV_BANDS[0]); ++i) {
        if (v < UV_BANDS[i].upper) return &UV_BANDS[i];
    }
    return &UV_BANDS[sizeof(UV_BANDS)/sizeof(UV_BANDS[0]) - 1];
}

// Pre-computed per-column planet/atmosphere top Ys. 32767 means off-arc.
static int16_t s_uv_planet_top[PANEL_W];
static int16_t s_uv_atm_top[PANEL_W];
static bool    s_uv_arcs_ready = false;

static void uv_compute_arcs(void) {
    if (s_uv_arcs_ready) return;
    for (int x = 0; x < PANEL_W; ++x) {
        float dx = (float)(x - UV_PLANET_CX);
        float ps = (float)(UV_PLANET_R * UV_PLANET_R) - dx * dx;
        float as = (float)(UV_ATM_R    * UV_ATM_R)    - dx * dx;
        s_uv_planet_top[x] = (ps >= 0) ? (int16_t)(UV_PLANET_CY - sqrtf(ps) + 0.5f) : 32767;
        s_uv_atm_top[x]    = (as >= 0) ? (int16_t)(UV_PLANET_CY - sqrtf(as) + 0.5f) : 32767;
    }
    s_uv_arcs_ready = true;
}

// Pre-baked land map: jittered ellipses give a few continent-sized blobs
// across the visible cap, instead of speckled per-pixel noise.
static uint8_t s_uv_land[PANEL_W * PANEL_H];
static bool    s_uv_land_ready = false;

static void uv_build_land(void) {
    if (s_uv_land_ready) return;
    static const struct { int cx, cy, rx, ry; } CONT[] = {
        { 13, 28,  8, 3 },
        { 30, 27, 11, 4 },
        { 47, 28,  7, 3 },
        { 22, 30,  4, 2 },
        { 38, 30,  3, 2 },
        { 56, 31,  3, 1 },
    };
    int n = sizeof(CONT) / sizeof(CONT[0]);
    for (int y = 0; y < PANEL_H; ++y) {
        for (int x = 0; x < PANEL_W; ++x) {
            uint8_t land = 0;
            for (int i = 0; i < n; ++i) {
                float dx = (float)(x - CONT[i].cx) / CONT[i].rx;
                float dy = (float)(y - CONT[i].cy) / CONT[i].ry;
                float r2 = dx * dx + dy * dy;
                float jit = ((pix_hash(x + CONT[i].cx * 7, y + CONT[i].cy * 13) & 0xffff)
                             / 65535.0f - 0.5f) * 0.45f;
                if (r2 + jit < 1.0f) { land = 1; break; }
            }
            s_uv_land[y * PANEL_W + x] = land;
        }
    }
    s_uv_land_ready = true;
}

static float frac_hash01(int x, int y) {
    return (float)(pix_hash(x, y) & 0xffff) / 65535.0f;
}

static void scene_uv(const daily_data_t *d, uint32_t tick) {
    gfx_rect(0, 0, PANEL_W, PANEL_H, 0, 0, 0);
    uv_compute_arcs();
    uv_build_land();

    if (!d->has_uv) {
        gfx_text_outlined(1, 1, "UV", 230, 200, 100);
        const char *msg = "NO DATA";
        int w = gfx_text_width(msg);
        gfx_text_outlined((PANEL_W - w) / 2, 14, msg, 120, 120, 120);
        return;
    }

    float uv_now = d->uv_index;
    const uv_band_t *band = uv_band_for(uv_now);
    uint8_t cr = band->r, cg = band->g, cb = band->b;

    // ── Twinkling stars (skip atmosphere-occluded columns) ────────────────
    static const uint8_t UV_STARS[][2] = {
        { 4,  6}, {11,  3}, {17, 10}, {23,  5},
        {38,  4}, {44, 11}, {50,  6}, {57, 13}, {60,  3},
    };
    for (size_t i = 0; i < sizeof(UV_STARS) / sizeof(UV_STARS[0]); ++i) {
        int sx = UV_STARS[i][0], sy = UV_STARS[i][1];
        if (s_uv_atm_top[sx] <= sy) continue;
        float ph = sinf(tick * 0.07f + sx * 0.5f + sy * 0.4f);
        int v = (int)(55 + ph * 35);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        int bv = v + 18;
        if (bv > 255) bv = 255;
        panel_pixel(sx, sy, (uint8_t)v, (uint8_t)v, (uint8_t)bv);
    }

    // ── Top "source" gradient band ────────────────────────────────────────
    // Brightness scales with UV: faint hint at night, full glow at peak.
    float source_i = 0.15f + 0.85f * fminf(1.0f, fmaxf(0.0f, uv_now / 11.0f));
    for (int yy = 0; yy <= UV_TOP_HALO; ++yy) {
        float f = (yy == 0) ? source_i
                            : (1.0f - (float)yy / (UV_TOP_HALO + 1)) *
                              (1.0f - (float)yy / (UV_TOP_HALO + 1)) * source_i;
        uint8_t tr = (uint8_t)(cr * f);
        uint8_t tg = (uint8_t)(cg * f);
        uint8_t tb = (uint8_t)(cb * f);
        for (int x = 0; x < PANEL_W; ++x) panel_pixel(x, yy, tr, tg, tb);
    }

    // ── Falling UV rays ───────────────────────────────────────────────────
    float uv_strength = fminf(1.0f, fmaxf(0.10f, uv_now / 11.0f));
    for (int x = 0; x < PANEL_W; ++x) {
        float col_hash    = frac_hash01(x, 11);
        float col_weight  = 0.45f + 0.55f * frac_hash01(x, 23);
        float po          = col_hash * 6.2832f;
        float col_base    = 0.30f
            + 0.30f * sinf(x * 0.42f + tick * 0.022f + po)
            + 0.22f * sinf(x * 0.71f - tick * 0.041f + po * 1.9f)
            + 0.18f * sinf(x * 0.17f + tick * 0.013f + po * 0.6f);
        if (col_base < 0) col_base = 0;
        float I = col_base * col_weight * uv_strength;
        if (I <= 0.04f) continue;

        int stop = (s_uv_atm_top[x] < PANEL_H) ? s_uv_atm_top[x] - 1 : PANEL_H - 1;
        if (stop > PANEL_H - 1) stop = PANEL_H - 1;

        for (int y = 1; y <= stop; ++y) {
            float n1 = 0.5f + 0.5f * sinf(y * 0.45f - tick * 0.32f + x * 0.21f + po);
            float n2 = 0.5f + 0.5f * sinf(y * 0.32f + tick * 0.18f + x * 0.13f + po * 1.3f);
            float along  = n1 * 0.60f + n2 * 0.40f;
            float jitter = (frac_hash01(x * 13 + 7, y * 17 + 3) - 0.5f) * 0.22f;
            float a = I * along * 1.35f + jitter;
            if (a < 0.06f) continue;
            if (a > 1.0f)  a = 1.0f;
            float core = (a - 0.62f) / 0.38f;
            if (core < 0) core = 0;
            int rr = (int)(cr * a + (255 - cr) * core * 0.6f);
            int gg = (int)(cg * a + (255 - cg) * core * 0.6f);
            int bb = (int)(cb * a + (255 - cb) * core * 0.6f);
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            panel_pixel(x, y, (uint8_t)rr, (uint8_t)gg, (uint8_t)bb);
        }
    }

    // ── Atmosphere band: bluish-grey haze with shimmer, bright cyan rim ───
    float rim_dim = 1.0f + 0.15f * uv_strength;
    for (int x = 0; x < PANEL_W; ++x) {
        int aT = s_uv_atm_top[x], pT = s_uv_planet_top[x];
        if (aT >= PANEL_H) continue;
        int band_h = (pT - aT);
        if (band_h < 1) band_h = 1;
        int y_end = (pT < PANEL_H) ? pT : PANEL_H;
        for (int y = aT; y < y_end; ++y) {
            float t = (float)(y - aT) / band_h;
            float shimmer = 0.5f + 0.5f * sinf(x * 0.18f + y * 0.22f + tick * 0.045f);
            float lift = 0.85f + shimmer * 0.30f;
            int rr = (int)((70  + 40 * t) * lift);
            int gg = (int)((95  + 55 * t) * lift);
            int bb = (int)((135 + 60 * t) * lift);
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            panel_pixel(x, y, (uint8_t)rr, (uint8_t)gg, (uint8_t)bb);
        }
        // Outer cyan rim (slightly boosted by UV)
        if (aT >= 0 && aT < PANEL_H) {
            int rr = (int)(150 * rim_dim);
            int gg = (int)(200 * rim_dim);
            int bbb = (int)(245 * rim_dim);
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bbb > 255) bbb = 255;
            panel_pixel(x, aT, (uint8_t)rr, (uint8_t)gg, (uint8_t)bbb);
        }
        // Soft exosphere halo above the rim (quadratic falloff).
        for (int i = 1; i <= UV_HALO_MAX; ++i) {
            int yy = aT - i;
            if (yy < 1) break;
            float t = 1.0f - (float)i / (UV_HALO_MAX + 1);
            float f = rim_dim * 0.65f * t * t;
            int rr  = (int)(150 * f);
            int gg  = (int)(200 * f);
            int bbb = (int)(245 * f);
            // No read access to the framebuffer in firmware; halo overwrites
            // whatever's underneath. Rays beneath are usually brighter so we
            // lose some detail vs the simulator's max-blend, but the limb's
            // outer falloff still reads correctly.
            panel_pixel(x, yy, (uint8_t)rr, (uint8_t)gg, (uint8_t)bbb);
        }
    }

    // ── Planet — ocean gradient + pre-baked continent land mask ───────────
    for (int x = 0; x < PANEL_W; ++x) {
        int pT = s_uv_planet_top[x];
        if (pT >= PANEL_H) continue;
        float dxf = (float)(x - UV_PLANET_CX);
        for (int y = pT; y < PANEL_H; ++y) {
            float d = (float)(y - pT) / fmaxf(1.0f, (float)(PANEL_H - pT));
            int rr, gg, bb;
            if (s_uv_land[y * PANEL_W + x]) {
                float lv = frac_hash01(x * 7, y * 11);
                rr = (int)(28 + lv * 26);
                gg = (int)(55 + lv * 40);
                bb = (int)(60 + lv * 38);
            } else {
                float horiz = 0.85f + 0.15f * sinf(dxf * 0.18f + tick * 0.012f);
                float baseR = 30  + (1 - d) * 75;
                float baseG = 90  + (1 - d) * 95;
                float baseB = 160 + (1 - d) * 75;
                rr = (int)(baseR * horiz);
                gg = (int)(baseG * horiz);
                bb = (int)(baseB * horiz);
            }
            if (rr > 255) rr = 255;
            if (gg > 255) gg = 255;
            if (bb > 255) bb = 255;
            panel_pixel(x, y, (uint8_t)rr, (uint8_t)gg, (uint8_t)bb);
        }
    }

    // ── NOW / MAX text — dark backdrop + outlined glyphs ──────────────────
    // Firmware can't read the framebuffer to dim what's underneath, so paint
    // a solid black rectangle where the text will land. ESP-IDF nano-printf
    // doesn't support %f, so format the decimal manually.
    char nbuf[16];
    {
        float v = uv_now < 0 ? 0 : (uv_now > 99.9f ? 99.9f : uv_now);
        int whole = (int)v;
        int frac  = (int)((v - whole) * 10.0f + 0.5f);
        if (frac >= 10) { whole++; frac = 0; }
        snprintf(nbuf, sizeof(nbuf), "%d.%d", whole, frac);
    }
    int now_lblw = gfx_text_width("NOW");
    int now_valw = gfx_text_width(nbuf);
    int now_w    = (now_lblw > now_valw ? now_lblw : now_valw) + 2;
    gfx_rect(0, 5, now_w, 12, 0, 0, 0);
    gfx_text_outlined(1, 6,  "NOW", 200, 200, 200);
    gfx_text_outlined(1, 12, nbuf, cr, cg, cb);

    char mbuf[16];
    {
        float v = d->uv_daily_max < 0 ? 0 : (d->uv_daily_max > 99.9f ? 99.9f : d->uv_daily_max);
        int whole = (int)v;
        int frac  = (int)((v - whole) * 10.0f + 0.5f);
        if (frac >= 10) { whole++; frac = 0; }
        snprintf(mbuf, sizeof(mbuf), "%d.%d", whole, frac);
    }
    int max_lblw = gfx_text_width("MAX");
    int max_valw = gfx_text_width(mbuf);
    int max_w    = (max_lblw > max_valw ? max_lblw : max_valw) + 2;
    gfx_rect(PANEL_W - max_w, 5, max_w, 12, 0, 0, 0);
    gfx_text_outlined(PANEL_W - max_lblw - 1, 6,  "MAX", 200, 200, 200);
    const uv_band_t *mb = uv_band_for(d->uv_daily_max);
    gfx_text_outlined(PANEL_W - max_valw - 1, 12, mbuf, mb->r, mb->g, mb->b);
}

// ─── scene_broadcast (button-triggered daily briefing) ──────────────────
// Off-rotation: only reached via the encoder press. Streams briefing.mp3
// from the server, decodes with minimp3, and plays through I2S while the
// scene draws a spectrum-analyzer view of the audio the user is hearing.
// LOADING state shows a pulsing-ring placeholder; PLAYING state runs a
// 512-point real FFT and draws 32 mirrored bars — mirrors the web sim's
// sceneRadio (display/index.html).

#include "dsps_fft2r.h"
#include "dsps_wind.h"

#define BC_FFT_N       512
#define BC_BAR_BINS    96      // first 96 bins ≈ 0..4.5 kHz at 24 kHz SR
#define BC_NBARS       32

static bool     s_bc_fft_ready = false;
static float    s_bc_wind[BC_FFT_N];
static float    s_bc_fft[BC_FFT_N * 2];   // interleaved complex
static int16_t  s_bc_pcm[BC_FFT_N];

static void bc_fft_init_once(void) {
    if (s_bc_fft_ready) return;
    if (dsps_fft2r_init_fc32(NULL, BC_FFT_N) == ESP_OK) {
        dsps_wind_hann_f32(s_bc_wind, BC_FFT_N);
        s_bc_fft_ready = true;
    }
}

static void draw_broadcast_bars(uint32_t tick) {
    (void)tick;
    bc_fft_init_once();
    if (!s_bc_fft_ready) return;

    audio_pcm_snapshot(s_bc_pcm, BC_FFT_N);

    // Windowed, normalized real → complex-with-zero-imag input.
    for (int i = 0; i < BC_FFT_N; ++i) {
        s_bc_fft[2*i]     = (float)s_bc_pcm[i] * (1.0f / 32768.0f) * s_bc_wind[i];
        s_bc_fft[2*i + 1] = 0.0f;
    }
    dsps_fft2r_fc32(s_bc_fft, BC_FFT_N);
    dsps_bit_rev_fc32(s_bc_fft, BC_FFT_N);

    // Per-bin magnitude for the useful spectrum.
    float mag[BC_BAR_BINS];
    for (int k = 0; k < BC_BAR_BINS; ++k) {
        float re = s_bc_fft[2*k];
        float im = s_bc_fft[2*k + 1];
        mag[k] = sqrtf(re * re + im * im);
    }

    const int bar_area_h = PANEL_H - 8;     // top 24 rows for the bars
    const int cy         = bar_area_h / 2;  // vertical center row
    const int max_half   = cy - 1;

    for (int i = 0; i < BC_NBARS; ++i) {
        int b0 = i * BC_BAR_BINS / BC_NBARS;
        int b1 = (i + 1) * BC_BAR_BINS / BC_NBARS;
        if (b1 <= b0) b1 = b0 + 1;
        float sum = 0.0f;
        for (int k = b0; k < b1; ++k) sum += mag[k];
        float m_lin = sum / (float)(b1 - b0);

        // Amplitude → dB compression. Voice content spans a wide dynamic
        // range and linear scaling saturates the top instantly. Mapping
        // [-50 dB .. +10 dB] to [0 .. 1] gives the classic VU-meter feel
        // where quiet passages still show visible bars and peaks reach
        // full height without clipping the majority of the spectrum.
        float m;
        if (!isfinite(m_lin) || m_lin <= 1e-4f) {
            m = 0.0f;
        } else {
            float db = 20.0f * log10f(m_lin);
            m = (db + 50.0f) / 60.0f;
            if (!isfinite(m) || m < 0.0f) m = 0.0f;
            if (m > 1.0f) m = 1.0f;
        }

        // Defensive clamp: `(int)(NaN * …)` is undefined and can leak a
        // huge value that turns the inner dy loop into a multi-billion
        // iteration hang (and starves the task watchdog on CPU0). Two
        // independent guards — finite-check on m above and a hard bound
        // on half here — mean neither a decoder glitch nor a corrupt FFT
        // sample can freeze the panel.
        int half = (int)(m * (float)max_half + 0.5f);
        if (half < 0)        half = 0;
        if (half > max_half) half = max_half;
        int x = i * 2;

        // Dim center axis dot on every column so the analyzer reads as a
        // line at rest, not just black.
        panel_pixel(x, cy, 60, 45, 90);

        // Cool violet base with brightness proportional to magnitude and
        // a subtle tip-fade so peaks look pointier.
        for (int dy = -half; dy <= half; ++dy) {
            int y = cy + dy;
            if (y < 0 || y >= bar_area_h) continue;
            float tip = 1.0f - fabsf((float)dy) / ((float)half + 1.0f) * 0.35f;
            float lit = (0.45f + m * 0.45f) * tip;
            uint8_t r = (uint8_t)(180.0f * lit);
            uint8_t g = (uint8_t)(140.0f * lit);
            uint8_t b = (uint8_t)(255.0f * lit);
            panel_pixel(x, y, r, g, b);
        }
    }
}

static void draw_broadcast_rings(uint32_t tick) {
    int cx = PANEL_W / 2;
    int cy = PANEL_H / 2 - 3;
    for (int i = 0; i < 3; ++i) {
        int rad = ((int)tick + i * 6) % 18;
        int f   = 240 - rad * 12;
        if (f < 0) f = 0;
        gfx_circle(cx, cy, rad, (uint8_t)(f / 3), (uint8_t)(f / 2), (uint8_t)f, 0);
    }
}

static void scene_broadcast(const daily_data_t *d, uint32_t tick) {
    (void)d;
    gfx_rect(0, 0, PANEL_W, PANEL_H, 0, 0, 0);

    audio_briefing_state_t st = audio_briefing_state();
    if (st == AUDIO_BRIEFING_PLAYING) {
        draw_broadcast_bars(tick);
    } else {
        draw_broadcast_rings(tick);
    }

    // Time top-left (dim), matching the web sim
    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8];
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        gfx_text_outlined(1, 1, clk, 80, 80, 120);
    }

    if (st == AUDIO_BRIEFING_LOADING) {
        const char *msg = "LOADING";
        int w = gfx_text_width(msg);
        gfx_text_outlined((PANEL_W - w) / 2, PANEL_H - 7, msg, 220, 220, 100);
    } else {
        gfx_ticker(PANEL_H - 7, "ORRERY RADIO", gfx_ticker_scroll(tick),
                   180, 140, 255);
    }
}

// ─── scene_apod (NASA Astronomy Picture of the Day) ─────────────────────

#define APOD_SCENE_TICKS (14 * 1000 / FRAME_MS)   // 14 s — one ticker pass + hold

static void scene_apod(const daily_data_t *d, uint32_t tick) {
    const uint8_t *px = apod_pixels();

    if (!px) {
        // Loading state — starfield placeholder + status text.
        gfx_rect(0, 0, PANEL_W, PANEL_H, 0, 0, 0);
        gfx_stars(1, 20, 180, 180, 180);
        const char *msg = apod_loaded() ? "NO DATA" : "LOADING...";
        int w = gfx_text_width(msg);
        gfx_text_outlined((PANEL_W - w) / 2, 13, msg, 200, 200, 200);
        return;
    }

    // Blit the full 64×32 frame; the ticker's outlined glyphs read
    // cleanly on top, matching how the moon / event scenes overlay text.
    for (int y = 0; y < PANEL_H; ++y) {
        const uint8_t *row = px + y * PANEL_W * 3;
        for (int x = 0; x < PANEL_W; ++x) {
            const uint8_t *p = row + x * 3;
            panel_pixel(x, y, p[0], p[1], p[2]);
        }
    }

    // Time top-left (matches the moon / event scenes)
    struct tm tm;
    if (clock_now(&tm)) {
        char clk[8];
        snprintf(clk, sizeof(clk), "%02d:%02d", tm.tm_hour, tm.tm_min);
        gfx_text_outlined(1, 1, clk, 220, 220, 220);
    }

    // Single-pass ticker: starts offscreen right, scrolls off the left,
    // then leaves the image clean for the remainder of the scene.
    char line[128];
    const char *title = (d && d->has_apod && d->apod_title[0])
                        ? d->apod_title
                        : "ASTRONOMY PICTURE OF THE DAY";
    snprintf(line, sizeof(line), "APOD  ~  %s", title);
    int  w = gfx_text_width(line);
    int  x = (int)PANEL_W - (int)gfx_ticker_scroll(tick);
    if (x + w > 0) {
        gfx_text_outlined(x, PANEL_H - 6, line, 255, 220, 100);
    }
}

// ─── rotator ─────────────────────────────────────────────────────────────
// Adds scenes one at a time as each is verified on hardware.

#define SCENE_TICKS         (12 * 1000 / FRAME_MS)   // 12 s default

typedef struct {
    void   (*draw)(const daily_data_t *, uint32_t);
    uint32_t ticks;
} scene_def_t;

static const scene_def_t SCENES[] = {
    { scene_now,      SCENE_TICKS         },
    { scene_forecast, SCENE_TICKS         },
    { scene_uv,       SCENE_TICKS         },
    { scene_moon,     SCENE_TICKS         },
    { scene_event,    SCENE_TICKS         },
    { scene_asteroid, ASTEROID_SCENE_TICKS },
    { scene_apod,     APOD_SCENE_TICKS    },
};
#define NUM_SCENES (sizeof(SCENES) / sizeof(SCENES[0]))

void scenes_run(const daily_data_t *data) {
    uint32_t tick         = 0;
    size_t   idx          = 0;
    bool     in_broadcast = false;
    uint32_t bc_frame     = 0;
    while (true) {
        panel_clear();
        if (in_broadcast) {
            scene_broadcast(data, bc_frame++);
        } else {
            SCENES[idx].draw(data, tick);
        }
        panel_flip();
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));

        if (in_broadcast) {
            // A second press aborts the briefing; the pipeline will notice
            // on its next frame and clear the active flag. Meanwhile drain
            // the rotary delta so scrolling during playback isn't queued.
            if (encoder_read_pressed() > 0) audio_briefing_stop();
            encoder_read_delta();
            if (!audio_briefing_active()) {
                in_broadcast = false;
                tick         = 0;   // restart current scene animation
            }
            continue;
        }

        tick++;
        if (encoder_read_pressed() > 0) {
            audio_briefing_start(BRIEFING_MP3_URL);
            in_broadcast = true;
            bc_frame     = 0;
            continue;
        }

        int delta = encoder_read_delta();
        if (delta != 0) {
            int n = (int)NUM_SCENES;
            int i = (int)idx + delta;
            i    = ((i % n) + n) % n;   // safe modulo for negative delta
            idx  = (size_t)i;
            tick = 0;
        } else if (tick >= SCENES[idx].ticks) {
            tick = 0;
            idx  = (idx + 1) % NUM_SCENES;
        }
    }
}
