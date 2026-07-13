// minimp3 is a header-only library — it emits the decoder implementation
// when MINIMP3_IMPLEMENTATION is defined before the include. We do that
// exactly once, in this dedicated translation unit, so audio.c only sees
// the declarations.
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
