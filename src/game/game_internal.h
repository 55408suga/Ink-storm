#ifndef INK_GAME_INTERNAL_H
#define INK_GAME_INTERNAL_H

#include <stdint.h>

/* C11 forbids empty structs; P3-A replaces this with simulation-internal state. */
typedef struct {
    uint32_t reserved;
} GameInternal;

#endif
