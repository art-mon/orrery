#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ambient-light-driven auto-brightness. Reads the BH1750 on I2C every 200 ms,
// maps lux → [FLOOR, 1.0] on a log curve, low-passes at τ = 3 s, and writes
// panel_set_brightness(round(max * factor)). Never exceeds `max`.
void als_start(uint8_t max_brightness);

#ifdef __cplusplus
}
#endif
