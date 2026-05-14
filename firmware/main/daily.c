#include "daily.h"
#include "wifi_creds.h"

#include <string.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>

static const char *TAG = "daily";

#define MAX_BODY_BYTES 16384

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} body_t;

static esp_err_t http_event(esp_http_client_event_t *evt) {
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

static void copy_str(char *dst, size_t cap, const cJSON *node) {
    if (cJSON_IsString(node) && node->valuestring) {
        strlcpy(dst, node->valuestring, cap);
    } else {
        dst[0] = '\0';
    }
}

bool daily_fetch(daily_weather_t *out) {
    memset(out, 0, sizeof(*out));

    body_t body = { .buf = malloc(MAX_BODY_BYTES), .len = 0, .cap = MAX_BODY_BYTES };
    if (!body.buf) return false;
    body.buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = DAILY_JSON_URL,
        .event_handler = http_event,
        .user_data = &body,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "fetch failed: err=%s status=%d", esp_err_to_name(err), status);
        free(body.buf);
        return false;
    }
    ESP_LOGI(TAG, "got %u bytes", (unsigned)body.len);

    cJSON *root = cJSON_Parse(body.buf);
    free(body.buf);
    if (!root) {
        ESP_LOGE(TAG, "json parse failed");
        return false;
    }

    cJSON *w = cJSON_GetObjectItemCaseSensitive(root, "weather");
    if (cJSON_IsObject(w)) {
        copy_str(out->city,      sizeof(out->city),      cJSON_GetObjectItemCaseSensitive(w, "city"));
        copy_str(out->condition, sizeof(out->condition), cJSON_GetObjectItemCaseSensitive(w, "condition"));
        cJSON *t  = cJSON_GetObjectItemCaseSensitive(w, "temp_c");
        cJSON *fl = cJSON_GetObjectItemCaseSensitive(w, "feels_c");
        cJSON *h  = cJSON_GetObjectItemCaseSensitive(w, "humidity");
        if (cJSON_IsNumber(t))  out->temp_c   = (float)t->valuedouble;
        if (cJSON_IsNumber(fl)) out->feels_c  = (float)fl->valuedouble;
        if (cJSON_IsNumber(h))  out->humidity = h->valueint;
        out->valid = out->city[0] != '\0';
    }

    cJSON_Delete(root);
    return out->valid;
}
