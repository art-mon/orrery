#include "audio.h"
#include "pins.h"
#include "minimp3.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <driver/i2s_std.h>
#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "audio";

// Matches the server's briefing.mp3 (mono, MPEG-2 layer III @24 kHz). The
// tone generator is happy at any rate — 440 Hz sine oversamples fine here.
#define SAMPLE_RATE       24000
#define TONE_CHUNK        512
#define TONE_AMP_INT16    8000       // ~25% full scale — pleasant on 3W amp
#define URL_MAX           160
#define BRIEFING_MAX_MB   4          // sanity cap for the PSRAM buffer

typedef enum {
    MODE_SILENT   = 0,
    MODE_TONE     = 1,
    MODE_BRIEFING = 2,
} audio_mode_t;

static i2s_chan_handle_t s_tx = NULL;
static atomic_int         s_mode = MODE_SILENT;                // audio_mode_t
static atomic_int         s_freq = 0;
static atomic_int         s_briefing_state = AUDIO_BRIEFING_IDLE;
static atomic_int         s_briefing_stop  = 0;
static char               s_briefing_url[URL_MAX];

// minimp3 decoder + PCM staging. Only touched from the audio task; the
// decoder struct alone is ~6.7 KB, so it lives out of the task stack.
static mp3dec_t           s_dec;
static int16_t            s_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

static void write_silence_chunk(void) {
    static int16_t zeros[TONE_CHUNK];
    size_t w = 0;
    i2s_channel_write(s_tx, zeros, sizeof(zeros), &w, portMAX_DELAY);
}

static void write_tone_chunk(float *phase) {
    int16_t buf[TONE_CHUNK];
    int freq = atomic_load(&s_freq);
    if (freq <= 0) freq = 440;
    float inc = 2.0f * (float)M_PI * (float)freq / (float)SAMPLE_RATE;
    for (int i = 0; i < TONE_CHUNK; ++i) {
        buf[i] = (int16_t)(sinf(*phase) * TONE_AMP_INT16);
        *phase += inc;
        if (*phase >= 2.0f * (float)M_PI) *phase -= 2.0f * (float)M_PI;
    }
    size_t w = 0;
    i2s_channel_write(s_tx, buf, sizeof(buf), &w, portMAX_DELAY);
}

// Download the whole MP3 into a fresh PSRAM buffer. Returns the buffer via
// *out_buf on success (caller frees with heap_caps_free); NULL on failure
// or user stop.
static bool load_briefing_to_ram(const char *url, uint8_t **out_buf, size_t *out_len) {
    *out_buf = NULL;
    *out_len = 0;

    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    bool ok = false;
    uint8_t *buf = NULL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "briefing: http open: %s", esp_err_to_name(err));
        goto out;
    }
    int64_t clen  = esp_http_client_fetch_headers(client);
    int     status = esp_http_client_get_status_code(client);
    if (status != 200 || clen <= 0 || clen > (int64_t)BRIEFING_MAX_MB * 1024 * 1024) {
        ESP_LOGE(TAG, "briefing: bad response status=%d length=%lld", status, clen);
        goto out;
    }

    buf = (uint8_t *)heap_caps_malloc((size_t)clen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "briefing: alloc %lld bytes failed", clen);
        goto out;
    }

    size_t total = 0;
    while (total < (size_t)clen) {
        if (atomic_load(&s_briefing_stop)) { ESP_LOGI(TAG, "briefing: aborted during load"); goto out; }
        int r = esp_http_client_read(client, (char *)(buf + total),
                                     (int)((size_t)clen - total));
        if (r < 0) { ESP_LOGE(TAG, "briefing: http read err"); goto out; }
        if (r == 0) { ESP_LOGE(TAG, "briefing: short read at %u/%lld", (unsigned)total, clen); goto out; }
        total += (size_t)r;
    }
    ESP_LOGI(TAG, "briefing: loaded %u bytes into PSRAM", (unsigned)total);
    *out_buf = buf;
    *out_len = total;
    ok = true;

out:
    if (!ok && buf) heap_caps_free(buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok;
}

