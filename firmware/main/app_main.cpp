#include "panel.h"
#include "wifi.h"
#include "daily.h"
#include "clock.h"
#include "scenes.h"
#include "world.h"
#include "wifi_creds.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "orrery";

#ifndef DEVICE_TZ
#define DEVICE_TZ "CET-1CEST,M3.5.0,M10.5.0/3"  // Zagreb / CET, default
#endif

#define FETCH_INTERVAL_MS (10 * 60 * 1000)

// Live data shared with the scene rotator. scenes_run only reads, the
// fetch task is the only writer — and writes are field-by-field via memcpy
// of a small struct, so a torn read just shows last-or-current values for
// one frame, which is fine for a display.
static daily_data_t g_data;

static void draw_status(const char *line1, uint8_t r, uint8_t g, uint8_t b) {
    panel_clear();
    int w = 5 * 6 - 1;
    (void)w;
    panel_draw_text(2, 12, line1, r, g, b);
}

static void fetch_task(void *arg) {
    (void)arg;
    while (true) {
        daily_data_t fresh;
        if (daily_fetch(&fresh)) {
            memcpy(&g_data, &fresh, sizeof(g_data));
            ESP_LOGI(TAG, "data: weather=%d asteroids=%d(%d) apod=%d",
                     g_data.has_weather,
                     g_data.has_asteroids, g_data.asteroid_count,
                     g_data.has_apod);
        } else {
            ESP_LOGW(TAG, "fetch failed, keeping previous data");
        }
        vTaskDelay(pdMS_TO_TICKS(FETCH_INTERVAL_MS));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "orrery boot — milestone 4: scene rotation");

    panel_init();
    panel_set_brightness(25);
    draw_status("BOOT", 200, 200, 200);

    draw_status("WIFI..", 255, 200, 0);
    if (!wifi_start_and_wait(20000)) {
        ESP_LOGE(TAG, "wifi failed");
        draw_status("NO WIFI", 255, 0, 0);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    clock_start(DEVICE_TZ);

    // First fetch synchronously so the rotator has data on its first tick.
    draw_status("FETCH..", 0, 255, 0);
    if (!daily_fetch(&g_data)) {
        ESP_LOGW(TAG, "first fetch failed; rotator will run with empty data");
        draw_status("RETRY", 255, 80, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // World map data (mask + clouds) for the Earth Event scene
    draw_status("MAP...", 80, 180, 255);
    world_fetch();

    xTaskCreate(fetch_task, "fetch", 8192, NULL, 4, NULL);

    scenes_run(&g_data);  // never returns
}
