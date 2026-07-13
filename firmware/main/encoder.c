#include "encoder.h"
#include "pins.h"

#include <stdatomic.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Quadrature state-machine decoder (Buxton table). Robust to contact bounce:
// only emits after a full valid CW/CCW sequence, and invalid transitions
// (bounces) push the state back to REST without emitting a click.
//
// A dedicated task polls the pins fast enough to catch every intermediate
// quadrature phase (the scene loop's 33 ms cadence is too slow — a whole
// detent can finish in ~100 ms with only 3 samples, missing states).
// Detents are counted into an atomic accumulator that the scene loop drains.

#define R_REST       0x0
#define R_CW_1       0x1
#define R_CW_2       0x2
#define R_CW_3       0x3
#define R_CCW_1      0x4
#define R_CCW_2      0x5
#define R_CCW_3      0x6

#define DIR_CW       0x10
#define DIR_CCW      0x20

// Rows: current state (masked with 0x0F).
// Cols: pin sample = (DT << 1) | CLK (0..3).
// Value: next state, optionally OR'd with DIR_CW / DIR_CCW when a detent
// completes at REST.
static const uint8_t TT[7][4] = {
    /* R_REST  */ { R_REST,   R_CW_1,   R_CCW_1,  R_REST },
    /* R_CW_1  */ { R_CW_2,   R_CW_1,   R_REST,   R_REST },
    /* R_CW_2  */ { R_CW_2,   R_CW_1,   R_CW_3,   R_REST },
    /* R_CW_3  */ { R_CW_2,   R_REST,   R_CW_3,   R_REST | DIR_CW  },
    /* R_CCW_1 */ { R_CCW_2,  R_REST,   R_CCW_1,  R_REST },
    /* R_CCW_2 */ { R_CCW_2,  R_CCW_3,  R_CCW_1,  R_REST },
    /* R_CCW_3 */ { R_CCW_2,  R_CCW_3,  R_REST,   R_REST | DIR_CCW },
};

static atomic_int s_accum = 0;

static void encoder_task(void *arg) {
    (void)arg;
    uint8_t state = R_REST;
    while (true) {
        uint8_t clk = gpio_get_level(PIN_ENC_CLK);
        uint8_t dt  = gpio_get_level(PIN_ENC_DT);
        uint8_t pins = (uint8_t)((dt << 1) | clk);
        state = TT[state & 0x0F][pins];
        if (state & DIR_CW)       atomic_fetch_add(&s_accum, +1);
        else if (state & DIR_CCW) atomic_fetch_add(&s_accum, -1);
        vTaskDelay(1);  // 1 tick — at CONFIG_FREERTOS_HZ=100 that's 10 ms
    }
}

void encoder_init(void) {
    gpio_config_t io = {
        .pin_bit_mask   = (1ULL << PIN_ENC_CLK) | (1ULL << PIN_ENC_DT),
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    atomic_store(&s_accum, 0);
    xTaskCreate(encoder_task, "encoder", 2048, NULL, 5, NULL);
}

int encoder_read_delta(void) {
    return atomic_exchange(&s_accum, 0);
}
