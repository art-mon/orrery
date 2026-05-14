#include "scenes.h"
#include "panel.h"
#include "pins.h"
#include "font.h"
#include "clock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define FRAME_MS         33     // ~30 FPS
#define SCENE_TICKS      (10 * 1000 / FRAME_MS)   // 10 s per scene
#define MARQUEE_PX_PER_S 12     // scroll speed

typedef void (*scene_fn_t)(const daily_data_t *, uint32_t tick);
typedef bool (*scene_avail_t)(const daily_data_t *);

typedef struct {
    const char    *name;
    scene_avail_t  available;
    scene_fn_t     draw;
} scene_t;

// ─────────── helpers ───────────

// Centred single-line text at y.
static void draw_centred(int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int w = font_text_width(s);
    int x = (PANEL_W - w) / 2;
    if (x < 0) x = 0;
    panel_draw_text(x, y, s, r, g, b);
}

// Right-aligned text ending at panel right edge minus margin.
static void draw_right(int margin, int y, const char *s, uint8_t r, uint8_t g, uint8_t b) {
    int w = font_text_width(s);
    panel_draw_text(PANEL_W - margin - w, y, s, r, g, b);
}

// Single-line horizontal marquee. `pxs` is total pixels of scroll (use tick).
// Loops with a fixed gap so it reads naturally.
static void draw_marquee(int y, const char *s, uint32_t scroll_px,
                         uint8_t r, uint8_t g, uint8_t b) {
    int w = font_text_width(s);
    if (w <= 0) return;
    int gap = 16;
    int period = w + gap;
    int offset = -(int)(scroll_px % (uint32_t)period);
    for (int x = offset; x < PANEL_W; x += period) {
        panel_draw_text(x, y, s, r, g, b);
    }
}

// ─────────── scenes ───────────

static bool always(const daily_data_t *d) { (void)d; return true; }
static bool ifw(const daily_data_t *d)    { return d->has_weather; }
static bool ifa(const daily_data_t *d)    { return d->has_asteroids; }
static bool ifap(const daily_data_t *d)   { return d->has_apod; }

static void scene_clock(const daily_data_t *d, uint32_t tick) {
    (void)d;
    struct tm tm;
    bool synced = clock_now(&tm);

    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d %02d", tm.tm_hour, tm.tm_min);

    // Colon blinks: visible 0..500ms each second
    int colon_on = ((tick * FRAME_MS) / 500) & 1;

    // Centre the time, 11 px wide (5+1+5 with hidden colon)
    int w = font_text_width(hhmm);
    int x = (PANEL_W - w) / 2;
    panel_draw_text(x, 4, hhmm, 255, 220, 80);
    if (colon_on) {
        // Draw colon manually at the gap between H and M
        int cx = x + FONT_ADVANCE * 2;  // after two digits
        panel_pixel(cx + 2, 6,  255, 220, 80);
        panel_pixel(cx + 2, 10, 255, 220, 80);
    }

    // Date line
    char date[40];
    if (synced) {
        snprintf(date, sizeof(date), "%04d-%02d-%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    } else {
        snprintf(date, sizeof(date), "SYNCING");
    }
    draw_centred(18, date, 120, 160, 220);
}

static void scene_weather(const daily_data_t *d, uint32_t tick) {
    (void)tick;
    panel_draw_text(1, 2, d->city, 80, 160, 255);

    char temp[16];
    snprintf(temp, sizeof(temp), "%.1fC", d->temp_c);
    panel_draw_text(1, 12, temp, 255, 180, 60);

    char feels[16];
    snprintf(feels, sizeof(feels), "(%.0f)", d->feels_c);
    draw_right(1, 12, feels, 180, 120, 40);

    panel_draw_text(1, 22, d->condition, 200, 200, 200);
}

static void scene_asteroids(const daily_data_t *d, uint32_t tick) {
    char header[16];
    snprintf(header, sizeof(header), "%d NEAR", d->asteroid_count);
    draw_centred(2, header, 255, 100, 100);

    // Marquee with name + size + miss distance
    char line[64];
    snprintf(line, sizeof(line), "%s  %dm  %ldkm",
             d->closest_name, d->closest_diameter_m, d->closest_miss_km);
    uint32_t scroll = (tick * FRAME_MS * MARQUEE_PX_PER_S) / 1000;
    draw_marquee(13, line, scroll, 255, 180, 80);

    char dist[16];
    snprintf(dist, sizeof(dist), "%ldkm", d->closest_miss_km);
    draw_centred(23, dist, 160, 160, 200);
}

static void scene_apod(const daily_data_t *d, uint32_t tick) {
    draw_centred(2, "APOD", 100, 180, 255);
    uint32_t scroll = (tick * FRAME_MS * MARQUEE_PX_PER_S) / 1000;
    draw_marquee(14, d->apod_title, scroll, 220, 220, 255);
}

static const scene_t SCENES[] = {
    { "clock",     always, scene_clock     },
    { "weather",   ifw,    scene_weather   },
    { "asteroids", ifa,    scene_asteroids },
    { "apod",      ifap,   scene_apod      },
};
#define NUM_SCENES (sizeof(SCENES) / sizeof(SCENES[0]))

static int next_available(int from, const daily_data_t *d) {
    for (size_t i = 0; i < NUM_SCENES; ++i) {
        size_t idx = (from + i) % NUM_SCENES;
        if (SCENES[idx].available(d)) return (int)idx;
    }
    return 0;  // clock is always available
}

void scenes_run(const daily_data_t *data) {
    int       cur  = 0;
    uint32_t  tick = 0;
    while (true) {
        panel_clear();
        SCENES[cur].draw(data, tick);
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
        tick++;
        if (tick >= SCENE_TICKS) {
            tick = 0;
            cur  = next_available(cur + 1, data);
        }
    }
}
