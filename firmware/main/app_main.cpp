#include "panel.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "orrery";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "orrery boot — milestone 1: panel test pattern");

    panel_init();
    panel_draw_test_pattern();

    // Slow brightness breathing so it's obvious the MCU is alive
    uint8_t b = 20;
    int8_t  d = 4;
    while (true) {
        panel_set_brightness(b);
        b = (uint8_t)(b + d);
        if (b >= 120 || b <= 20) d = -d;
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
