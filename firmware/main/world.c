#include "world.h"
#include "pins.h"

#include <string.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <mbedtls/base64.h>

static const char *TAG = "world";

// daily.json lives at the same prefix as world_mask / world_clouds.
// Derive the dir by reusing the same host config defined in wifi_creds.h.
#include "wifi_creds.h"

// Strip the file name off DAILY_JSON_URL to get the base directory URL.
// e.g. "https://host/path/data/daily.json" -> "https://host/path/data/"
static void build_url(char *out, size_t cap, const char *file) {
    const char *u = DAILY_JSON_URL;
    size_t n = strlen(u);
    while (n > 0 && u[n - 1] != '/') --n;
    snprintf(out, cap, "%.*s%s", (int)n, u, file);
}

static uint8_t s_mask[256];          // 64*32 / 8 packed land bits
static bool    s_mask_ok = false;

static uint8_t s_clouds[64 * 32];    // grayscale density
static bool    s_clouds_ok = false;

// Earth sprite texture — assumed 64x64 RGB888
#define EARTH_TEX_SIZE 64
static uint8_t s_earth[EARTH_TEX_SIZE * EARTH_TEX_SIZE * 3];
static bool    s_earth_ok = false;

const uint8_t *world_earth_tex(int *size_out) {
    if (!s_earth_ok) return NULL;
    if (size_out) *size_out = EARTH_TEX_SIZE;
    return s_earth;
}

bool world_mask_loaded(void)   { return s_mask_ok; }
bool world_clouds_loaded(void) { return s_clouds_ok; }

bool world_is_land(int x, int y) {
    if (!s_mask_ok) return false;
    if (x < 0 || x >= 64 || y < 0 || y >= 32) return false;
    int bit_idx = y * 64 + x;
    uint8_t byte = s_mask[bit_idx >> 3];
    int bit = 7 - (bit_idx & 7);
    return (byte & (1u << bit)) != 0;
}

uint8_t world_cloud(int x, int y) {
    if (!s_clouds_ok) return 0;
    if (x < 0 || x >= 64 || y < 0 || y >= 32) return 0;
    return s_clouds[y * 64 + x];
}

// HTTP fetch into a heap buffer; caller frees.
static char *http_get_text(const char *url, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    typedef struct { char *buf; size_t *len; size_t cap; } ctx_t;
    ctx_t c = { .buf = buf, .len = &len, .cap = cap };

    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .user_data = &c,
        .event_handler = NULL,   // we'll read body with esp_http_client_read
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) goto fail;
    int cl = esp_http_client_fetch_headers(client);
    (void)cl;
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

static bool load_mask(void) {
    char url[256];
    build_url(url, sizeof(url), "world_mask.json");
    size_t blen = 0;
    char  *body = http_get_text(url, &blen);
    if (!body) { ESP_LOGE(TAG, "mask fetch failed"); return false; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return false;
    cJSON *b64 = cJSON_GetObjectItemCaseSensitive(root, "land_b64");
    bool ok = false;
    if (cJSON_IsString(b64) && b64->valuestring) {
        size_t olen = 0;
        int rc = mbedtls_base64_decode(s_mask, sizeof(s_mask), &olen,
                                       (const unsigned char *)b64->valuestring,
                                       strlen(b64->valuestring));
        ok = (rc == 0 && olen == sizeof(s_mask));
    }
    cJSON_Delete(root);
    if (ok) { s_mask_ok = true; ESP_LOGI(TAG, "mask loaded"); }
    return ok;
}

static bool load_clouds(void) {
    char url[256];
    build_url(url, sizeof(url), "world_clouds.json");
    size_t blen = 0;
    char  *body = http_get_text(url, &blen);
    if (!body) { ESP_LOGW(TAG, "clouds fetch failed"); return false; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return false;
    cJSON *b64 = cJSON_GetObjectItemCaseSensitive(root, "alpha_b64");
    if (!cJSON_IsString(b64) || !b64->valuestring)
        b64 = cJSON_GetObjectItemCaseSensitive(root, "clouds_b64");
    bool ok = false;
    if (cJSON_IsString(b64) && b64->valuestring) {
        size_t olen = 0;
        int rc = mbedtls_base64_decode(s_clouds, sizeof(s_clouds), &olen,
                                       (const unsigned char *)b64->valuestring,
                                       strlen(b64->valuestring));
        ok = (rc == 0 && olen == sizeof(s_clouds));
    }
    cJSON_Delete(root);
    if (ok) { s_clouds_ok = true; ESP_LOGI(TAG, "clouds loaded"); }
    return ok;
}

static bool load_earth(void) {
    char url[256];
    build_url(url, sizeof(url), "earth_texture.json");
    size_t blen = 0;
    char  *body = http_get_text(url, &blen);
    if (!body) { ESP_LOGW(TAG, "earth fetch failed"); return false; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return false;
    cJSON *b64 = cJSON_GetObjectItemCaseSensitive(root, "rgb_b64");
    bool ok = false;
    if (cJSON_IsString(b64) && b64->valuestring) {
        size_t olen = 0;
        int rc = mbedtls_base64_decode(s_earth, sizeof(s_earth), &olen,
                                       (const unsigned char *)b64->valuestring,
                                       strlen(b64->valuestring));
        ok = (rc == 0 && olen == sizeof(s_earth));
    }
    cJSON_Delete(root);
    if (ok) { s_earth_ok = true; ESP_LOGI(TAG, "earth tex loaded"); }
    return ok;
}

bool world_fetch(void) {
    bool a = load_mask();
    bool b = load_clouds();
    bool c = load_earth();
    return a || b || c;
}
