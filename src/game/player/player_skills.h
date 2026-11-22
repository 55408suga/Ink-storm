#ifndef INK_GAME_PLAYER_SKILLS_H
#define INK_GAME_PLAYER_SKILLS_H

#include "game.h"

void player_skills_step(GameState *g, const BrushInput *valid_in);
void player_skills_cast_free_gale(GameState *g, float origin_x, float origin_y, float dir_x,
                                  float dir_y);

#endif
