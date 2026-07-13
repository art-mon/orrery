#include "als.h"
#include "pins.h"
#include "panel.h"

#include <math.h>
#include <stdint.h>

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "als";

// BH1750 command bytes and timing (see Rohm datasheet, rev.D).
#define BH1750_ADDR              0x23
#define BH1750_POWER_ON          0x01
#define BH1750_ONE_SHOT_HIRES    0x20   // 1 lux resolution, ~180 ms max
#define BH1750_MEAS_WAIT_MS      180

// Sample cadence and filter.
#define SAMPLE_PERIOD_MS         200
#define TAU_S                    3.0f   // 1-pole LPF time constant
// α = dt / (τ + dt) — computed once at init.

// Log-linear lux → factor map.
// Below LUX_LO the brightness sits at the floor; above LUX_HI it's full max.
#define LUX_LO                   5.0f
#define LUX_HI                   200.0f
#define FACTOR_FLOOR             0.2f   // ≈ 8/40 at the current max
#define FACTOR_CEIL              1.0f

static i2c_master_bus_handle_t s_bus  = NULL;
static i2c_master_dev_handle_t s_dev  = NULL;

static esp_err_t als_trigger_measurement(void) {
    const uint8_t cmd = BH1750_ONE_SHOT_HIRES;
    return i2c_master_transmit(s_dev, &cmd, 1, 100);
}

static esp_err_t als_read_lux(float *lux_out) {
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_master_receive(s_dev, buf, sizeof(buf), 100);
    if (err != ESP_OK) return err;
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *lux_out = (float)raw / 1.2f;
    return ESP_OK;
}

static float lux_to_factor(float lux) {
    if (lux <= LUX_LO) return FACTOR_FLOOR;
    if (lux >= LUX_HI) return FACTOR_CEIL;
    // Interpolate on log10 so the curve matches perceived brightness.
    float t = (log10f(lux) - log10f(LUX_LO)) / (log10f(LUX_HI) - log10f(LUX_LO));
    return FACTOR_FLOOR + (FACTOR_CEIL - FACTOR_FLOOR) * t;
}

static void als_task(void *arg) {
    const uint8_t max_brightness = (uint8_t)(uintptr_t)arg;

    // α computed from the actual loop period (τ = 3 s, dt = 200 ms → α ≈ 0.0625).
    const float alpha = (float)SAMPLE_PERIOD_MS / 1000.0f
                     / (TAU_S + (float)SAMPLE_PERIOD_MS / 1000.0f);

    // Seed the filter with the first good sample so we don't ramp from an
    // arbitrary initial value at boot.
    float filtered = -1.0f;
    int log_countdown = 0;

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        esp_err_t err = als_trigger_measurement();
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(BH1750_MEAS_WAIT_MS));
            float lux = 0.0f;
            err = als_read_lux(&lux);
            if (err == ESP_OK) {
                float target = lux_to_factor(lux);
                if (filtered < 0.0f) filtered = target;
                else                 filtered += alpha * (target - filtered);

                int b = (int)lroundf((float)max_brightness * filtered);
                if (b < 1) b = 1;
                if (b > max_brightness) b = max_brightness;
                panel_set_brightness((uint8_t)b);

                if (--log_countdown <= 0) {
                    ESP_LOGI(TAG, "lux=%.1f target=%.2f filt=%.2f b=%d/%u",
                             lux, target, filtered, b, max_brightness);
                    log_countdown = 5;  // once per second
                }
            }
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "BH1750 I/O err 0x%x — holding last brightness", err);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

void als_start(uint8_t max_brightness) {
    i2c_master_bus_config_t bus_cfg = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = PIN_I2C_SDA,
        .scl_io_num                   = PIN_I2C_SCL,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BH1750_ADDR,
        .scl_speed_hz    = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    // Power-on ping; harmless if the sensor is absent (task will log I/O errors).
    const uint8_t power_on = BH1750_POWER_ON;
    esp_err_t err = i2c_master_transmit(s_dev, &power_on, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BH1750 power-on failed 0x%x — check wiring", err);
    } else {
        ESP_LOGI(TAG, "BH1750 online @ 0x%02x — max brightness cap = %u",
                 BH1750_ADDR, max_brightness);
    }

    xTaskCreate(als_task, "als", 3072, (void *)(uintptr_t)max_brightness, 3, NULL);
}
