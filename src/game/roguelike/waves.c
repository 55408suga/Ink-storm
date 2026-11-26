#include "roguelike/waves.h"

#include "core.h"
#include "enemies/enemy_defs.h"
#include "roguelike/stage_gimmicks.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define MAX_INITIAL_ROSTER 8

typedef struct {
    EnemyDefId def_id;
    int32_t count;
} RosterEntry;

typedef struct {
    int32_t quota;
    int32_t max_alive;
    float spawn_interval;
    RosterEntry initial[MAX_INITIAL_ROSTER];
    int32_t initial_entries;
    uint32_t weights[ENEMY_DEF_COUNT];
} WaveDef;

typedef struct {
    int32_t act_index;
    int32_t wave_count;
    EnemyDefId boss_def_id;
    WaveDef waves[GAME_BOSS_WAVE_INDEX + 1];
} ActDef;

static const ActDef
    k_act_defs[GAME_MAIN_ACT_COUNT] =
        {
            [0] =
                {
                    .act_index = 0,
                    .wave_count = 3,
                    .boss_def_id = ENEMY_BOSS_TENGU,
                    .waves =
                        {
                            [0] =
                                {
                                    .quota = 6,
                                    .max_alive = 5,
                                    .spawn_interval = 1.1f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 3},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                        },
                                    .initial_entries = 2,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 45u,
                                            [ENEMY_LANTERN_FIRE] = 30u,
                                        },
                                },
                            [1] =
                                {
                                    .quota = 8,
                                    .max_alive = 6,
                                    .spawn_interval = 1.0f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                            {.def_id = ENEMY_UMBRELLA, .count = 2},
                                        },
                                    .initial_entries = 3,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 45u,
                                            [ENEMY_LANTERN_FIRE] = 30u,
                                            [ENEMY_UMBRELLA] = 25u,
                                        },
                                },
                            [2] =
                                {
                                    .quota = 10,
                                    .max_alive = 6,
                                    .spawn_interval = 1.0f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 2},
                                            {.def_id = ENEMY_UMBRELLA, .count = 2},
                                        },
                                    .initial_entries = 3,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 45u,
                                            [ENEMY_LANTERN_FIRE] = 30u,
                                            [ENEMY_UMBRELLA] = 25u,
                                        },
                                },
                            [GAME_BOSS_WAVE_INDEX] =
                                {
                                    .quota = 0,
                                    .max_alive = 4,
                                    .spawn_interval = 2.4f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_BOSS_TENGU, .count = 1},
                                            {.def_id = ENEMY_ARCHER, .count = 1},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                        },
                                    .initial_entries = 3,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 45u,
                                            [ENEMY_LANTERN_FIRE] = 30u,
                                            [ENEMY_UMBRELLA] = 25u,
                                        },
                                },
                        },
                },
            [1] =
                {
                    .act_index = 1,
                    .wave_count = 3,
                    .boss_def_id = ENEMY_BOSS_NUE,
                    .waves =
                        {
                            [0] =
                                {
                                    .quota = 10,
                                    .max_alive = 7,
                                    .spawn_interval = 0.9f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_STORM, .count = 1},
                                            {.def_id = ENEMY_UMBRELLA, .count = 1},
                                            {.def_id = ENEMY_KAMAITACHI, .count = 1},
                                        },
                                    .initial_entries = 4,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 25u,
                                            [ENEMY_LANTERN_FIRE] = 15u,
                                            [ENEMY_LANTERN_STORM] = 15u,
                                            [ENEMY_UMBRELLA] = 20u,
                                            [ENEMY_KAMAITACHI] = 15u,
                                        },
                                },
                            [1] =
                                {
                                    .quota = 12,
                                    .max_alive = 7,
                                    .spawn_interval = 0.9f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                            {.def_id = ENEMY_LANTERN_STORM, .count = 1},
                                            {.def_id = ENEMY_STONE_ONI, .count = 1},
                                        },
                                    .initial_entries = 4,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 25u,
                                            [ENEMY_LANTERN_FIRE] = 15u,
                                            [ENEMY_LANTERN_STORM] = 15u,
                                            [ENEMY_UMBRELLA] = 20u,
                                            [ENEMY_KAMAITACHI] = 15u,
                                            [ENEMY_STONE_ONI] = 10u,
                                        },
                                },
                            [2] =
                                {
                                    .quota = 14,
                                    .max_alive = 7,
                                    .spawn_interval = 0.9f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                            {.def_id = ENEMY_LANTERN_STORM, .count = 1},
                                            {.def_id = ENEMY_UMBRELLA, .count = 1},
                                            {.def_id = ENEMY_KAMAITACHI, .count = 1},
                                        },
                                    .initial_entries = 5,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 25u,
                                            [ENEMY_LANTERN_FIRE] = 15u,
                                            [ENEMY_LANTERN_STORM] = 15u,
                                            [ENEMY_UMBRELLA] = 20u,
                                            [ENEMY_KAMAITACHI] = 15u,
                                            [ENEMY_STONE_ONI] = 10u,
                                        },
                                },
                            [GAME_BOSS_WAVE_INDEX] =
                                {
                                    .quota = 0,
                                    .max_alive = 4,
                                    .spawn_interval = 2.4f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_BOSS_NUE, .count = 1},
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                        },
                                    .initial_entries = 2,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 25u,
                                            [ENEMY_LANTERN_FIRE] = 15u,
                                            [ENEMY_LANTERN_STORM] = 15u,
                                            [ENEMY_UMBRELLA] = 20u,
                                            [ENEMY_KAMAITACHI] = 15u,
                                            [ENEMY_STONE_ONI] = 10u,
                                        },
                                },
                        },
                },
            [2] =
                {
                    .act_index = 2,
                    .wave_count = 3,
                    .boss_def_id = ENEMY_BOSS_RYU,
                    .waves =
                        {
                            [0] =
                                {
                                    .quota = 14,
                                    .max_alive = 8,
                                    .spawn_interval = 0.8f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                            {.def_id = ENEMY_LANTERN_STORM, .count = 1},
                                            {.def_id = ENEMY_UMBRELLA, .count = 1},
                                            {.def_id = ENEMY_INK_LEECH, .count = 1},
                                        },
                                    .initial_entries = 5,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 20u,
                                            [ENEMY_LANTERN_FIRE] = 12u,
                                            [ENEMY_LANTERN_STORM] = 12u,
                                            [ENEMY_UMBRELLA] = 16u,
                                            [ENEMY_KAMAITACHI] = 12u,
                                            [ENEMY_STONE_ONI] = 10u,
                                            [ENEMY_INK_LEECH] = 10u,
                                            [ENEMY_OFUDA_FOX] = 8u,
                                        },
                                },
                            [1] =
                                {
                                    .quota = 16,
                                    .max_alive = 8,
                                    .spawn_interval = 0.8f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_UMBRELLA, .count = 1},
                                            {.def_id = ENEMY_KAMAITACHI, .count = 1},
                                            {.def_id = ENEMY_STONE_ONI, .count = 1},
                                            {.def_id = ENEMY_OFUDA_FOX, .count = 1},
                                        },
                                    .initial_entries = 5,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 20u,
                                            [ENEMY_LANTERN_FIRE] = 12u,
                                            [ENEMY_LANTERN_STORM] = 12u,
                                            [ENEMY_UMBRELLA] = 16u,
                                            [ENEMY_KAMAITACHI] = 12u,
                                            [ENEMY_STONE_ONI] = 10u,
                                            [ENEMY_INK_LEECH] = 10u,
                                            [ENEMY_OFUDA_FOX] = 8u,
                                        },
                                },
                            [2] =
                                {
                                    .quota = 18,
                                    .max_alive = 8,
                                    .spawn_interval = 0.8f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_ARCHER, .count = 2},
                                            {.def_id = ENEMY_LANTERN_FIRE, .count = 1},
                                            {.def_id = ENEMY_LANTERN_STORM, .count = 1},
                                            {.def_id = ENEMY_STONE_ONI, .count = 1},
                                            {.def_id = ENEMY_INK_LEECH, .count = 1},
                                        },
                                    .initial_entries = 5,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 20u,
                                            [ENEMY_LANTERN_FIRE] = 12u,
                                            [ENEMY_LANTERN_STORM] = 12u,
                                            [ENEMY_UMBRELLA] = 16u,
                                            [ENEMY_KAMAITACHI] = 12u,
                                            [ENEMY_STONE_ONI] = 10u,
                                            [ENEMY_INK_LEECH] = 10u,
                                            [ENEMY_OFUDA_FOX] = 8u,
                                        },
                                },
                            [GAME_BOSS_WAVE_INDEX] =
                                {
                                    .quota = 0,
                                    .max_alive = 4,
                                    .spawn_interval = 2.4f,
                                    .initial =
                                        {
                                            {.def_id = ENEMY_BOSS_RYU, .count = 1},
                                        },
                                    .initial_entries = 1,
                                    .weights =
                                        {
                                            [ENEMY_ARCHER] = 20u,
                                            [ENEMY_LANTERN_FIRE] = 12u,
                                            [ENEMY_LANTERN_STORM] = 12u,
                                            [ENEMY_UMBRELLA] = 16u,
                                            [ENEMY_KAMAITACHI] = 12u,
                                            [ENEMY_STONE_ONI] = 10u,
                                            [ENEMY_INK_LEECH] = 10u,
                                            [ENEMY_OFUDA_FOX] = 8u,
                                        },
                                },
                        },
                },
};

