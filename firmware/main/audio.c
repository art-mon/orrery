#include "audio.h"
#include "pins.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <driver/i2s_std.h>
#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "audio";

#define SAMPLE_RATE   16000
#define CHUNK_SAMPLES 512
#define AMP_INT16     8000     // ~25% full scale — pleasant on the 3W amp

static i2s_chan_handle_t s_tx = NULL;
static atomic_int         s_freq = 0;

// Continuous writer: blocks on i2s_channel_write, which self-paces to the
// sample rate. When freq is 0 we push silence so DMA never underruns.
static void audio_task(void *arg) {
    (void)arg;
    static int16_t buf[CHUNK_SAMPLES];
    float phase = 0.0f;
    while (true) {
        int freq = atomic_load(&s_freq);
        if (freq > 0) {
            float inc = 2.0f * (float)M_PI * (float)freq / (float)SAMPLE_RATE;
            for (int i = 0; i < CHUNK_SAMPLES; ++i) {
                buf[i] = (int16_t)(sinf(phase) * AMP_INT16);
                phase += inc;
                if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
            }
        } else {
            memset(buf, 0, sizeof(buf));
            phase = 0.0f;
        }
        size_t written = 0;
        esp_err_t err = i2s_channel_write(s_tx, buf, sizeof(buf), &written, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s_channel_write failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
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

    xTaskCreate(audio_task, "audio", 4096, NULL, 5, NULL);
}

void audio_tone_start(int freq_hz) {
    ESP_LOGI(TAG, "tone start %d Hz", freq_hz);
    atomic_store(&s_freq, freq_hz);
}
void audio_tone_stop(void) {
    ESP_LOGI(TAG, "tone stop");
    atomic_store(&s_freq, 0);
}
