#include "enemies/enemy_defs.h"

#include <stddef.h>

static const EnemyDef k_enemy_defs[ENEMY_DEF_COUNT] = {
    [ENEMY_ARCHER] =
        {
            .id = ENEMY_ARCHER,
            .base_hp = 20,
            .radius = 16.0f,
            .mass_tier = MASS_TURRET,
            .fluid_coupling = 0.0f,
            .score = 120,
            .orb_count = 1,
            .spawn_fn = archer_spawn,
            .ai_step_fn = archer_step,
        },
    [ENEMY_LANTERN_FIRE] =
        {
            .id = ENEMY_LANTERN_FIRE,
            .base_hp = 20,
            .radius = 17.0f,
            .mass_tier = MASS_TURRET,
            .fluid_coupling = 0.0f,
            .score = 120,
            .orb_count = 2,
            .spawn_fn = lantern_fire_spawn,
            .ai_step_fn = lantern_fire_step,
        },
    [ENEMY_UMBRELLA] =
        {
            .id = ENEMY_UMBRELLA,
            .base_hp = 30,
            .radius = 18.0f,
            .mass_tier = MASS_LIGHT,
            .fluid_coupling = 3.8f,
            .score = 180,
            .orb_count = 2,
            .spawn_fn = umbrella_spawn,
            .ai_step_fn = umbrella_step,
        },
    [ENEMY_BOSS_TENGU] =
        {
            .id = ENEMY_BOSS_TENGU,
            .base_hp = 400,
            .radius = 44.0f,
            .mass_tier = MASS_BOSS,
            .fluid_coupling = 0.0f,
            .score = 1500,
            .orb_count = 8,
            .spawn_fn = boss_tengu_spawn,
            .ai_step_fn = boss_tengu_step,
        },
};

const EnemyDef *enemy_def_get(EnemyDefId id)
{
    if ((unsigned)id >= (unsigned)ENEMY_DEF_COUNT) {
        return NULL;
    }
    return &k_enemy_defs[id];
}

EntityId enemy_spawn(GameState *g, EnemyDefId def_id, float x, float y)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || (unsigned)def_id >= (unsigned)ENEMY_DEF_COUNT) {
        return invalid;
    }
    const EnemyDef *def = &k_enemy_defs[def_id];
    uint64_t mask = COMP_TRANSFORM | COMP_VELOCITY | COMP_HEALTH | COMP_STATUS | COMP_ENEMY;
    if (def_id == ENEMY_BOSS_TENGU) {
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
        tr->radius = def->radius;
    }
    VelocityComp *vel = game_velocity_mut(g, id);
    if (vel != NULL) {
        vel->mass_tier = def->mass_tier;
        vel->fluid_coupling = def->fluid_coupling;
    }
    HealthComp *hp = game_health_mut(g, id);
    if (hp != NULL) {
        hp->max_hp = def->base_hp;
        hp->hp = def->base_hp;
        hp->flash_t = 0.45f;
    }
    EnemyComp *en = game_enemy_mut(g, id);
    if (en != NULL) {
        en->def_id = def_id;
    }
    if (def->spawn_fn != NULL) {
        def->spawn_fn(g, id);
    }
    return id;
}

void enemies_ai_step(GameState *g)
{
    if (g == NULL) {
        return;
    }
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL || tr->dissolving) {
            continue;
        }
        const EnemyDef *def = enemy_def_get(en->def_id);
        if (def != NULL && def->ai_step_fn != NULL) {
            def->ai_step_fn(g, id);
        }
    }
}
