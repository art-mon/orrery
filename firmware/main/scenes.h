#pragma once

#include "daily.h"

#ifdef __cplusplus
extern "C" {
#endif

// Run the scene rotator forever. `data` may be updated externally between
// rotator ticks (the loop re-reads it each frame). The data fetcher should
// run on a separate task; scenes_run does its own panel drawing only.
void scenes_run(const daily_data_t *data);

#ifdef __cplusplus
}
#endif
