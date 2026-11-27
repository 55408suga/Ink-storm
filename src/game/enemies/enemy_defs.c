#include "enemies/enemy_defs.h"

#include "core.h"

#include <math.h>
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
    [ENEMY_LANTERN_STORM] =
        {
            .id = ENEMY_LANTERN_STORM,
            .base_hp = 20,
            .radius = 17.0f,
            .mass_tier = MASS_TURRET,
            .fluid_coupling = 0.0f,
            .score = 140,
            .orb_count = 2,
            .spawn_fn = lantern_storm_spawn,
            .ai_step_fn = lantern_storm_step,
        },
    [ENEMY_KAMAITACHI] =
        {
            .id = ENEMY_KAMAITACHI,
            .base_hp = 30,
            .radius = 16.0f,
            .mass_tier = MASS_MEDIUM,
            .fluid_coupling = 1.5f,
            .score = 160,
            .orb_count = 1,
            .spawn_fn = kamaitachi_spawn,
            .ai_step_fn = kamaitachi_step,
        },
    [ENEMY_STONE_ONI] =
        {
            .id = ENEMY_STONE_ONI,
            .base_hp = 90,
            .radius = 24.0f,
            .mass_tier = MASS_HEAVY,
            .fluid_coupling = 0.3f,
            .score = 300,
            .orb_count = 3,
            .spawn_fn = stone_oni_spawn,
            .ai_step_fn = stone_oni_step,
        },
    [ENEMY_INK_LEECH] =
        {
            .id = ENEMY_INK_LEECH,
            .base_hp = 40,
            .radius = 19.0f,
            .mass_tier = MASS_MEDIUM,
            .fluid_coupling = 1.5f,
            .score = 220,
            .orb_count = 3,
            .spawn_fn = ink_leech_spawn,
            .ai_step_fn = ink_leech_step,
        },
    [ENEMY_BOSS_NUE] =
        {
            .id = ENEMY_BOSS_NUE,
            .base_hp = 700,
            .radius = 40.0f,
            .mass_tier = MASS_BOSS,
            .fluid_coupling = 0.0f,
            .score = 1500,
            .orb_count = 8,
            .spawn_fn = boss_nue_spawn,
            .ai_step_fn = boss_nue_step,
        },
    [ENEMY_BOSS_RYU] =
        {
            .id = ENEMY_BOSS_RYU,
            .base_hp = 900,
            .radius = 46.0f,
            .mass_tier = MASS_BOSS,
            .fluid_coupling = 0.0f,
            .score = 1500,
            .orb_count = 8,
            .spawn_fn = boss_ryu_spawn,
            .ai_step_fn = boss_ryu_step,
        },
    [ENEMY_OFUDA_FOX] =
        {
            .id = ENEMY_OFUDA_FOX,
            .base_hp = 30,
            .radius = 17.0f,
            .mass_tier = MASS_MEDIUM,
            .fluid_coupling = 1.5f,
            .score = 200,
            .orb_count = 2,
            .spawn_fn = ofuda_fox_spawn,
            .ai_step_fn = ofuda_fox_step,
        },
    [ENEMY_BOSS_RYU_ORB] =
        {
            .id = ENEMY_BOSS_RYU_ORB,
            .base_hp = 300,
            .radius = 30.0f,
            .mass_tier = MASS_BOSS,
            .fluid_coupling = 0.0f,
            .score = 300,
            .orb_count = 2,
            .spawn_fn = NULL,
            .ai_step_fn = NULL,
        },
};

const EnemyDef *enemy_def_get(EnemyDefId id)
{
    if ((unsigned)id >= (unsigned)ENEMY_DEF_COUNT) {
        return NULL;
    }
    return &k_enemy_defs[id];
}

int32_t enemy_endless_loop_n(const GameState *g)
{
    if (g == NULL) {
        return 0;
    }
    int32_t n = g->run.act_index / GAME_MAIN_ACT_COUNT;
    if (n < 0) {
        n = 0;
    } else if (n > ENDLESS_MAX_LOOP_N) {
        n = ENDLESS_MAX_LOOP_N;
    }
    return n;
}

float enemy_endless_boss_cd_mul(const GameState *g)
{
    const int32_t n = enemy_endless_loop_n(g);
    if (n <= 0) {
        return 1.0f;
    }
    return fmaxf(0.70f, powf(0.90f, (float)n));
}