static int32_t stage_act_index(int32_t act_index)
{
    return act_index < 0 ? 0 : (act_index % GAME_MAIN_ACT_COUNT);
}

static const ActDef *current_act_def(const GameState *g)
{
    return &k_act_defs[stage_act_index(g->run.act_index)];
}

static void clear_ryu_body(GameState *g)
{
    memset(&g->ryu_body, 0, sizeof g->ryu_body);
    g->ryu_body.head_id = (EntityId){ECS_INVALID_INDEX, 0};
    for (int32_t i = 0; i < GAME_RYU_ORB_COUNT; ++i) {
        g->ryu_body.orb_ids[i] = (EntityId){ECS_INVALID_INDEX, 0};
    }
    g->ryu_body.active_orb_slot = -1;
}

static uint8_t clamp_reroll_lv(const RunConfig *cfg)
{
    const uint8_t max_lv = meta_upgrade_max_level(META_UPG_REROLLS);
    const uint8_t lv = cfg->meta_upgrade_lv[META_UPG_REROLLS];
    return lv > max_lv ? max_lv : lv;
}

static float rng_range_f(uint64_t *rng, float lo, float hi)
{
    return lo + (hi - lo) * ink_rng_f01(rng);
}

static int32_t per_type_cap(int32_t act_index, EnemyDefId def_id)
{
    if (def_id == ENEMY_KAMAITACHI) {
        return 2;
    }
    if (def_id == ENEMY_STONE_ONI) {
        return stage_act_index(act_index) >= 2 ? 2 : 1;
    }
    if (def_id == ENEMY_INK_LEECH) {
        return 2;
    }
    if (def_id == ENEMY_OFUDA_FOX) {
        return 1;
    }
    return 0;
}

