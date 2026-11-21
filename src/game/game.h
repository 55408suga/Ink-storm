#ifndef INK_GAME_H
#define INK_GAME_H

#include "ecs.h"
#include "fluid.h"

#include <stdint.h>

typedef struct {
    FluidState fluid;
    EcsWorld ecs;
    uint64_t rng;
    uint64_t ticks;
} GameState;

void game_init(GameState *game, uint64_t seed);
void game_step_smoke(GameState *game);

#endif
