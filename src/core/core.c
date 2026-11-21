#include "core.h"

#include <math.h>
#include <string.h>

#define FNV1A_PRIME UINT64_C(0x100000001b3)

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

uint64_t ink_hash_bytes(uint64_t h, const void *data, size_t size)
{
    if (data == NULL || size == 0) {
        return h;
    }
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; ++i) {
        h ^= (uint64_t)bytes[i];
        h *= FNV1A_PRIME;
    }
    return h;
}

uint64_t ink_hash_u32(uint64_t h, uint32_t v)
{
    uint8_t bytes[4];
    for (int i = 0; i < 4; ++i) {
        bytes[i] = (uint8_t)((v >> (i * 8)) & 0xffu);
    }
    return ink_hash_bytes(h, bytes, sizeof bytes);
}

uint64_t ink_hash_u64(uint64_t h, uint64_t v)
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = (uint8_t)((v >> (i * 8)) & 0xffu);
    }
    return ink_hash_bytes(h, bytes, sizeof bytes);
}

uint64_t ink_hash_f32(uint64_t h, float v)
{
    uint32_t bits = 0;
    if (isnan(v)) {
        bits = UINT32_C(0x7fc00000);
    } else if (v == 0.0f) {
        bits = 0;
    } else {
        memcpy(&bits, &v, sizeof bits);
    }
    return ink_hash_u32(h, bits);
}