static bool spawn_pos_is_valid(const GameState *g, float x, float y)
{
    if (fluid_is_solid(&g->fluid, x, y)) {
        return false;
    }
    if (g->shrine_count > 0) {
        const float sdx = x - g->shrines[0].x;
        const float sdy = y - g->shrines[0].y;
        if (sdx * sdx + sdy * sdy < 200.0f * 200.0f) {
            return false;
        }
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
        const float dx = tr->x - x;
        const float dy = tr->y - y;
        if (dx * dx + dy * dy < 88.0f * 88.0f) {
            return false;
        }
    }
    return true;
}

static void pick_spawn_position(GameState *g, EnemyDefId def_id, int32_t edge_hint, float *out_x,
                                float *out_y)
{
    if (def_id == ENEMY_BOSS_TENGU) {
        *out_x = (float)INK_SCREEN_W * 0.5f;
        *out_y = 112.0f;
        return;
    }
    if (def_id == ENEMY_BOSS_NUE) {
        *out_x = (float)INK_SCREEN_W * 0.5f;
        *out_y = 140.0f;
        return;
    }
    if (def_id == ENEMY_BOSS_RYU) {
        *out_x = 260.0f;
        *out_y = 150.0f;
        return;
    }
    float cand_x = 76.0f;
    float cand_y = 140.0f;
    for (int32_t tries = 0; tries < 10; ++tries) {
        const int32_t side = edge_hint >= 0 ? (edge_hint + tries) % 4
                                            : (int32_t)(ink_rng_next(&g->rng) % UINT64_C(4));
        if (side == 0) {
            cand_x = 76.0f;
            cand_y = rng_range_f(&g->rng, 130.0f, (float)INK_SCREEN_H - 115.0f);
        } else if (side == 1) {
            cand_x = (float)INK_SCREEN_W - 76.0f;
            cand_y = rng_range_f(&g->rng, 130.0f, (float)INK_SCREEN_H - 115.0f);
        } else if (side == 2) {
            cand_x = rng_range_f(&g->rng, 210.0f, (float)INK_SCREEN_W - 210.0f);
            cand_y = 96.0f;
        } else {
            cand_x = rng_range_f(&g->rng, 210.0f, (float)INK_SCREEN_W - 210.0f);
            cand_y = (float)INK_SCREEN_H - 86.0f;
        }
        if (spawn_pos_is_valid(g, cand_x, cand_y)) {
            break;
        }
    }
    *out_x = cand_x;
    *out_y = cand_y;
}

