#include "game.h"

#include <stddef.h>

EntityId game_debug_spawn_enemy(GameState *g, EnemyDefId def, float x, float y)
{
    /* P3-Pre stub */
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || (unsigned)def >= (unsigned)ENEMY_DEF_COUNT) {
        return invalid;
    }
    const bool is_boss = def == ENEMY_BOSS_TENGU;
    uint64_t mask = COMP_TRANSFORM | COMP_VELOCITY | COMP_HEALTH | COMP_STATUS | COMP_ENEMY;
    if (is_boss) {
        mask |= COMP_BOSS;
    }
    const EntityId id = game_spawn_entity(g, mask);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }

    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = is_boss ? 44.0f : 16.0f;
    }
    HealthComp *hp = game_health_mut(g, id);
    if (hp != NULL) {
        hp->max_hp = is_boss ? 400 : 20;
        hp->hp = hp->max_hp;
    }
    EnemyComp *en = game_enemy_mut(g, id);
    if (en != NULL) {
        en->def_id = def;
    }
    if (is_boss) {
        g->run.boss_id = id;
    }
    return id;
}

EntityId game_debug_spawn_projectile(GameState *g, ProjKind kind, float x, float y, float vx,
                                     float vy)
{
    /* P3-Pre stub */
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || (unsigned)kind >= (unsigned)PROJ_KIND_COUNT) {
        return invalid;
    }
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_VELOCITY | COMP_PROJ);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }

    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = 6.0f;
    }
    VelocityComp *vel = game_velocity_mut(g, id);
    if (vel != NULL) {
        vel->vx = vx;
        vel->vy = vy;
    }
    ProjComp *proj = game_proj_mut(g, id);
    if (proj != NULL) {
        proj->kind = kind;
    }
    return id;
}

EntityId game_debug_spawn_orb(GameState *g, float x, float y)
{
    /* P3-Pre stub */
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL) {
        return invalid;
    }
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_VELOCITY | COMP_INK_ORB);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }

    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = 8.0f;
    }
    InkOrbComp *orb = game_orb_mut(g, id);
    if (orb != NULL) {
        orb->value = 16;
    }
    return id;
}

void game_debug_start_wave(GameState *g, int32_t wave_index)
{
    /* P3-Pre stub */
    if (g == NULL || wave_index < 0 || wave_index > GAME_BOSS_WAVE_INDEX) {
        return;
    }
    static const int32_t k_quotas[GAME_BOSS_WAVE_INDEX + 1] = {6, 8, 10, 0};
    g->run.wave_index = wave_index;
    g->run.wave_kills = 0;
    g->run.wave_quota = k_quotas[wave_index];
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 0.0f;
}

void game_debug_force_wave_clear(GameState *g)
{
    /* P3-Pre stub */
    if (g == NULL || g->phase != RUN_PHASE_PLAYING) {
        return;
    }
    g->run.wave_kills = g->run.wave_quota;
}