// Decode the in-RAM MP3 straight to I2S. Any stereo frames are downmixed
// in place so we always emit one channel per sample (matches our mono I2S
// slot config).
static void play_briefing_from_ram(const uint8_t *mp3, size_t len) {
    mp3dec_init(&s_dec);
    size_t pos     = 0;
    int    frames  = 0;
    int    logged_hz = 0;

    while (pos < len) {
        if (atomic_load(&s_briefing_stop)) { ESP_LOGI(TAG, "briefing: stop mid-play"); break; }

        mp3dec_frame_info_t info = { 0 };
        int samples = mp3dec_decode_frame(&s_dec, mp3 + pos, (int)(len - pos),
                                          s_pcm, &info);
        if (info.frame_bytes > 0) pos += (size_t)info.frame_bytes;
        else break;   // no more valid frames

        if (samples <= 0) continue;

        if (info.hz != logged_hz) {
            ESP_LOGI(TAG, "briefing: hz=%d channels=%d layer=%d br=%dkbps",
                     info.hz, info.channels, info.layer, info.bitrate_kbps);
            logged_hz = info.hz;
        }

        if (info.channels == 2) {
            for (int i = 0; i < samples; ++i) {
                int32_t mix = (int32_t)s_pcm[2*i] + (int32_t)s_pcm[2*i + 1];
                s_pcm[i] = (int16_t)(mix / 2);
            }
        }

        size_t written = 0;
        esp_err_t werr = i2s_channel_write(s_tx, s_pcm,
                                           (size_t)samples * sizeof(int16_t),
                                           &written, portMAX_DELAY);
        if (werr != ESP_OK) {
            ESP_LOGE(TAG, "briefing: i2s write: %s", esp_err_to_name(werr));
            break;
        }
        frames++;
    }
    ESP_LOGI(TAG, "briefing: playback done, %d frames", frames);
}

// Full lifecycle: LOADING → PLAYING → IDLE. Called on the audio task
// from the mode dispatcher.
static void run_briefing(void) {
    atomic_store(&s_briefing_state, AUDIO_BRIEFING_LOADING);
    // Flush the DMA with silence so nothing leftover loops during load.
    write_silence_chunk();

    uint8_t *buf = NULL;
    size_t   len = 0;
    if (load_briefing_to_ram(s_briefing_url, &buf, &len)) {
        if (!atomic_load(&s_briefing_stop)) {
            atomic_store(&s_briefing_state, AUDIO_BRIEFING_PLAYING);
            play_briefing_from_ram(buf, len);
        }
        heap_caps_free(buf);
    }

    atomic_store(&s_briefing_stop, 0);
    atomic_store(&s_briefing_state, AUDIO_BRIEFING_IDLE);
    atomic_store(&s_mode, MODE_SILENT);
}

static void audio_task(void *arg) {
    (void)arg;
    float tone_phase = 0.0f;
    while (true) {
        audio_mode_t mode = (audio_mode_t)atomic_load(&s_mode);
        switch (mode) {
            case MODE_TONE:
                write_tone_chunk(&tone_phase);
                break;
            case MODE_BRIEFING:
                tone_phase = 0.0f;
                run_briefing();
                break;
            case MODE_SILENT:
            default:
                tone_phase = 0.0f;
                write_silence_chunk();
                break;
        }
    }
}

void audio_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = PIN_I2S_DATA,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0, 0, 0 },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    ESP_LOGI(TAG, "i2s std TX ready — bclk=%d ws=%d dout=%d @%d Hz",
             PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DATA, SAMPLE_RATE);

    // 24 KB stack for minimp3's worst-case IMDCT synth. Pinned to CPU1
    // (APP CPU) so WiFi and the panel refresh (both on CPU0) never wait
    // on the decoder, and the decoder never waits on beacon-time IRQs.
    xTaskCreatePinnedToCore(audio_task, "audio", 24576, NULL, 5, NULL, 1);
}

void audio_tone_start(int freq_hz) {
    ESP_LOGI(TAG, "tone start %d Hz", freq_hz);
    atomic_store(&s_freq, freq_hz);
    atomic_store(&s_mode, MODE_TONE);
}
void audio_tone_stop(void) {
    ESP_LOGI(TAG, "tone stop");
    atomic_store(&s_mode, MODE_SILENT);
}

void audio_briefing_start(const char *url) {
    if (!url || atomic_load(&s_briefing_state) != AUDIO_BRIEFING_IDLE) return;
    strlcpy(s_briefing_url, url, sizeof(s_briefing_url));
    atomic_store(&s_briefing_stop, 0);
    atomic_store(&s_briefing_state, AUDIO_BRIEFING_LOADING);
    atomic_store(&s_mode, MODE_BRIEFING);
    ESP_LOGI(TAG, "briefing start");
}

void audio_briefing_stop(void) {
    if (atomic_load(&s_briefing_state) == AUDIO_BRIEFING_IDLE) return;
    ESP_LOGI(TAG, "briefing stop request");
    atomic_store(&s_briefing_stop, 1);
}

audio_briefing_state_t audio_briefing_state(void) {
    return (audio_briefing_state_t)atomic_load(&s_briefing_state);
}

bool audio_briefing_active(void) {
    return audio_briefing_state() != AUDIO_BRIEFING_IDLE;
}
