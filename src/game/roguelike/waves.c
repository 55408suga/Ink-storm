#include "roguelike/waves.h"

#include "core.h"
#include "enemies/enemy_defs.h"

#include <math.h>
#include <stddef.h>

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

static const ActDef k_act1_def = {
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
                            [ENEMY_UMBRELLA] = 0u,
                            [ENEMY_BOSS_TENGU] = 0u,
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
                            [ENEMY_BOSS_TENGU] = 0u,
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
                            [ENEMY_BOSS_TENGU] = 0u,
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
                            [ENEMY_BOSS_TENGU] = 0u,
                        },
                },
        },
};

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

static void spawn_wave_enemy(GameState *g, EnemyDefId def_id, int32_t edge_hint)
{
    float x = 0.0f;
    float y = 0.0f;
    pick_spawn_position(g, def_id, edge_hint, &x, &y);
    const EntityId id = enemy_spawn(g, def_id, x, y);
    if (id.index == ECS_INVALID_INDEX) {
        return;
    }
    const uint32_t bit = UINT32_C(1) << (unsigned)def_id;
    if ((g->internal.seen_enemies_mask & bit) == 0u) {
        g->internal.seen_enemies_mask |= bit;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_ENEMY_INTRO,
                                     .value = (int32_t)def_id,
                                     .flags = 0,
                                 });
    }
    if (def_id == ENEMY_BOSS_TENGU) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_BOSS_ENTER,
                                     .x = x,
                                     .y = y,
                                     .value = (int32_t)ENEMY_BOSS_TENGU,
                                     .flags = 0,
                                 });
    }
}

void waves_prepare_act(GameState *g)
{
    if (g == NULL) {
        return;
    }
    fluid_reset(&g->fluid);
    fluid_clear_solids(&g->fluid);
    g->run.rerolls_left = 1 + (int32_t)clamp_reroll_lv(&g->config);
    g->internal.act_prepared = true;
}

void waves_start_wave(GameState *g, int32_t wave_index)
{
    if (g == NULL || wave_index < 0 || wave_index > GAME_BOSS_WAVE_INDEX) {
        return;
    }
    const WaveDef *wdef = &k_act1_def.waves[wave_index];

    /* Clear any leftover enemies or projectiles before starting the wave cleanly. */
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY) || ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);

    g->run.wave_index = wave_index;
    g->run.wave_kills = 0;
    g->run.wave_quota = wdef->quota;
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 0.0f;
    g->internal.spawn_cd = wdef->spawn_interval;
    g->internal.boss_vortex_t = 0.0f;

    g->player.ink = g->player.ink_max;
    g->player.dry = false;

    int32_t edge = 0;
    for (int32_t e = 0; e < wdef->initial_entries; ++e) {
        const RosterEntry *entry = &wdef->initial[e];
        for (int32_t c = 0; c < entry->count; ++c) {
            spawn_wave_enemy(g, entry->def_id, edge);
            edge = (edge + 1) % 4;
        }
    }
}

void waves_step_spawns(GameState *g)
{
    if (g == NULL || g->run.wave_index < 0 || g->run.wave_index > GAME_BOSS_WAVE_INDEX) {
        return;
    }
    const WaveDef *wdef = &k_act1_def.waves[g->run.wave_index];
    g->internal.spawn_cd -= INK_FIXED_DT;
    if (g->internal.spawn_cd > 1e-4f) {
        return;
    }
    g->internal.spawn_cd = wdef->spawn_interval;

    int32_t alive_enemies = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr != NULL && en != NULL && !tr->dissolving) {
            ++alive_enemies;
        }
    }
    if (alive_enemies >= wdef->max_alive) {
        return;
    }

    uint64_t total_w = 0;
    for (int32_t i = 0; i < (int32_t)ENEMY_DEF_COUNT; ++i) {
        total_w += wdef->weights[i];
    }
    if (total_w == 0) {
        return;
    }
    const uint64_t roll = ink_rng_next(&g->rng) % total_w;
    uint64_t acc = 0;
    EnemyDefId chosen = ENEMY_ARCHER;
    for (int32_t i = 0; i < (int32_t)ENEMY_DEF_COUNT; ++i) {
        acc += wdef->weights[i];
        if (roll < acc && wdef->weights[i] > 0u) {
            chosen = (EnemyDefId)i;
            break;
        }
    }
    spawn_wave_enemy(g, chosen, -1);
}
