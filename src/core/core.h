#ifndef INK_CORE_H
#define INK_CORE_H

#include <stddef.h>
#include <stdint.h>

#define INK_VERSION "0.1.0"
#define INK_SCREEN_W 1280
#define INK_SCREEN_H 720
#define INK_TICK_HZ 60
#define INK_FIXED_DT (1.0f / 60.0f)

#define INK_HASH_INIT UINT64_C(0xcbf29ce484222325)

float ink_clampf(float value, float min, float max);
uint64_t ink_rng_next(uint64_t *state);
float ink_rng_f01(uint64_t *state);

uint64_t ink_hash_bytes(uint64_t h, const void *data, size_t size);
uint64_t ink_hash_u32(uint64_t h, uint32_t v);
uint64_t ink_hash_u64(uint64_t h, uint64_t v);
uint64_t ink_hash_f32(uint64_t h, float v);

#endif
