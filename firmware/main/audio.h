#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Bring up the MAX98357A I2S TX channel and start a background writer task.
// Idle-safe: writes silence until audio_tone_start is called.
void audio_init(void);

// Start / stop a continuous sine tone. Thread-safe (atomic).
void audio_tone_start(int freq_hz);
void audio_tone_stop(void);

#ifdef __cplusplus
}
#endif