EntityId enemy_spawn(GameState *g, EnemyDefId def_id, float x, float y)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || (unsigned)def_id >= (unsigned)ENEMY_DEF_COUNT) {
        return invalid;
    }
    const EnemyDef *def = &k_enemy_defs[def_id];
    const bool is_boss = game_is_boss_enemy(def_id);
    const bool is_boss_part = game_is_boss_part(def_id);
    uint64_t mask = COMP_TRANSFORM | COMP_VELOCITY | COMP_HEALTH | COMP_STATUS | COMP_ENEMY;
    if (is_boss || is_boss_part) {
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
        const int32_t loop_n = enemy_endless_loop_n(g);
        const float hp_mul = (is_boss || is_boss_part) ? (1.0f + 0.35f * (float)loop_n)
                                                       : (1.0f + 0.25f * (float)loop_n);
        const int32_t scaled_hp = (int32_t)floorf((float)def->base_hp * hp_mul + 0.5f);
        hp->max_hp = scaled_hp > 0 ? scaled_hp : 1;
        hp->hp = hp->max_hp;
        hp->flash_t = 0.45f;
    }
    EnemyComp *en = game_enemy_mut(g, id);
    if (en != NULL) {
        en->def_id = def_id;
    }
    if (is_boss) {
        g->run.boss_id = id;
        g->internal.last_boss_x = x;
        g->internal.last_boss_y = y;
    }
    if (def->spawn_fn != NULL) {
        def->spawn_fn(g, id);
    }
    return id;
}

void enemy_apply_elite(GameState *g, EntityId id, uint32_t elite_flags)
{
    if (g == NULL || !ecs_is_alive(&g->ecs, id)) {
        return;
    }
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    HealthComp *hp = game_health_mut(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || vel == NULL || hp == NULL || en == NULL) {
        return;
    }
    const EnemyDef *def = enemy_def_get(en->def_id);
    if (def == NULL || game_is_boss_enemy(en->def_id) || game_is_boss_part(en->def_id) ||
        en->def_id == ENEMY_OFUDA_FOX) {
        return;
    }

    uint32_t flags = elite_flags & ELITE_ALL_MASK;
    if (def->mass_tier == MASS_TURRET) {
        flags &= ~ELITE_RIGID;
    }

    en->is_elite = true;
    en->elite_flags = (en->elite_flags & ~ELITE_ALL_MASK) | flags;

    const int32_t loop_n = enemy_endless_loop_n(g);
    const int32_t elite_hp =
        (int32_t)floorf((float)def->base_hp * (1.0f + 0.25f * (float)loop_n) * 2.5f + 0.5f);
    hp->max_hp = elite_hp > 0 ? elite_hp : 1;
    hp->hp = hp->max_hp;
    tr->radius = def->radius * 1.2f;

    vel->mass_tier = def->mass_tier;
    if ((flags & ELITE_RIGID) != 0u) {
        if (vel->mass_tier == MASS_LIGHT) {
            vel->mass_tier = MASS_MEDIUM;
        } else if (vel->mass_tier == MASS_MEDIUM) {
            vel->mass_tier = MASS_HEAVY;
        }
    }
}

static void step_elite_ink_repel(GameState *g, float ex, float ey)
{
    float *ink_ch = fluid_channel_mut(&g->fluid, 0);
    if (ink_ch == NULL) {
        return;
    }
    const float repel_r = 48.0f;
    const int reach = fluid_cell_reach(repel_r);
    const int cx = (int)(ex / FLUID_CELL_PX);
    const int cy = (int)(ey / FLUID_CELL_PX);
    const int x0 = cx - reach < 1 ? 1 : cx - reach;
    const int x1 = cx + reach > FLUID_W - 2 ? FLUID_W - 2 : cx + reach;
    const int y0 = cy - reach < 1 ? 1 : cy - reach;
    const int y1 = cy + reach > FLUID_H - 2 ? FLUID_H - 2 : cy + reach;
    const float keep = 1.0f - 0.20f * INK_FIXED_DT;

    for (int gy = y0; gy <= y1; ++gy) {
        const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
        const float dy = cell_y - ey;
        const int row = gy * FLUID_W;
        for (int gx = x0; gx <= x1; ++gx) {
            const int idx = row + gx;
            if (g->fluid.solid[idx] != 0) {
                continue;
            }
            const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
            const float dx = cell_x - ex;
            if (dx * dx + dy * dy <= repel_r * repel_r && isfinite(ink_ch[idx])) {
                ink_ch[idx] *= keep;
            }
        }
    }
    fluid_end_mut_borrow(&g->fluid);
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
        if ((en->elite_flags & ELITE_INK_REPEL) != 0u) {
            step_elite_ink_repel(g, tr->x, tr->y);
        }
        const EnemyDef *def = enemy_def_get(en->def_id);
        if (def != NULL && def->ai_step_fn != NULL) {
            def->ai_step_fn(g, id);
        }
    }
}
