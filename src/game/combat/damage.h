#ifndef INK_GAME_DAMAGE_H
#define INK_GAME_DAMAGE_H

#include "game.h"

#include <stdint.h>

int32_t combat_apply_damage_f(GameState *g, EntityId target, float base_dmg, uint32_t dmg_flags,
                              float kx, float ky);
int32_t combat_apply_damage(GameState *g, EntityId target, int32_t base_dmg, uint32_t dmg_flags,
                            float kx, float ky);
void combat_damage_shrine(GameState *g, float source_x, float source_y, int32_t dmg);

EntityId combat_spawn_orb(GameState *g, float x, float y);
void combat_physics_and_slams_step(GameState *g);
void combat_orbs_step(GameState *g);
void combat_begin_wave_aftermath(GameState *g);

#endif
