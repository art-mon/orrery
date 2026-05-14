#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Blocks until connected or until timeout_ms elapses. Returns true on success.
bool wifi_start_and_wait(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
