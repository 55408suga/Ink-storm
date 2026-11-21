#ifndef INK_CORE_H
#define INK_CORE_H

#include <stdint.h>

#define INK_VERSION "0.1.0"
#define INK_SCREEN_W 1280
#define INK_SCREEN_H 720
#define INK_TICK_HZ 60
#define INK_FIXED_DT (1.0f / 60.0f)

float ink_clampf(float value, float min, float max);
uint64_t ink_rng_next(uint64_t *state);
float ink_rng_f01(uint64_t *state);

#endif
