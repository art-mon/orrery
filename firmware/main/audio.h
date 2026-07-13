#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_BRIEFING_IDLE    = 0,
    AUDIO_BRIEFING_LOADING = 1,  // fetching the MP3 into RAM
    AUDIO_BRIEFING_PLAYING = 2,  // decoding + streaming to I2S
} audio_briefing_state_t;

// Bring up the MAX98357A I2S TX channel and start a single audio task
// (pinned to CPU1) that owns all writes to the I2S bus. Idle-safe: writes
// silence until a tone_start or briefing_start call switches modes.
void audio_init(void);

// Continuous sine tone. Thread-safe (atomic).
void audio_tone_start(int freq_hz);
void audio_tone_stop(void);

// Kick off a briefing playback: the audio task will download the MP3 at
// `url` into PSRAM, then decode+play it. Non-blocking. Ignored if a
// briefing is already active. `url` is copied.
void audio_briefing_start(const char *url);

// Signal the running briefing to stop. Idempotent. The audio task will
// abort at the next network read or decoded frame, free the buffer, and
// return to silent mode.
void audio_briefing_stop(void);

// Snapshot of the briefing lifecycle. Cheap; safe to call every frame.
audio_briefing_state_t audio_briefing_state(void);

// Convenience: true while state != IDLE.
bool audio_briefing_active(void);

#ifdef __cplusplus
}
#endif