static void record_enemy_intro(GameState *g, EnemyDefId def_id)
{
    const uint32_t bit = UINT32_C(1) << (unsigned)def_id;
    if ((g->internal.seen_enemies_mask & bit) == 0u) {
        g->internal.seen_enemies_mask |= bit;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_ENEMY_INTRO,
                                     .value = (int32_t)def_id,
                                     .flags = 0,
                                 });
    }
}

static uint32_t roll_elite_flags(GameState *g, EnemyDefId def_id)
{
    const EnemyDef *def = enemy_def_get(def_id);
    uint32_t pool[4];
    int32_t pool_n = 0;
    if (def != NULL && def->mass_tier != MASS_TURRET) {
        pool[pool_n++] = ELITE_RIGID;
    }
    pool[pool_n++] = ELITE_INK_REPEL;
    pool[pool_n++] = ELITE_FIREPROOF;
    pool[pool_n++] = ELITE_STORMPROOF;

    const int32_t mut_count =
        (g->run.act_index >= 2 && (ink_rng_next(&g->rng) & UINT64_C(1)) != 0u) ? 2 : 1;

    uint32_t picked = 0u;
    for (int32_t attempt = 0; attempt < 8; ++attempt) {
        const int32_t i0 = (int32_t)(ink_rng_next(&g->rng) % (uint64_t)pool_n);
        picked = pool[i0];
        if (mut_count == 2 && pool_n >= 2) {
            int32_t i1 = (int32_t)(ink_rng_next(&g->rng) % (uint64_t)(pool_n - 1));
            if (i1 >= i0) {
                ++i1;
            }
            picked |= pool[i1];
        }
        if (picked != g->internal.last_elite_flags) {
            return picked;
        }
    }

    /* Deterministic fallback if random attempts collided with last_elite_flags. */
    for (int32_t i = 0; i < pool_n; ++i) {
        if (mut_count == 1) {
            if (pool[i] != g->internal.last_elite_flags) {
                return pool[i];
            }
        } else {
            for (int32_t j = i + 1; j < pool_n; ++j) {
                const uint32_t combo = pool[i] | pool[j];
                if (combo != g->internal.last_elite_flags) {
                    return combo;
                }
            }
        }
    }
    return picked;
}

static EntityId spawn_wave_enemy(GameState *g, EnemyDefId def_id, int32_t edge_hint)
{
    float x = 0.0f;
    float y = 0.0f;
    pick_spawn_position(g, def_id, edge_hint, &x, &y);
    const EntityId id = enemy_spawn(g, def_id, x, y);
    if (id.index == ECS_INVALID_INDEX) {
        return id;
    }
    record_enemy_intro(g, def_id);
    if (game_is_boss_enemy(def_id)) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_BOSS_ENTER,
                                     .x = x,
                                     .y = y,
                                     .value = (int32_t)def_id,
                                     .flags = 0,
                                 });
    }
    return id;
}

static float effective_spawn_interval(const GameState *g, float base_interval)
{
    const int32_t loop_n = enemy_endless_loop_n(g);
    const float scaled =
        loop_n > 0 ? fmaxf(0.6f, base_interval * powf(0.95f, (float)loop_n)) : base_interval;
    const float mul =
        g->internal.mods.spawn_interval_mul > 0.0f ? g->internal.mods.spawn_interval_mul : 1.0f;
    const float eff = scaled * mul;
    return eff < 0.35f ? 0.35f : eff;
}

