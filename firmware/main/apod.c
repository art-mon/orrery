#include "apod.h"
#include "pins.h"
#include "wifi_creds.h"

#include <stdlib.h>
#include <string.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <mbedtls/base64.h>

static const char *TAG = "apod";

#define APOD_BYTES (PANEL_W * PANEL_H * 3)

// Roomy cap — v1 QR is 21×21, but leave headroom in case the encoded URL
// later needs a bigger version.
#define QR_MAX_SIZE  40
#define QR_MAX_BYTES ((QR_MAX_SIZE * QR_MAX_SIZE + 7) / 8)

static uint8_t s_pixels[APOD_BYTES];
static bool    s_loaded = false;

static uint8_t s_qr_bits[QR_MAX_BYTES];
static int     s_qr_size    = 0;
static bool    s_qr_loaded  = false;

bool           apod_loaded(void)     { return s_loaded; }
const uint8_t *apod_pixels(void)     { return s_loaded ? s_pixels : NULL; }
bool           apod_qr_loaded(void)  { return s_qr_loaded; }
int            apod_qr_size(void)    { return s_qr_size; }

bool apod_qr_module(int x, int y) {
    if (!s_qr_loaded) return false;
    if (x < 0 || y < 0 || x >= s_qr_size || y >= s_qr_size) return false;
    int idx = y * s_qr_size + x;
    return (s_qr_bits[idx >> 3] & (1u << (7 - (idx & 7)))) != 0;
}

// Strip the file name off DAILY_JSON_URL to derive the data dir.
// e.g. "https://host/path/data/daily.json" -> "https://host/path/data/"
static void build_url(char *out, size_t cap, const char *file) {
    const char *u = DAILY_JSON_URL;
    size_t n = strlen(u);
    while (n > 0 && u[n - 1] != '/') --n;
    snprintf(out, cap, "%.*s%s", (int)n, u, file);
}

// Grow-buffer HTTP GET into heap; caller frees.
static char *http_get_text(const char *url, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) goto fail;
    esp_http_client_fetch_headers(client);
    while (true) {
        if (len + 1024 > cap) {
            size_t ncap = cap * 2;
            char *n = realloc(buf, ncap);
            if (!n) goto fail;
            buf = n; cap = ncap;
        }
        int r = esp_http_client_read(client, buf + len, cap - len - 1);
        if (r <= 0) break;
        len += (size_t)r;
        buf[len] = '\0';
    }
    int status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (status != 200) goto fail2;
    *out_len = len;
    return buf;
fail:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
fail2:
    free(buf);
    return NULL;
}

bool apod_fetch(void) {
    char url[256];
    build_url(url, sizeof(url), "frame_b64.json");
    size_t blen = 0;
    char  *body = http_get_text(url, &blen);
    if (!body) { ESP_LOGW(TAG, "fetch failed"); return false; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) { ESP_LOGE(TAG, "json parse failed"); return false; }

    cJSON *b64 = cJSON_GetObjectItemCaseSensitive(root, "rgb_b64");
    bool ok = false;
    if (cJSON_IsString(b64) && b64->valuestring) {
        size_t olen = 0;
        int rc = mbedtls_base64_decode(s_pixels, sizeof(s_pixels), &olen,
                                       (const unsigned char *)b64->valuestring,
                                       strlen(b64->valuestring));
        ok = (rc == 0 && olen == sizeof(s_pixels));
    }
    cJSON_Delete(root);
    if (ok) { s_loaded = true; ESP_LOGI(TAG, "frame loaded (%d bytes)", APOD_BYTES); }
    return ok;
}

bool apod_qr_fetch(void) {
    char url[256];
    build_url(url, sizeof(url), "apod_qr.json");
    size_t blen = 0;
    char  *body = http_get_text(url, &blen);
    if (!body) { ESP_LOGW(TAG, "qr fetch failed"); return false; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) { ESP_LOGE(TAG, "qr json parse failed"); return false; }

    cJSON *sz  = cJSON_GetObjectItemCaseSensitive(root, "size");
    cJSON *b64 = cJSON_GetObjectItemCaseSensitive(root, "modules_b64");
    bool ok = false;
    if (cJSON_IsNumber(sz) && cJSON_IsString(b64) && b64->valuestring) {
        int n = sz->valueint;
        if (n > 0 && n <= QR_MAX_SIZE) {
            size_t need = (size_t)((n * n + 7) / 8);
            size_t olen = 0;
            int rc = mbedtls_base64_decode(s_qr_bits, sizeof(s_qr_bits), &olen,
                                           (const unsigned char *)b64->valuestring,
                                           strlen(b64->valuestring));
            if (rc == 0 && olen == need) {
                s_qr_size = n;
                ok = true;
            }
        }
    }
    cJSON_Delete(root);
    if (ok) { s_qr_loaded = true; ESP_LOGI(TAG, "qr loaded (%d modules)", s_qr_size); }
    return ok;
}
