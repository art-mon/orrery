#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Configure KY-040 CLK/DT as inputs (SW is not read yet).
void encoder_init(void);

// Poll once per frame. Returns +1 for one CW detent, -1 for CCW, 0 otherwise.
int encoder_read_delta(void);

// Poll once per frame. Returns the number of button presses (falling edges,
// debounced) since the last call; 0 if none.
int encoder_read_pressed(void);

#ifdef __cplusplus
}
#endif
