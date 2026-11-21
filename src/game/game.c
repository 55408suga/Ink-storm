#include "game.h"

#include "core.h"

#include <stdlib.h>

GameState *game_create(uint64_t seed)
{
    GameState *game = calloc(1, sizeof *game);
    if (game != NULL) {
        game_init(game, seed);
    }
    return game;
}

void game_destroy(GameState *game) { free(game); }

void game_init(GameState *game, uint64_t seed)
{
    if (game == NULL) {
        return;
    }
    fluid_init(&game->fluid, NULL);
    ecs_init(&game->ecs);
    game->rng = seed;
    game->ticks = 0;
}

void game_step_smoke(GameState *game)
{
    if (game == NULL) {
        return;
    }
    fluid_step(&game->fluid, INK_FIXED_DT);
    ink_rng_next(&game->rng);
    ++game->ticks;
}
