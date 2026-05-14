#include "clock.h"

#include <stdlib.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>

static const char *TAG = "clock";
static bool s_synced = false;

static void on_sync(struct timeval *tv) {
    (void)tv;
    if (!s_synced) {
        s_synced = true;
        ESP_LOGI(TAG, "time synced");
    }
}

void clock_start(const char *tz) {
    setenv("TZ", tz, 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = on_sync;
    esp_netif_sntp_init(&cfg);
}

bool clock_synced(void) {
    return s_synced;
}

bool clock_now(struct tm *tm_out) {
    time_t now = 0;
    time(&now);
    localtime_r(&now, tm_out);
    return s_synced;
}
