#ifndef INK_GAME_ENEMY_DEFS_H
#define INK_GAME_ENEMY_DEFS_H

#include "game.h"

#include <stdint.h>

typedef struct {
    EnemyDefId id;
    int32_t base_hp;
    float radius;
    MassTier mass_tier;
    float fluid_coupling;
    int32_t score;
    int32_t orb_count;
    void (*spawn_fn)(GameState *g, EntityId id);
    void (*ai_step_fn)(GameState *g, EntityId id);
} EnemyDef;

const EnemyDef *enemy_def_get(EnemyDefId id);
EntityId enemy_spawn(GameState *g, EnemyDefId def_id, float x, float y);
void enemy_apply_elite(GameState *g, EntityId id, uint32_t elite_flags);
void enemies_ai_step(GameState *g);

/* Per-enemy hooks implemented in their dedicated translation units. */
void archer_spawn(GameState *g, EntityId id);
void archer_step(GameState *g, EntityId id);

void lantern_fire_spawn(GameState *g, EntityId id);
void lantern_fire_step(GameState *g, EntityId id);

void lantern_storm_spawn(GameState *g, EntityId id);
void lantern_storm_step(GameState *g, EntityId id);

void umbrella_spawn(GameState *g, EntityId id);
void umbrella_step(GameState *g, EntityId id);

void kamaitachi_spawn(GameState *g, EntityId id);
void kamaitachi_step(GameState *g, EntityId id);

void stone_oni_spawn(GameState *g, EntityId id);
void stone_oni_step(GameState *g, EntityId id);

void ink_leech_spawn(GameState *g, EntityId id);
void ink_leech_step(GameState *g, EntityId id);

void boss_tengu_spawn(GameState *g, EntityId id);
void boss_tengu_step(GameState *g, EntityId id);

void boss_nue_spawn(GameState *g, EntityId id);
void boss_nue_step(GameState *g, EntityId id);

void boss_ryu_spawn(GameState *g, EntityId id);
void boss_ryu_step(GameState *g, EntityId id);

void ofuda_fox_spawn(GameState *g, EntityId id);
void ofuda_fox_step(GameState *g, EntityId id);

#endif
