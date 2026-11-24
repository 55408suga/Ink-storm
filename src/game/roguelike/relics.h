#ifndef INK_GAME_RELICS_H
#define INK_GAME_RELICS_H

#include "roguelike/upgrades.h"

#include <stdbool.h>
#include <stdint.h>

bool roguelike_grant_relic(GameState *g, RelicId id);
void relics_apply_modifiers_add(const GameState *g, RunModifiers *mods);
void relics_apply_modifiers_mul(const GameState *g, RunModifiers *mods);

void relics_fire_on_stroke(GameState *g, float px, float py, float x, float y, float vx, float vy,
                           float dist);
void relics_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                            float *storm_sustain_add);
void relics_fire_on_gale(GameState *g, float x, float y, float dx, float dy);
void relics_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags);
void relics_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags);
void relics_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity);
void relics_fire_on_tick(GameState *g, float dt);

#endif
