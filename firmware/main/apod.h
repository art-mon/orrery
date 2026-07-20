#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fetch + base64-decode frame_b64.json (6144 B raw RGB888, one triple per
// panel pixel row-major). Cached in a static buffer. Returns true on success.
bool apod_fetch(void);

// True if a frame has been loaded at least once.
bool apod_loaded(void);

// Row-major RGB888 buffer: PANEL_W * PANEL_H * 3 bytes. NULL until loaded.
const uint8_t *apod_pixels(void);

// Fetch + decode apod_qr.json (bit-packed row-major QR modules).
bool  apod_qr_fetch(void);
bool  apod_qr_loaded(void);
int   apod_qr_size(void);                 // 0 until loaded; typically 21 (v1)
bool  apod_qr_module(int x, int y);       // true = dark module

#ifdef __cplusplus
}
#endif
