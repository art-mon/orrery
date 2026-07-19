#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ambient-light-driven auto-brightness. Reads the BH1750 on I2C every 200 ms,
// maps lux → [FLOOR, 1.0] on a log curve, low-passes at τ = 3 s, and writes
// panel_set_brightness(round(max * factor)). Never exceeds `max`.
void als_start(uint8_t max_brightness);

// Most recent raw lux reading from the sensor. Returns NAN before the first
// successful read (sensor missing, I²C error, or task not yet ticked past
// the first BH1750 integration window ~180 ms after als_start).
float als_get_lux(void);

// Low-pass-filtered brightness factor in [FACTOR_FLOOR, 1.0] — what the
// ALS task is currently driving the panel to. Returns 0 if not seeded yet.
float als_get_factor(void);

#ifdef __cplusplus
}
#endif
