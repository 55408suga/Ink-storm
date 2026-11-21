#include "game.h"

#include "combat/damage.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "roguelike/waves.h"

#include <stddef.h>

EntityId game_debug_spawn_enemy(GameState *g, EnemyDefId def, float x, float y)
{
    return enemy_spawn(g, def, x, y);
}

EntityId game_debug_spawn_projectile(GameState *g, ProjKind kind, float x, float y, float vx,
                                     float vy)
{
    return projectiles_spawn(g, kind, x, y, vx, vy);
}

EntityId game_debug_spawn_orb(GameState *g, float x, float y) { return combat_spawn_orb(g, x, y); }

void game_debug_start_wave(GameState *g, int32_t wave_index) { waves_start_wave(g, wave_index); }

void game_debug_force_wave_clear(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_PLAYING) {
        return;
    }
    if (g->run.wave_index == GAME_BOSS_WAVE_INDEX) {
        if (g->run.boss_id.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, g->run.boss_id)) {
            HealthComp *hp = game_health_mut(g, g->run.boss_id);
            if (hp != NULL) {
                hp->invuln_t = 0.0f;
                const int32_t lethal = hp->max_hp > 0 ? hp->max_hp * 4 : 10000;
                (void)combat_apply_damage(g, g->run.boss_id, lethal, DMG_PHYSICAL, 0.0f, 0.0f);
            }
        }
    } else {
        g->run.wave_kills = g->run.wave_quota;
    }
}
