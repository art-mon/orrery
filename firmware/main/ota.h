#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fetch VERSION_JSON_URL, compare its "version" against the running app,
// and — if newer — download the binary at "url" into the inactive OTA slot
// and reboot. Blocks; safe to call from a low-priority background task.
// Returns true only when an update was applied (i.e. it did not return —
// the device rebooted); returns false on "up to date" or any failure.
bool ota_check_and_apply(void);

// Cancel the pending-verify rollback on the currently-running slot. Call
// this once the app has proven itself healthy (wifi + first data fetch
// succeeded). Safe to call unconditionally; a no-op if the running slot
// is not in PENDING_VERIFY state.
void ota_mark_running_valid(void);

// Spawn a background task that runs ota_check_and_apply() shortly after
// boot and then once per day. Non-blocking.
void ota_start(void);

#ifdef __cplusplus
}
#endif
