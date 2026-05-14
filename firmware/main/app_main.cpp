#include "panel.h"
#include "wifi.h"
#include "daily.h"

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "orrery";

static void draw_status(const char *line1, uint8_t r, uint8_t g, uint8_t b) {
    panel_clear();
    panel_draw_text(0, 12, line1, r, g, b);
}

static void draw_weather(const daily_weather_t *w) {
    panel_clear();

    // Line 1: city (uppercased-ish — we keep as-is). y=2 → top.
    panel_draw_text(1, 2, w->city, 80, 160, 255);

    // Line 2: temperature with one decimal, suffix "C". y=14.
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f C", w->temp_c);
    panel_draw_text(1, 14, buf, 255, 180, 60);

    // Line 3: condition (lowercase). y=24.
    panel_draw_text(1, 24, w->condition, 200, 200, 200);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "orrery boot — milestone 3: wifi + daily.json");

    panel_init();
    panel_set_brightness(80);
    draw_status("BOOT", 200, 200, 200);

    draw_status("WIFI..", 255, 200, 0);
    if (!wifi_start_and_wait(20000)) {
        ESP_LOGE(TAG, "wifi failed");
        draw_status("NO WIFI", 255, 0, 0);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    daily_weather_t w;
    while (true) {
        draw_status("FETCH..", 0, 255, 0);
        if (daily_fetch(&w)) {
            ESP_LOGI(TAG, "weather: %s %.1fC %s", w.city, w.temp_c, w.condition);
            draw_weather(&w);
        } else {
            draw_status("FETCH ERR", 255, 0, 0);
        }
        // Refresh every 10 minutes
        vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));
    }
}
