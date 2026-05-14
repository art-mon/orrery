#pragma once

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start SNTP and set the TZ environment variable. Non-blocking.
// `tz` is a POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3").
void clock_start(const char *tz);

// True once SNTP has synced at least once.
bool clock_synced(void);

// Returns local time in *tm_out. Returns true if synced; if not yet synced,
// fills tm_out with epoch (1970-01-01) but still returns false.
bool clock_now(struct tm *tm_out);

#ifdef __cplusplus
}
#endif
