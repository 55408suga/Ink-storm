#ifndef INK_GAME_WAVES_H
#define INK_GAME_WAVES_H

#include "game.h"

#include <stdint.h>

void waves_prepare_act(GameState *g);
void waves_start_wave(GameState *g, int32_t wave_index);
void waves_step_spawns(GameState *g);

int32_t waves_count_alive_escorts(const GameState *g);
EntityId waves_spawn_summoned_escort(GameState *g, EnemyDefId def_id, float x, float y);

#endif