int32_t waves_count_alive_escorts(const GameState *g)
{
    if (g == NULL) {
        return 0;
    }
    int32_t escorts = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr != NULL && en != NULL && !tr->dissolving && !game_is_boss_enemy(en->def_id) &&
            !game_is_boss_part(en->def_id)) {
            ++escorts;
        }
    }
    return escorts;
}

EntityId waves_spawn_summoned_escort(GameState *g, EnemyDefId def_id, float x, float y)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || game_is_boss_enemy(def_id) || game_is_boss_part(def_id)) {
        return invalid;
    }
    const int32_t max_escorts = 4 + g->internal.mods.max_alive_add;
    if (waves_count_alive_escorts(g) >= max_escorts) {
        return invalid;
    }
    const EntityId id = enemy_spawn(g, def_id, x, y);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }
    EnemyComp *en = game_enemy_mut(g, id);
    if (en != NULL) {
        en->elite_flags |= ENEMY_FLAG_SUMMONED;
    }
    record_enemy_intro(g, def_id);
    return id;
}

static void clear_transient_field_state(GameState *g)
{
    memset(g->internal.cyclones, 0, sizeof g->internal.cyclones);
    memset(g->internal.maelstrom_bind_t, 0, sizeof g->internal.maelstrom_bind_t);
    memset(g->internal.maelstrom_hit_cd, 0, sizeof g->internal.maelstrom_hit_cd);
    g->internal.wind_telegraph_t = 0.0f;
    g->internal.wind_active_t = 0.0f;
    g->internal.thunderburst_hitstop_cd = 0.0f;
    if (stage_act_index(g->run.act_index) == 2) {
        g->internal.wind_timer = 12.0f - 1.5f;
    }
}

void waves_prepare_act(GameState *g)
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
        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY) || ecs_has_mask(&g->ecs, id, COMP_PROJ) ||
            ecs_has_mask(&g->ecs, id, COMP_INK_ORB) || ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);

    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    clear_ryu_body(g);
    g->hitstop_t = 0.0f;
    g->pending_impulse_count = 0;
    g->internal.boss_vortex_t = 0.0f;
    g->internal.ryu_coil_t = 0.0f;
    g->internal.nue_trail_t[0] = 0.0f;
    g->internal.nue_trail_t[1] = 0.0f;
    clear_transient_field_state(g);

    fluid_reset(&g->fluid);
    stage_gimmicks_setup_act(g, g->run.act_index);
    g->run.rerolls_left = 1 + (int32_t)clamp_reroll_lv(&g->config);
    g->internal.act_prepared = true;
}

void waves_start_wave(GameState *g, int32_t wave_index)
{
    if (g == NULL || wave_index < 0 || wave_index > GAME_BOSS_WAVE_INDEX) {
        return;
    }
    const WaveDef *wdef = &current_act_def(g)->waves[wave_index];

    /* Clear leftover enemies, projectiles, and non-pillar gimmicks before starting cleanly. */
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY) || ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            ecs_destroy(&g->ecs, id);
        } else if (ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            const GimmickComp *gim = game_gimmick(g, id);
            if (gim != NULL && gim->kind != (int32_t)GIMMICK_SPIRIT_PILLAR) {
                ecs_destroy(&g->ecs, id);
            }
        }
    }
    ecs_flush_destroy(&g->ecs);

    const int32_t loop_n = enemy_endless_loop_n(g);
    g->run.wave_index = wave_index;
    g->run.wave_kills = 0;
    g->run.wave_quota =
        wave_index < GAME_BOSS_WAVE_INDEX ? (wdef->quota + 2 * loop_n) : wdef->quota;
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    clear_ryu_body(g);
    g->hitstop_t = 0.0f;
    g->pending_impulse_count = 0;
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 0.0f;
    g->internal.spawn_cd = effective_spawn_interval(g, wdef->spawn_interval);
    g->internal.boss_vortex_t = 0.0f;
    g->internal.wave_elites_spawned = 0;
    g->internal.last_elite_flags = 0u;
    g->internal.nue_trail_t[0] = 0.0f;
    g->internal.nue_trail_t[1] = 0.0f;
    g->internal.ryu_coil_t = 0.0f;
    clear_transient_field_state(g);

    g->player.ink = g->player.ink_max;
    g->player.dry = false;

    int32_t edge = 0;
    for (int32_t e = 0; e < wdef->initial_entries; ++e) {
        const RosterEntry *entry = &wdef->initial[e];
        for (int32_t c = 0; c < entry->count; ++c) {
            (void)spawn_wave_enemy(g, entry->def_id, edge);
            edge = (edge + 1) % 4;
        }
    }
}

