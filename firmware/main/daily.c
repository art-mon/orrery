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

// Strip non-printable / non-ASCII bytes. Our 5x7 font only covers 0x20..0x7E,
// and the JSON occasionally has UTF-8 (e.g. M-dashes in APOD titles). Replace
// out-of-range bytes with '?'.
static void sanitize_ascii(char *s) {
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c > 0x7E) *s = '?';
    }
}

static void copy_str(char *dst, size_t cap, const cJSON *node) {
    if (cJSON_IsString(node) && node->valuestring) {
        strlcpy(dst, node->valuestring, cap);
        sanitize_ascii(dst);
    } else {
        dst[0] = '\0';
    }
}

bool daily_fetch(daily_data_t *out) {
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

    // Weather
    cJSON *w = cJSON_GetObjectItemCaseSensitive(root, "weather");
    if (cJSON_IsObject(w) && !cJSON_GetObjectItemCaseSensitive(w, "error")) {
        copy_str(out->city,      sizeof(out->city),      cJSON_GetObjectItemCaseSensitive(w, "city"));
        copy_str(out->condition, sizeof(out->condition), cJSON_GetObjectItemCaseSensitive(w, "condition"));
        cJSON *t  = cJSON_GetObjectItemCaseSensitive(w, "temp_c");
        cJSON *fl = cJSON_GetObjectItemCaseSensitive(w, "feels_c");
        cJSON *h  = cJSON_GetObjectItemCaseSensitive(w, "humidity");
        if (cJSON_IsNumber(t))  out->temp_c   = (float)t->valuedouble;
        if (cJSON_IsNumber(fl)) out->feels_c  = (float)fl->valuedouble;
        if (cJSON_IsNumber(h))  out->humidity = h->valueint;
        out->has_weather = out->city[0] != '\0';
    }

    // Asteroids — server sorts ascending by approach date then miss distance,
    // so index 0 is the closest.
    cJSON *a = cJSON_GetObjectItemCaseSensitive(root, "asteroids");
    cJSON *list = cJSON_GetObjectItemCaseSensitive(a, "asteroids");
    if (cJSON_IsArray(list)) {
        int n = cJSON_GetArraySize(list);
        out->asteroid_count = n;
        if (n > 0) {
            cJSON *first = cJSON_GetArrayItem(list, 0);
            copy_str(out->closest_name, sizeof(out->closest_name),
                     cJSON_GetObjectItemCaseSensitive(first, "name"));
            cJSON *d  = cJSON_GetObjectItemCaseSensitive(first, "diameter_m");
            cJSON *md = cJSON_GetObjectItemCaseSensitive(first, "miss_distance_km");
            if (cJSON_IsNumber(d))  out->closest_diameter_m = d->valueint;
            if (cJSON_IsNumber(md)) out->closest_miss_km    = (long)md->valuedouble;
            out->has_asteroids = out->closest_name[0] != '\0';
        }
    }

    // APOD
    cJSON *ap = cJSON_GetObjectItemCaseSensitive(root, "apod");
    if (cJSON_IsObject(ap) && !cJSON_GetObjectItemCaseSensitive(ap, "error")) {
        copy_str(out->apod_title, sizeof(out->apod_title),
                 cJSON_GetObjectItemCaseSensitive(ap, "title"));
        out->has_apod = out->apod_title[0] != '\0';
    }

    cJSON_Delete(root);
    return out->has_weather || out->has_asteroids || out->has_apod;
}
