#include "encoder.h"
#include "pins.h"

#include <driver/gpio.h>

// KY-040 detent = one full CLK cycle. Sample CLK each frame; on a falling
// edge, DT tells us direction (HIGH = CW, LOW = CCW).
static int s_last_clk = 1;

void encoder_init(void) {
    gpio_config_t io = {
        .pin_bit_mask   = (1ULL << PIN_ENC_CLK) | (1ULL << PIN_ENC_DT),
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    s_last_clk = gpio_get_level(PIN_ENC_CLK);
}

int encoder_read_delta(void) {
    int clk = gpio_get_level(PIN_ENC_CLK);
    int delta = 0;
    if (s_last_clk == 1 && clk == 0) {
        delta = gpio_get_level(PIN_ENC_DT) ? +1 : -1;
    }
    s_last_clk = clk;
    return delta;
}
