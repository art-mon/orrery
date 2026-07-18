#include "ota.h"
#include "wifi_creds.h"

#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_app_desc.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <cJSON.h>

static const char *TAG = "ota";

// Manifest is tiny — currently ~5 fields — but leave headroom for release
// notes and future fields.
#define MANIFEST_MAX_BYTES 4096

// Delay before the first OTA check after boot. Gives wifi + the initial
// data fetch time to settle, and lets the user notice a bad update and
// power-cycle before we overwrite the other slot.
// TEST CADENCE — 60s so a fresh flash validates end-to-end within minutes.
// Revert to a few minutes before shipping.
#define OTA_FIRST_CHECK_DELAY_MS (60u * 1000u)

// Cadence between checks once the first one runs. Design target is a
// scheduled 02:00-local check; for now a rolling timer is enough and
// doesn't depend on the RTC being set.
// TEST CADENCE — 30 min so a release lands on the bench in the same
// session. Bump back to 24h (or wire the 02:00-local schedule) before
// shipping.
#define OTA_CHECK_INTERVAL_MS (30u * 60u * 1000u)

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} body_t;

static esp_err_t manifest_http_event(esp_http_client_event_t *evt) {
    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    body_t *b = (body_t *)evt->user_data;
    size_t remaining = b->cap - b->len - 1;
    if (remaining == 0) return ESP_OK;
    size_t n = evt->data_len < remaining ? evt->data_len : remaining;
    memcpy(b->buf + b->len, evt->data, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return ESP_OK;
}

// Parse "MAJOR.MINOR.PATCH" into a 3-element array. Missing components
// are treated as 0 ("1.2" == "1.2.0"). Any non-digit byte after the last
// parsed field ends parsing — so "1.2.0-rc1" reads as 1.2.0. Returns true
// if at least MAJOR parsed as a number.
static bool parse_semver(const char *s, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!s || !*s) return false;
    int idx = 0;
    const char *p = s;
    while (*p && idx < 3) {
        if (*p < '0' || *p > '9') break;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) break;
        out[idx++] = (int)v;
        p = end;
        if (*p != '.') break;
        p++;
    }
    return idx > 0;
}

// Returns >0 if `a` is newer than `b`, 0 if equal, <0 if older.
static int semver_cmp(const int a[3], const int b[3]) {
    for (int i = 0; i < 3; ++i) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

// Fetch and parse the manifest. On success writes the manifest version
// and download URL into the provided buffers and returns true.
static bool fetch_manifest(char *ver_out, size_t ver_cap,
                           char *url_out, size_t url_cap) {
    body_t body = { .buf = malloc(MANIFEST_MAX_BYTES), .len = 0, .cap = MANIFEST_MAX_BYTES };
    if (!body.buf) return false;
    body.buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = VERSION_JSON_URL,
        .event_handler = manifest_http_event,
        .user_data = &body,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "manifest fetch failed: err=%s status=%d",
                 esp_err_to_name(err), status);
        free(body.buf);
        return false;
    }

    cJSON *root = cJSON_Parse(body.buf);
    free(body.buf);
    if (!root) {
        ESP_LOGW(TAG, "manifest json parse failed");
        return false;
    }

    bool ok = false;
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *u = cJSON_GetObjectItemCaseSensitive(root, "url");
    if (cJSON_IsString(v) && v->valuestring &&
        cJSON_IsString(u) && u->valuestring) {
        strlcpy(ver_out, v->valuestring, ver_cap);
        strlcpy(url_out, u->valuestring, url_cap);
        ok = true;
    } else {
        ESP_LOGW(TAG, "manifest missing version/url");
    }
    cJSON_Delete(root);
    return ok;
}

bool ota_check_and_apply(void) {
    const esp_app_desc_t *cur = esp_app_get_description();
    ESP_LOGI(TAG, "running version: %s", cur->version);

    char remote_ver[32] = {0};
    char remote_url[256] = {0};
    if (!fetch_manifest(remote_ver, sizeof(remote_ver),
                        remote_url, sizeof(remote_url))) {
        return false;
    }
    ESP_LOGI(TAG, "manifest version: %s url: %s", remote_ver, remote_url);

    int cur_sv[3], rem_sv[3];
    if (!parse_semver(cur->version, cur_sv) ||
        !parse_semver(remote_ver, rem_sv)) {
        ESP_LOGW(TAG, "unparseable version string, skipping");
        return false;
    }
    if (semver_cmp(rem_sv, cur_sv) <= 0) {
        ESP_LOGI(TAG, "up to date");
        return false;
    }

    ESP_LOGI(TAG, "newer version available — downloading");
    esp_http_client_config_t http_cfg = {
        .url = remote_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "OTA success — rebooting into new slot");
    vTaskDelay(pdMS_TO_TICKS(500));  // let the log flush
    esp_restart();
    return true;  // unreachable
}

void ota_mark_running_valid(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return;

    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "marked running slot valid (rollback cancelled)");
        } else {
            ESP_LOGW(TAG, "mark valid failed: %s", esp_err_to_name(err));
        }
    }
}

static void ota_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(OTA_FIRST_CHECK_DELAY_MS));
    for (;;) {
        ota_check_and_apply();  // reboots on success; otherwise falls through
        vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS));
    }
}

void ota_start(void) {
    xTaskCreate(ota_task, "ota", 8192, NULL, 3, NULL);
}
