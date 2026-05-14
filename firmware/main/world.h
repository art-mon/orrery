#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fetch + base64-decode world_mask.json (256 B packed bits, one bit per
// pixel: 1 = land) and world_clouds.json (2048 B grayscale density,
// one byte per pixel). Both are cached in static buffers. Returns true
// if at least one resource was loaded successfully.
bool world_fetch(void);

bool world_is_land(int x, int y);

// 0..255 cloud density at pixel (x, y). 0 if clouds not loaded.
uint8_t world_cloud(int x, int y);

bool world_mask_loaded(void);
bool world_clouds_loaded(void);

// Earth sprite texture: 64×64 RGB888 (12288 bytes). Returns NULL if not loaded.
const uint8_t *world_earth_tex(int *size_out);

#ifdef __cplusplus
}
#endif
