#include "game.h"

#include "core.h"

void game_init(GameState *game, uint64_t seed)
{
    fluid_init(&game->fluid);
    ecs_init(&game->ecs);
    game->rng = seed;
    game->ticks = 0;
}

void game_step_smoke(GameState *game)
{
    fluid_step_smoke(&game->fluid);
    ink_rng_next(&game->rng);
    ++game->ticks;
}
