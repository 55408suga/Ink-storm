#ifndef INK_GAME_REWARDS_H
#define INK_GAME_REWARDS_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

void rewards_generate_offer(GameState *g);
void rewards_generate_relic_offer(GameState *g);
bool rewards_pick_reward(GameState *g, int32_t choice_index);
bool rewards_can_reroll(const GameState *g);
bool rewards_reroll(GameState *g);
bool rewards_repair_converts_excess(const GameState *g);
bool rewards_repair_shrine(GameState *g);
bool rewards_pick_endless_crystal(GameState *g);

#endif
