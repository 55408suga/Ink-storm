#ifndef INK_GAME_ELEMENTS_H
#define INK_GAME_ELEMENTS_H

#include "game.h"

void elements_step(GameState *g);
void elements_step_post_solve(GameState *g);
void elements_step_idle_cooldowns(GameState *g);

#endif