void waves_step_spawns(GameState *g)
{
    if (g == NULL || g->run.wave_index < 0 || g->run.wave_index > GAME_BOSS_WAVE_INDEX ||
        game_wave_goal_met(g)) {
        return;
    }
    const WaveDef *wdef = &current_act_def(g)->waves[g->run.wave_index];
    g->internal.spawn_cd -= INK_FIXED_DT;
    if (g->internal.spawn_cd > 1e-4f) {
        return;
    }
    g->internal.spawn_cd = effective_spawn_interval(g, wdef->spawn_interval);

    const bool is_boss_wave = g->run.wave_index == GAME_BOSS_WAVE_INDEX;
    int32_t alive_count = 0;
    int32_t alive_elites = 0;
    int32_t type_counts[ENEMY_DEF_COUNT] = {0};

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
        if ((unsigned)en->def_id < (unsigned)ENEMY_DEF_COUNT) {
            ++type_counts[en->def_id];
        }
        if (en->is_elite) {
            ++alive_elites;
        }
        if (!game_is_boss_part(en->def_id) && (!is_boss_wave || !game_is_boss_enemy(en->def_id))) {
            ++alive_count;
        }
    }
    const int32_t max_alive = wdef->max_alive + g->internal.mods.max_alive_add;
    if (alive_count >= max_alive) {
        return;
    }

    uint64_t total_w = 0;
    uint32_t eff_weights[ENEMY_DEF_COUNT] = {0};
    for (int32_t i = 0; i < (int32_t)ENEMY_DEF_COUNT; ++i) {
        if (wdef->weights[i] == 0u) {
            continue;
        }
        const int32_t cap = per_type_cap(g->run.act_index, (EnemyDefId)i);
        if (cap > 0 && type_counts[i] >= cap) {
            continue;
        }
        eff_weights[i] = wdef->weights[i];
        total_w += eff_weights[i];
    }
    if (total_w == 0) {
        return;
    }

    const uint64_t roll = ink_rng_next(&g->rng) % total_w;
    uint64_t acc = 0;
    EnemyDefId chosen = ENEMY_ARCHER;
    for (int32_t i = 0; i < (int32_t)ENEMY_DEF_COUNT; ++i) {
        acc += eff_weights[i];
        if (roll < acc && eff_weights[i] > 0u) {
            chosen = (EnemyDefId)i;
            break;
        }
    }

    const EntityId spawned = spawn_wave_enemy(g, chosen, -1);
    if (spawned.index == ECS_INVALID_INDEX) {
        return;
    }

    /* Elite mutation roll: normal waves only, excluding Ofuda Fox; endless scales +4n% (cap 30%).
     */
    if (!is_boss_wave && chosen != ENEMY_OFUDA_FOX) {
        const int32_t stage_act = stage_act_index(g->run.act_index);
        const int32_t loop_n = enemy_endless_loop_n(g);
        const float base_prob = stage_act == 0 ? 0.0f : (stage_act == 1 ? 0.08f : 0.12f);
        const float prob = fminf(0.30f, base_prob + 0.04f * (float)loop_n);
        if (prob > 0.0f) {
            const int32_t max_concurrent = (stage_act == 2 || loop_n >= 2) ? 2 : 1;
            const int32_t max_per_wave = (stage_act == 2 || loop_n >= 2) ? 3 : 2;
            if (alive_elites < max_concurrent && g->internal.wave_elites_spawned < max_per_wave &&
                ink_rng_f01(&g->rng) < prob) {
                const uint32_t flags = roll_elite_flags(g, chosen);
                enemy_apply_elite(g, spawned, flags);
                ++g->internal.wave_elites_spawned;
                g->internal.last_elite_flags = flags & ELITE_ALL_MASK;
            }
        }
    }
}
