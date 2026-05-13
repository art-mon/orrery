#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void panel_init(void);
void panel_clear(void);
void panel_set_brightness(uint8_t b);
void panel_draw_test_pattern(void);

#ifdef __cplusplus
}
#endif
