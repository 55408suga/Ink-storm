#ifndef INK_CORE_H
#define INK_CORE_H

#include <stdint.h>

#define INK_VERSION "0.1.0"

float ink_clampf(float value, float min, float max);
uint64_t ink_rng_next(uint64_t *state);
float ink_rng_f01(uint64_t *state);

#endif
