#include "core.h"

float ink_clampf(float value, float min, float max)
{
    if (value < min) {
        return min;
    }
    return value > max ? max : value;
}

uint64_t ink_rng_next(uint64_t *state)
{
    *state += UINT64_C(0x9e3779b97f4a7c15);
    uint64_t bits = *state;
    bits = (bits ^ (bits >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    bits = (bits ^ (bits >> 27)) * UINT64_C(0x94d049bb133111eb);
    return bits ^ (bits >> 31);
}

float ink_rng_f01(uint64_t *state) { return (float)(ink_rng_next(state) >> 40) * 0x1p-24f; }
