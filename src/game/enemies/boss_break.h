#ifndef INK_GAME_BOSS_BREAK_H
#define INK_GAME_BOSS_BREAK_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOSS_BREAK_NONE = 0,
    BOSS_BREAK_STANCE,
    BOSS_BREAK_SLAM,
    BOSS_BREAK_FLINCH
} BossBreakKind;

float boss_break_threshold(const GameState *g);
float boss_break_duration_for(EnemyDefId def_id, BossBreakKind kind);
void boss_break_add_stagger(GameState *g, EntityId boss, float amount);
void boss_break_start(GameState *g, EntityId boss, BossBreakKind kind, float dur, int32_t orb_n);
void boss_break_end(GameState *g, EntityId boss, bool emit_end);
void boss_break_step(GameState *g, EntityId boss);
float boss_break_damage_mul(const GameState *g, EntityId target, uint32_t dmg_flags);

#endif
