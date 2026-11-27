#include "combat/damage.h"
#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "game.h"
#include "roguelike/relics.h"
#include "roguelike/stage_gimmicks.h"
#include "roguelike/waves.h"
#include "unity.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void advance_aftermath(GameState *g)
{
    for (int i = 0; i < 65 && g->phase == RUN_PHASE_WAVE_AFTERMATH; ++i) {
        game_step(g, NULL, NULL);
    }
}

/* A reaction or slam on the winning tick starts a hitstop, and the clear resolves when it ends. */
static void step_until_cleared(GameState *g)
{
    for (int i = 0; i < 10 && g->phase == RUN_PHASE_PLAYING; ++i) {
        game_step(g, NULL, NULL);
    }
}

static void advance_act_transition(GameState *g)
{
    for (int i = 0; i < 130 && g->phase == RUN_PHASE_ACT_TRANSITION; ++i) {
        game_step(g, NULL, NULL);
    }
}

static void assert_boss_alive_kind(const GameState *g, EnemyDefId expected_boss)
{
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, g->run.boss_id.index);
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, g->run.boss_id));
    const EnemyComp *ben = game_enemy(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(ben);
    TEST_ASSERT_EQUAL_INT(expected_boss, ben->def_id);
    const HealthComp *bhp = game_health(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(bhp);
    TEST_ASSERT_GREATER_THAN_INT32(0, bhp->hp);
}

static int32_t count_projectiles(const GameState *g)
{
    int32_t n = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            ++n;
        }
    }
    return n;
}

static void clear_enemies_and_projs(GameState *g)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY) || ecs_has_mask(&g->ecs, id, COMP_PROJ) ||
            ecs_has_mask(&g->ecs, id, COMP_INK_ORB)) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);
    g->internal.spawn_cd = 999.0f;
}

static void test_three_acts_boss_relic_and_transitions(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT32(GAME_MAIN_ACT_COUNT, g->run.act_count);
    TEST_ASSERT_EQUAL_INT32(0, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.relic_count);

    /* Act 1 boss (O-Tengu) defeat -> Shrine HP +3 (capped at max_hp) -> relic 3-choice. */
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    g->shrines[0].hp = g->shrines[0].max_hp - 4;
    game_debug_force_wave_clear(g);
    step_until_cleared(g);

    TEST_ASSERT_EQUAL_INT(RUN_PHASE_WAVE_AFTERMATH, g->phase);
    TEST_ASSERT_EQUAL_INT32(1, g->run.acts_cleared);
    TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp - 1, g->shrines[0].hp);

    advance_aftermath(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_REWARD, g->phase);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);
    TEST_ASSERT_EQUAL_INT32(GAME_MAX_REWARD_CHOICES, g->run.reward.choice_count);
    TEST_ASSERT_FALSE(g->run.reward.repair_available);

    const RelicId picked0 = (RelicId)g->run.reward.choice_ids[0];
    TEST_ASSERT_TRUE((unsigned)picked0 < (unsigned)RELIC_COUNT);
    TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[0], g->run.reward.choice_ids[1]);
    TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[0], g->run.reward.choice_ids[2]);
    TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[1], g->run.reward.choice_ids[2]);

    const GameCommand pick0 = {.type = CMD_PICK_REWARD, .arg = 0};
    game_step(g, NULL, &pick0);

    TEST_ASSERT_EQUAL_INT32(1, g->run.relic_count);
    TEST_ASSERT_TRUE(game_has_relic(g, picked0));
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
    bool saw_relic_pick = false;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_REWARD_PICK && g->events[i].value == (int32_t)picked0 &&
            (g->events[i].flags & EVF_REWARD_RELIC) != 0u) {
            saw_relic_pick = true;
        }
    }
    TEST_ASSERT_TRUE(saw_relic_pick);

    /* Act 2 transition -> Act 2 boss (Nue) defeat -> relic reroll + pick -> Act 3 transition. */
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);

    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_NUE);
    g->shrines[0].hp = g->shrines[0].max_hp - 1;
    game_debug_force_wave_clear(g);
    step_until_cleared(g);
    TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp, g->shrines[0].hp);
    TEST_ASSERT_EQUAL_INT32(2, g->run.acts_cleared);

    advance_aftermath(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_REWARD, g->phase);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);

    const uint32_t serial_before = g->run.reward.serial;
    const GameCommand reroll = {.type = CMD_REROLL_REWARD, .arg = 0};
    game_step(g, NULL, &reroll);
    TEST_ASSERT_EQUAL_UINT32(serial_before + 1u, g->run.reward.serial);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);

    const RelicId picked1 = (RelicId)g->run.reward.choice_ids[0];
    TEST_ASSERT_FALSE(game_has_relic(g, picked1));
    game_step(g, NULL, &pick0);
    TEST_ASSERT_EQUAL_INT32(2, g->run.relic_count);
    TEST_ASSERT_TRUE(game_has_relic(g, picked1));
    TEST_ASSERT_EQUAL_INT32(2, g->run.act_index);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);

    /* Act 3 transition -> Act 3 boss (Ippitsu-Ryu) defeat -> VICTORY. */
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT32(2, g->run.act_index);

    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_RYU);
    game_debug_force_wave_clear(g);
    step_until_cleared(g);
    TEST_ASSERT_EQUAL_INT32(3, g->run.acts_cleared);

    advance_aftermath(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_VICTORY, g->phase);
    TEST_ASSERT_TRUE(game_run_result(g).cleared);

    game_destroy(g);
}

static void test_boss_relic_fallback_advances_act(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    for (int i = 0; i < (int)RELIC_COUNT; ++i) {
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)i));
    }
    TEST_ASSERT_EQUAL_INT32((int32_t)RELIC_COUNT, g->run.relic_count);

    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);
    game_debug_force_wave_clear(g);
    step_until_cleared(g);
    advance_aftermath(g);

    TEST_ASSERT_EQUAL_INT(RUN_PHASE_REWARD, g->phase);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
    TEST_ASSERT_EQUAL_INT32(0, g->run.reward.choice_count);
    TEST_ASSERT_TRUE(g->run.reward.repair_available);

    const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
    game_step(g, NULL, &repair);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);

    game_destroy(g);
}

static void assert_no_solid_within_shrine_120px(const GameState *g)
{
    for (int gy = 1; gy < FLUID_H - 1; ++gy) {
        const float cy = ((float)gy + 0.5f) * FLUID_CELL_PX;
        const float dy = cy - 360.0f;
        for (int gx = 1; gx < FLUID_W - 1; ++gx) {
            const float cx = ((float)gx + 0.5f) * FLUID_CELL_PX;
            const float dx = cx - 640.0f;
            if (dx * dx + dy * dy <= 120.0f * 120.0f) {
                TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, cx, cy));
            }
        }
    }
}

static void test_act2_and_act3_terrain_solid_invariants(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* Act 2: Torii gates + 4 rock pillars. */
    stage_gimmicks_setup_act(g, 1);
    assert_no_solid_within_shrine_120px(g);

    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 384.0f, 328.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 384.0f, 392.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 896.0f, 328.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 896.0f, 392.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 384.0f, 360.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 896.0f, 360.0f));

    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 340.0f, 190.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 940.0f, 190.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 340.0f, 530.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 940.0f, 530.0f));

    /* Act 3: 2 upper rock pillars + 3 Spirit Pillars. */
    stage_gimmicks_setup_act(g, 2);
    assert_no_solid_within_shrine_120px(g);

    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 340.0f, 190.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 940.0f, 190.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 340.0f, 530.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 940.0f, 530.0f));

    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 640.0f, 130.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 441.0f, 475.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 839.0f, 475.0f));

    int32_t pillar_count = 0;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            const GimmickComp *gim = game_gimmick(g, id);
            if (gim != NULL && gim->kind == (int32_t)GIMMICK_SPIRIT_PILLAR) {
                ++pillar_count;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT32(3, pillar_count);

    game_destroy(g);
}

static void test_act3_spirit_pillar_and_mountain_wind(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    game_debug_start_act(g, 2);
    advance_act_transition(g);
    clear_enemies_and_projs(g);

    /* Find top spirit pillar at (640, 130). */
    EntityId top_pillar = {ECS_INVALID_INDEX, 0};
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const GimmickComp *gim = game_gimmick(g, id);
        if (tr != NULL && gim != NULL && gim->kind == (int32_t)GIMMICK_SPIRIT_PILLAR &&
            fabsf(tr->x - 640.0f) < 2.0f && fabsf(tr->y - 130.0f) < 2.0f) {
            top_pillar = id;
            break;
        }
    }
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, top_pillar.index);

    /* Write fire = 0.5 into adjacent non-solid cell (640, 152) -> distance 22px in (16, 28]. */
    float *fire_ch = fluid_channel_mut(&g->fluid, 1);
    const int adj_x = (int)(640.0f / FLUID_CELL_PX);
    const int adj_y = (int)(152.0f / FLUID_CELL_PX);
    TEST_ASSERT_EQUAL_UINT8(0u, g->fluid.solid[adj_y * FLUID_W + adj_x]);
    fire_ch[adj_y * FLUID_W + adj_x] = 0.55f;

    stage_gimmicks_step(g);
    const GimmickComp *pgim = game_gimmick(g, top_pillar);
    TEST_ASSERT_NOT_NULL(pgim);
    TEST_ASSERT_EQUAL_INT((int)GIMMICK_ELEM_FIRE, pgim->elem_kind);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f, pgim->timer);

    /* Clear ambient fire and verify a 0.5s (30 ticks) pulse increases surrounding fire. */
    fluid_reset(&g->fluid);
    for (int t = 0; t < 29; ++t) {
        stage_gimmicks_step(g);
    }
    const float fire_before_pulse = fluid_channel_const(&g->fluid, 1)[adj_y * FLUID_W + adj_x];
    stage_gimmicks_step(g);
    const float fire_after_pulse = fluid_channel_const(&g->fluid, 1)[adj_y * FLUID_W + adj_x];
    TEST_ASSERT_TRUE(fire_after_pulse > fire_before_pulse + 0.05f);

    /* Switch to Storm while lit by bathing adjacent cell with storm = 0.6. */
    fluid_reset(&g->fluid);
    float *storm_ch = fluid_channel_mut(&g->fluid, 2);
    storm_ch[adj_y * FLUID_W + adj_x] = 0.65f;
    stage_gimmicks_step(g);
    TEST_ASSERT_EQUAL_INT((int)GIMMICK_ELEM_STORM, pgim->elem_kind);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f, pgim->timer);

    /* Mountain wind: advance until EV_WIND_TELEGRAPH fires, then 90 ticks later verify upwind band
     * velocity. */
    bool saw_telegraph = false;
    int32_t wind_dir = -1;
    for (int t = 0; t < 700; ++t) {
        game_step(g, NULL, NULL);
        TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));
        for (int32_t e = 0; e < g->event_count; ++e) {
            if (g->events[e].type == EV_WIND_TELEGRAPH) {
                saw_telegraph = true;
                wind_dir = g->events[e].value;
                break;
            }
        }
        if (saw_telegraph) {
            break;
        }
    }
    TEST_ASSERT_TRUE(saw_telegraph);
    TEST_ASSERT_TRUE(wind_dir >= 0 && wind_dir < 4);

    /* Advance 90 ticks (1.5s telegraph) + 15 ticks into active blow. */
    for (int t = 0; t < 105; ++t) {
        game_step(g, NULL, NULL);
        TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));
    }
    TEST_ASSERT_TRUE(g->internal.wind_active_t > 0.0f);

    float sample_u = 0.0f;
    float sample_v = 0.0f;
    if (wind_dir == 0) {
        fluid_sample(&g->fluid, 80.0f, 360.0f, &sample_u, &sample_v, NULL);
        TEST_ASSERT_TRUE(sample_u > 3.0f);
    } else if (wind_dir == 1) {
        fluid_sample(&g->fluid, (float)INK_SCREEN_W - 80.0f, 360.0f, &sample_u, &sample_v, NULL);
        TEST_ASSERT_TRUE(sample_u < -3.0f);
    } else if (wind_dir == 2) {
        fluid_sample(&g->fluid, 640.0f, 80.0f, &sample_u, &sample_v, NULL);
        TEST_ASSERT_TRUE(sample_v > 3.0f);
    } else {
        fluid_sample(&g->fluid, 640.0f, (float)INK_SCREEN_H - 80.0f, &sample_u, &sample_v, NULL);
        TEST_ASSERT_TRUE(sample_v < -3.0f);
    }

    game_destroy(g);
}

static void test_new_normal_enemies_and_projectiles(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 1);
    advance_act_transition(g);
    clear_enemies_and_projs(g);

    /* 1. Storm Lantern: fires PROJ_STORM_ORB and splashes storm on death. */
    fluid_reset(&g->fluid);
    const EntityId sl = game_debug_spawn_enemy(g, ENEMY_LANTERN_STORM, 100.0f, 300.0f);
    EnemyComp *slen = game_enemy_mut(g, sl);
    TEST_ASSERT_NOT_NULL(slen);
    slen->fire_cd = 0.01f;
    game_step(g, NULL, NULL);

    bool saw_storm_orb = false;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            const ProjComp *p = game_proj(g, id);
            if (p != NULL && p->kind == PROJ_STORM_ORB) {
                saw_storm_orb = true;
            }
        }
    }
    TEST_ASSERT_TRUE(saw_storm_orb);

    fluid_reset(&g->fluid);
    (void)combat_apply_damage(g, sl, 50, DMG_PHYSICAL, 0.0f, 0.0f);
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 100.0f, 300.0f, NULL, NULL, dye);
    TEST_ASSERT_TRUE(dye[2] > 0.5f);
    ecs_flush_destroy(&g->ecs);

    /* 2. Kamaitachi: fires PROJ_WIND_SICKLE (coupling=1.5) & spawns GIMMICK_KAMAITACHI_VORTEX. */
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);
    const EntityId km = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 460.0f, 200.0f);
    EnemyComp *kmen = game_enemy_mut(g, km);
    TEST_ASSERT_NOT_NULL(kmen);
    kmen->fire_cd = 0.01f;
    kmen->absorbed_ink = 0.01f;
    game_step(g, NULL, NULL);

    bool saw_sickle = false;
    bool saw_vortex = false;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            const ProjComp *p = game_proj(g, id);
            const VelocityComp *v = game_velocity(g, id);
            if (p != NULL && v != NULL && p->kind == PROJ_WIND_SICKLE) {
                saw_sickle = true;
                TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.5f, v->fluid_coupling);
            }
        } else if (ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            const GimmickComp *gim = game_gimmick(g, id);
            if (gim != NULL && gim->kind == (int32_t)GIMMICK_KAMAITACHI_VORTEX) {
                saw_vortex = true;
            }
        }
    }
    TEST_ASSERT_TRUE(saw_sickle);
    TEST_ASSERT_TRUE(saw_vortex);

    /* Step twice so Step 6 vortex injection is reflected in Step 4 fluid_step stats. */
    game_step(g, NULL, NULL);
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(g->fluid.stats.max_vel > 1.0f);

    /* 3. Stone Oni: 2 shrine damage + retreat, and 80 slam damage + 1.0s stun. */
    clear_enemies_and_projs(g);
    const int32_t hp_before = g->shrines[0].hp;
    const EntityId oni = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 640.0f + 30.0f, 360.0f);
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(hp_before - 2, g->shrines[0].hp);
    const EnemyComp *onien = game_enemy(g, oni);
    TEST_ASSERT_NOT_NULL(onien);
    TEST_ASSERT_EQUAL_INT32(1, onien->ai_state);

    /* Slam Stone Oni into Act 2 rock pillar at (940, 190). */
    fluid_reset(&g->fluid);
    TransformComp *onitr = game_transform_mut(g, oni);
    VelocityComp *onivel = game_velocity_mut(g, oni);
    StatusComp *onist = game_status_mut(g, oni);
    const HealthComp *onihp = game_health(g, oni);
    TEST_ASSERT_NOT_NULL(onitr);
    TEST_ASSERT_NOT_NULL(onivel);
    TEST_ASSERT_NOT_NULL(onist);
    TEST_ASSERT_NOT_NULL(onihp);
    onitr->x = 914.0f;
    onitr->y = 190.0f;
    onivel->vx = 360.0f;
    onivel->vy = 0.0f;
    onist->knockback_t = 1.0f;
    onist->ink_mark_t = 0.0f;
    const int32_t oni_hp0 = onihp->hp;
    for (int t = 0; t < 4 && onihp->hp == oni_hp0; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT32(oni_hp0 - 80, onihp->hp);
    TEST_ASSERT_TRUE(onist->stun_t >= 0.90f);

    /* 4. Ink Leech: absorbs ink, heals HP, swells radius, takes extra fire dmg, bursts ink on kill.
     */
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);
    const EntityId leech = game_debug_spawn_enemy(g, ENEMY_INK_LEECH, 500.0f, 200.0f);
    HealthComp *lhp = game_health_mut(g, leech);
    const TransformComp *ltr = game_transform(g, leech);
    const EnemyComp *len = game_enemy(g, leech);
    TEST_ASSERT_NOT_NULL(lhp);
    TEST_ASSERT_NOT_NULL(ltr);
    TEST_ASSERT_NOT_NULL(len);
    lhp->hp = 20;
    const float init_rad = ltr->radius;
    const float pool_dye[FLUID_MAX_CHANNELS] = {2.2f, 0.0f, 0.0f, 0.0f};
    for (int t = 0; t < 75; ++t) {
        fluid_splat(&g->fluid, ltr->x, ltr->y, 8.0f, 0.0f, 0.0f, pool_dye);
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(len->absorbed_ink > 1.0f);
    TEST_ASSERT_GREATER_THAN_INT32(20, lhp->hp);
    TEST_ASSERT_TRUE(ltr->radius > init_rad + 1.0f);

    /* Verify fire damage bonus scales up (> base 10) and death releases dense ink. */
    StatusComp *lst = game_status_mut(g, leech);
    lst->ink_mark_t = 0.0f;
    const int32_t fire_dmg = combat_apply_damage(g, leech, 10, DMG_FIRE, 0.0f, 0.0f);
    TEST_ASSERT_GREATER_THAN_INT32(10, fire_dmg);
    const float lx = ltr->x;
    const float ly = ltr->y;
    fluid_reset(&g->fluid);
    (void)combat_apply_damage(g, leech, 500, DMG_PHYSICAL, 0.0f, 0.0f);
    fluid_sample(&g->fluid, lx, ly, NULL, NULL, dye);
    TEST_ASSERT_TRUE(dye[0] > 0.7f);
    ecs_flush_destroy(&g->ecs);

    /* 5. PROJ_BOUNCE_ORB: bounces twice off outer wall/solid without self-deflecting, dies on 3rd.
     */
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);
    /* Place bounce orb moving left into the left outer wall from x=20, y=200. */
    const EntityId bo =
        game_debug_spawn_projectile(g, PROJ_BOUNCE_ORB, 20.0f, 200.0f, -230.0f, 0.0f);
    const ProjComp *bp = game_proj(g, bo);
    VelocityComp *bv = game_velocity_mut(g, bo);
    TransformComp *bt = game_transform_mut(g, bo);
    TEST_ASSERT_NOT_NULL(bp);
    TEST_ASSERT_EQUAL_INT32(2, bp->bounces_left);

    /* Bounce 1 off left wall (x < 8). */
    for (int t = 0; t < 10 && bp->bounces_left == 2; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, bo));
    TEST_ASSERT_EQUAL_INT32(1, bp->bounces_left);
    TEST_ASSERT_TRUE(bv->vx > 0.0f);
    TEST_ASSERT_FALSE(bp->deflected); /* Must not self-deflect without fluid flow! */

    /* Bounce 2 off Act 2 rock pillar at (340, 190). */
    bt->x = 308.0f;
    bt->y = 190.0f;
    bv->vx = 230.0f;
    bv->vy = 0.0f;
    for (int t = 0; t < 10 && bp->bounces_left == 1; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, bo));
    TEST_ASSERT_EQUAL_INT32(0, bp->bounces_left);
    TEST_ASSERT_FALSE(bp->deflected);

    /* 3rd impact destroys it. */
    bt->x = 14.0f;
    bt->y = 200.0f;
    bv->vx = -230.0f;
    bv->vy = 0.0f;
    for (int t = 0; t < 10 && ecs_is_alive(&g->ecs, bo); ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, bo));

    game_destroy(g);
}

static void test_elite_mutations_and_summoned_exclusion(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 1);
    advance_act_transition(g);
    clear_enemies_and_projs(g);

    /* 1. Common elite stats: HP x2.5, radius x1.2, 5 orbs, score x3. */
    const EntityId e0 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 250.0f, 200.0f);
    enemy_apply_elite(g, e0, ELITE_RIGID);
    const HealthComp *hp0 = game_health(g, e0);
    const TransformComp *tr0 = game_transform(g, e0);
    const VelocityComp *vel0 = game_velocity(g, e0);
    const EnemyComp *en0 = game_enemy(g, e0);
    TEST_ASSERT_EQUAL_INT32(75, hp0->max_hp); /* 30 * 2.5 */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.0f * 1.2f, tr0->radius);
    TEST_ASSERT_EQUAL_INT(MASS_MEDIUM, vel0->mass_tier); /* LIGHT -> MEDIUM */
    TEST_ASSERT_TRUE(en0->is_elite);

    /* Rigid slam resistance: -50%. */
    const int32_t rigid_slam = combat_apply_damage(g, e0, 60, DMG_PHYSICAL | DMG_SLAM, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(30, rigid_slam);

    /* Kill elite -> score x3 (180*3=540) and 5 orbs spawned. */
    g->run.score = 0;
    g->run.chain = 0;
    g->run.chain_t = 0.0f;
    (void)combat_apply_damage(g, e0, 500, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(540, g->run.score);
    int32_t orb_n = 0;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_INK_ORB)) {
            ++orb_n;
        }
    }
    TEST_ASSERT_EQUAL_INT32(5, orb_n);
    clear_enemies_and_projs(g);

    /* Turret stripped of ELITE_RIGID. */
    const EntityId turret = game_debug_spawn_enemy(g, ENEMY_ARCHER, 100.0f, 200.0f);
    enemy_apply_elite(g, turret, ELITE_RIGID | ELITE_FIREPROOF);
    TEST_ASSERT_EQUAL_UINT32(ELITE_FIREPROOF, game_enemy(g, turret)->elite_flags & ELITE_ALL_MASK);
    clear_enemies_and_projs(g);

    /* 2. ELITE_INK_REPEL: immune to ink mark + decays surrounding ink. */
    fluid_reset(&g->fluid);
    const EntityId repel = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 500.0f, 200.0f);
    enemy_apply_elite(g, repel, ELITE_INK_REPEL);
    const float ink_dye[FLUID_MAX_CHANNELS] = {2.0f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, 500.0f, 200.0f, 5.0f, 0.0f, 0.0f, ink_dye);
    float dye_before[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 500.0f, 200.0f, NULL, NULL, dye_before);
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, game_status(g, repel)->ink_mark_t);
    float dye_after[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 500.0f, 200.0f, NULL, NULL, dye_after);
    TEST_ASSERT_TRUE(dye_after[0] < dye_before[0]);
    clear_enemies_and_projs(g);

    /* 3. ELITE_FIREPROOF / ELITE_STORMPROOF & dual-element lower-resistance rule. */
    const EntityId fp = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 500.0f, 200.0f);
    enemy_apply_elite(g, fp, ELITE_FIREPROOF);
    TEST_ASSERT_EQUAL_INT32(30, combat_apply_damage(g, fp, 100, DMG_FIRE, 0.0f, 0.0f));
    TEST_ASSERT_EQUAL_INT32(100, combat_apply_damage(g, fp, 100, DMG_FIRE | DMG_STORM, 0.0f, 0.0f));

    const EntityId sp = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 600.0f, 200.0f);
    enemy_apply_elite(g, sp, ELITE_STORMPROOF);
    TEST_ASSERT_EQUAL_INT32(30, combat_apply_damage(g, sp, 100, DMG_STORM, 0.0f, 0.0f));

    const EntityId both = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 700.0f, 200.0f);
    enemy_apply_elite(g, both, ELITE_FIREPROOF | ELITE_STORMPROOF);
    TEST_ASSERT_EQUAL_INT32(30,
                            combat_apply_damage(g, both, 100, DMG_FIRE | DMG_STORM, 0.0f, 0.0f));

    /* 4. Summoned escort kill does NOT count toward wave_kills. */
    clear_enemies_and_projs(g);
    g->run.wave_index = 0;
    g->run.wave_kills = 0;
    const int32_t kills_before = g->run.kills;
    const EntityId sum = waves_spawn_summoned_escort(g, ENEMY_KAMAITACHI, 300.0f, 200.0f);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, sum.index);
    (void)combat_apply_damage(g, sum, 500, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(kills_before + 1, g->run.kills);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_kills);

    game_destroy(g);
}

static void test_boss_nue_and_boss_ryu(void)
{
    /* --- Act 2 Boss: Nue --- */
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 1);
    advance_act_transition(g);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_NUE);

    const EntityId nue = g->run.boss_id;
    TransformComp *ntr = game_transform_mut(g, nue);
    VelocityComp *nvel = game_velocity_mut(g, nue);
    StatusComp *nst = game_status_mut(g, nue);
    EnemyComp *nen = game_enemy_mut(g, nue);
    const HealthComp *nhp = game_health(g, nue);
    const BossComp *nboss = game_boss(g, nue);
    TEST_ASSERT_EQUAL_INT32(700, nhp->max_hp);

    /* Dash leaves lightning trail; slamming during dash is immune. */
    nen->ai_state = 2; /* dashing */
    nen->fire_cd = 1.0f;
    ntr->x = 910.0f;
    ntr->y = 190.0f;
    nvel->vx = 360.0f;
    nvel->vy = 0.0f;
    nst->knockback_t = 1.0f;
    const int32_t hp_dash_before = nhp->hp;
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(hp_dash_before, nhp->hp);
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 910.0f, 190.0f, NULL, NULL, dye);
    TEST_ASSERT_TRUE(dye[2] > 0.2f);

    /* Stopped (ai_state == 1): slamming into rock pillar deals 60 dmg + 1.0s stun. */
    fluid_reset(&g->fluid);
    g->internal.nue_trail_t[0] = 0.0f;
    g->internal.nue_trail_t[1] = 0.0f;
    nen->ai_state = 1;
    nen->fire_cd = 1.5f;
    ntr->x = 914.0f;
    ntr->y = 190.0f;
    nvel->vx = 360.0f;
    nvel->vy = 0.0f;
    nst->knockback_t = 1.0f;
    nst->ink_mark_t = 0.0f;
    const int32_t hp_stop_before = nhp->hp;
    for (int t = 0; t < 4 && nhp->hp == hp_stop_before; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT32(hp_stop_before - 60, nhp->hp);
    TEST_ASSERT_TRUE(nst->stun_t >= 0.90f);

    /* Drop HP below 50% -> EV_BOSS_PHASE & phase_idx == 1. */
    g->event_count = 0;
    (void)combat_apply_damage(g, nue, 360, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(1, nboss->phase_idx);
    bool saw_nue_phase = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PHASE && g->events[e].value == 2) {
            saw_nue_phase = true;
        }
    }
    TEST_ASSERT_TRUE(saw_nue_phase);
    game_destroy(g);

    /* --- Act 3 Boss: Ippitsu-Ryu (Multi-Part 3-Phase ECS) --- */
    g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 2);
    advance_act_transition(g);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_RYU);

    const EntityId ryu = g->run.boss_id;
    HealthComp *rhp = game_health_mut(g, ryu);
    const TransformComp *rtr = game_transform(g, ryu);
    const BossComp *rboss = game_boss(g, ryu);
    EnemyComp *ren = game_enemy_mut(g, ryu);
    TEST_ASSERT_EQUAL_INT32(900, rhp->max_hp);
    TEST_ASSERT_EQUAL_INT32(0, rboss->phase_idx);
    TEST_ASSERT_EQUAL_UINT32(ryu.index, g->ryu_body.head_id.index);
    TEST_ASSERT_EQUAL_UINT8(0u, g->ryu_body.orb_broken_mask);
    TEST_ASSERT_EQUAL_INT32(0, g->ryu_body.active_orb_slot);
    TEST_ASSERT_EQUAL_INT32(80, g->ryu_body.trail_count);

    /* Phase 1: Head is untargetable and takes 0 damage; 3 Dragon Orbs placed at 0/450/900px. */
    TEST_ASSERT_FALSE(game_enemy_is_targetable(g, ryu));
    TEST_ASSERT_EQUAL_INT32(0, combat_apply_damage(g, ryu, 100, DMG_PHYSICAL, 0.0f, 0.0f));
    TEST_ASSERT_EQUAL_INT32(900, rhp->hp);

    static const float k_expected_arc[GAME_RYU_ORB_COUNT] = {0.0f, 450.0f, 900.0f};
    EntityId orbs[GAME_RYU_ORB_COUNT];
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        orbs[s] = g->ryu_body.orb_ids[s];
        TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, orbs[s].index);
        TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, orbs[s]));
        TEST_ASSERT_TRUE(game_enemy_is_targetable(g, orbs[s]));
        const EnemyComp *oen = game_enemy(g, orbs[s]);
        const HealthComp *ohp = game_health(g, orbs[s]);
        const BossComp *oboss = game_boss(g, orbs[s]);
        const TransformComp *otr = game_transform(g, orbs[s]);
        TEST_ASSERT_NOT_NULL(oen);
        TEST_ASSERT_NOT_NULL(ohp);
        TEST_ASSERT_NOT_NULL(oboss);
        TEST_ASSERT_NOT_NULL(otr);
        TEST_ASSERT_EQUAL_INT(ENEMY_BOSS_RYU_ORB, oen->def_id);
        TEST_ASSERT_EQUAL_INT32(300, ohp->max_hp);
        TEST_ASSERT_EQUAL_INT32(s, oboss->part_id);
        TEST_ASSERT_EQUAL_UINT32(ryu.index, oboss->parent_id.index);
        TEST_ASSERT_FLOAT_WITHIN(4.0f, k_expected_arc[s], hypotf(rtr->x - otr->x, rtr->y - otr->y));
    }

    /* Fire Orb (slot 0) attacks with 2.2s CD and rotates active_orb_slot to Storm Orb (slot 1). */
    ren->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.2f, ren->fire_cd);
    TEST_ASSERT_EQUAL_INT32(1, g->ryu_body.active_orb_slot);

    /* Storm Orb (slot 1) spawns lightning strikes and does not self-trigger Cataclysm/Thunderburst.
     */
    ren->fire_cd = 0.01f;
    for (int t = 0; t < 6; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT32(0, g->run.cataclysms);
    TEST_ASSERT_EQUAL_INT32(0, g->run.thunderbursts);
    int32_t strike_n = 0;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            const GimmickComp *gim = game_gimmick(g, id);
            if (gim != NULL && gim->kind == (int32_t)GIMMICK_LIGHTNING_STRIKE) {
                ++strike_n;
            }
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, strike_n);

    /* Test Lightning Strike rod with ink > 0.3 vs no ink hitting shrine (must zap targetable orb).
     */
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    fluid_reset(&g->fluid);
    const EntityId rod_strike = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
    game_transform_mut(g, rod_strike)->x = sx;
    game_transform_mut(g, rod_strike)->y = sy;
    GimmickComp *rgim = game_gimmick_mut(g, rod_strike);
    rgim->kind = (int32_t)GIMMICK_LIGHTNING_STRIKE;
    rgim->timer = INK_FIXED_DT * 0.5f;
    const float rod_ink[FLUID_MAX_CHANNELS] = {1.0f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, sx, sy, 5.0f, 0.0f, 0.0f, rod_ink);
    const int32_t shrine_hp0 = g->shrines[0].hp;
    const int32_t zaps0 = g->run.storm_zaps;
    stage_gimmicks_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_INT32(shrine_hp0, g->shrines[0].hp);
    TEST_ASSERT_GREATER_THAN_INT32(zaps0, g->run.storm_zaps);
    TEST_ASSERT_EQUAL_INT32(900, rhp->hp); /* Head stayed unharmed */

    /* Without ink, lightning strike inside shrine damages shrine by 1. */
    fluid_reset(&g->fluid);
    const EntityId hit_strike = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
    game_transform_mut(g, hit_strike)->x = sx;
    game_transform_mut(g, hit_strike)->y = sy;
    GimmickComp *hgim = game_gimmick_mut(g, hit_strike);
    hgim->kind = (int32_t)GIMMICK_LIGHTNING_STRIKE;
    hgim->timer = INK_FIXED_DT * 0.5f;
    g->shrines[0].invuln_t = 0.0f;
    stage_gimmicks_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_INT32(shrine_hp0 - 1, g->shrines[0].hp);

    /* Destroy active Orb 0 (Fire) late in its telegraph -> EV_BOSS_PART_BREAK(value=0), mask=0x1,
     * replacement orb gets full >= 0.65s warning (fire_cd reset >= 0.75s), then CD scales x0.85. */
    g->ryu_body.active_orb_slot = 0;
    ren->fire_cd = 0.05f;
    g->event_count = 0;
    (void)combat_apply_damage(g, orbs[0], 500, DMG_PHYSICAL, 0.0f, 0.0f);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_UINT8(0x1u, g->ryu_body.orb_broken_mask);
    TEST_ASSERT_EQUAL_INT32(1, g->ryu_body.active_orb_slot);
    TEST_ASSERT_TRUE(ren->fire_cd >= 0.75f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, game_boss(g, orbs[1])->telegraph);
    bool saw_break0 = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PART_BREAK && g->events[e].value == 0) {
            saw_break0 = true;
        }
    }
    TEST_ASSERT_TRUE(saw_break0);
    /* Next tick must not immediately fire from replacement orb; it counts down full warning. */
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(1, g->ryu_body.active_orb_slot);
    TEST_ASSERT_TRUE(ren->fire_cd > 0.65f);
    ren->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.2f * 0.85f, ren->fire_cd);

    /* Destroy Orb 1 (Storm) -> EV_BOSS_PART_BREAK(value=1), mask=0x3, attack CD scales x0.85^2. */
    g->event_count = 0;
    (void)combat_apply_damage(g, orbs[1], 500, DMG_PHYSICAL, 0.0f, 0.0f);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_UINT8(0x3u, g->ryu_body.orb_broken_mask);
    bool saw_break1 = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PART_BREAK && g->events[e].value == 1) {
            saw_break1 = true;
        }
    }
    TEST_ASSERT_TRUE(saw_break1);
    ren->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.2f * 0.85f * 0.85f, ren->fire_cd);

    /* The earlier on-screen orb volleys are real fire/storm sources that may legitimately react,
     * and breaking a third orb in one chain adds chain lightning storm onto its death splash (a
     * genuine Cataclysm). Clear both so only what the awakening itself injects is under test. */
    g->run.chain = 0;
    g->run.chain_t = 0.0f;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId pid = ecs_entity_at(&g->ecs, i);
        if (pid.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, pid, COMP_PROJ)) {
            ecs_destroy(&g->ecs, pid);
        }
    }
    ecs_flush_destroy(&g->ecs);
    memset(fluid_channel_mut(&g->fluid, 1), 0, sizeof(float) * FLUID_N);
    memset(fluid_channel_mut(&g->fluid, 2), 0, sizeof(float) * FLUID_N);
    const int32_t cataclysms_before = g->run.cataclysms;
    const int32_t thunderbursts_before = g->run.thunderbursts;

    /* Destroy Orb 2 (Wind) late in countdown -> Phase 2 Awakening (EV_BOSS_PHASE value=2, head
     * targetable, wind 6s, head fire_cd clamped >= 0.90s so post-invuln attack keeps full warning).
     */
    ren->fire_cd = 0.05f;
    g->event_count = 0;
    (void)combat_apply_damage(g, orbs[2], 500, DMG_PHYSICAL, 0.0f, 0.0f);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_UINT8(0x7u, g->ryu_body.orb_broken_mask);
    TEST_ASSERT_EQUAL_INT32(-1, g->ryu_body.active_orb_slot);
    TEST_ASSERT_EQUAL_INT32(1, rboss->phase_idx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.5f, rhp->invuln_t);
    TEST_ASSERT_TRUE(ren->fire_cd >= 0.90f);
    TEST_ASSERT_TRUE(game_enemy_is_targetable(g, ryu));
    bool saw_break2 = false;
    bool saw_ryu_p2 = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PART_BREAK && g->events[e].value == 2) {
            saw_break2 = true;
        }
        if (g->events[e].type == EV_BOSS_PHASE && g->events[e].value == 2) {
            saw_ryu_p2 = true;
        }
    }
    TEST_ASSERT_TRUE(saw_break2);
    TEST_ASSERT_TRUE(saw_ryu_p2);

    /* Boss awakening swirl must not self-trigger Cataclysm or Thunderburst; mountain wind clamps to
     * 6s period (wind_timer <= 1.5s). Counters were sampled before the last orb broke, so any fire
     * or storm the awakening itself injects is still on the field here. */
    g->internal.wind_timer = 7.0f;
    g->internal.wind_telegraph_t = 0.0f;
    g->internal.wind_active_t = 0.0f;
    for (int t = 0; t < 6; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT32(cataclysms_before, g->run.cataclysms);
    TEST_ASSERT_EQUAL_INT32(thunderbursts_before, g->run.thunderbursts);
    TEST_ASSERT_TRUE(g->internal.wind_timer <= 1.5f);

    /* Let the full 1.5s Phase 2 invuln expire naturally, then verify the head counts down from
     * >= 0.90s without immediately firing (fire_cd decreases rather than resetting to 2.2s). */
    g->internal.spawn_cd = 999.0f;
    g->internal.ryu_coil_t = 0.0f;
    int invuln_ticks = 0;
    while (rhp->invuln_t > 0.0f && invuln_ticks < 100) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        ++invuln_ticks;
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, rhp->invuln_t);
    TEST_ASSERT_TRUE(invuln_ticks >= 80 && invuln_ticks <= 95);
    const float p2_cd_before = ren->fire_cd;
    TEST_ASSERT_TRUE(p2_cd_before >= 0.85f && p2_cd_before <= 0.90f);
    const int32_t p2_proj_before = count_projectiles(g);
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(ren->fire_cd < p2_cd_before);
    TEST_ASSERT_TRUE(ren->fire_cd >= 0.80f && ren->fire_cd <= 0.90f);
    TEST_ASSERT_EQUAL_INT32(p2_proj_before, count_projectiles(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, g->internal.ryu_coil_t);

    /* Phase 2 Cataclysm damage is 1x (80, NOT 160). */
    game_status_mut(g, ryu)->ink_mark_t = 0.0f;
    const int32_t p2_cata =
        combat_apply_damage(g, ryu, 80, DMG_FIRE | DMG_STORM | DMG_CATACLYSM, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(80, p2_cata);

    /* Drop Dragon Eye HP <= 40% (<= 360) late in countdown -> Phase 3 Ascension (EV_BOSS_PHASE
     * value=3, head fire_cd clamped >= 0.90s for full Breath/Coil warning after 1.5s invuln). */
    ren->fire_cd = 0.05f;
    g->event_count = 0;
    (void)combat_apply_damage(g, ryu, 460, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(360, rhp->hp);
    TEST_ASSERT_EQUAL_INT32(2, rboss->phase_idx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.5f, rhp->invuln_t);
    TEST_ASSERT_TRUE(ren->fire_cd >= 0.90f);
    bool saw_ryu_p3 = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PHASE && g->events[e].value == 3) {
            saw_ryu_p3 = true;
        }
    }
    TEST_ASSERT_TRUE(saw_ryu_p3);

    /* Let the full 1.5s Phase 3 invuln expire naturally, then verify no immediate attack (fire_cd
     * decreases from ~0.90s rather than resetting to 1.76s). */
    g->internal.spawn_cd = 999.0f;
    g->internal.ryu_coil_t = 0.0f;
    invuln_ticks = 0;
    while (rhp->invuln_t > 0.0f && invuln_ticks < 100) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        ++invuln_ticks;
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, rhp->invuln_t);
    TEST_ASSERT_TRUE(invuln_ticks >= 80 && invuln_ticks <= 95);
    const float p3_cd_before = ren->fire_cd;
    TEST_ASSERT_TRUE(p3_cd_before >= 0.85f && p3_cd_before <= 0.90f);
    const int32_t p3_proj_before = count_projectiles(g);
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(ren->fire_cd < p3_cd_before);
    TEST_ASSERT_TRUE(ren->fire_cd >= 0.80f && ren->fire_cd <= 0.90f);
    TEST_ASSERT_EQUAL_INT32(p3_proj_before, count_projectiles(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, g->internal.ryu_coil_t);

    /* Phase 3 Cataclysm damage x2: 80 -> 160; attack CD x0.8 (1.76s). */
    game_status_mut(g, ryu)->ink_mark_t = 0.0f;
    const int32_t p3_cata =
        combat_apply_damage(g, ryu, 80, DMG_FIRE | DMG_STORM | DMG_CATACLYSM, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(160, p3_cata);

    ren->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.2f * 0.8f, ren->fire_cd);

    game_destroy(g);
}

static void test_ofuda_fox_barrier_homing_and_act3_roster(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* 1. Act 3 Wave 2 (3-2) initial roster includes 1 Ofuda Fox. */
    game_debug_start_act(g, 2);
    advance_act_transition(g);
    game_debug_start_wave(g, 1);

    int32_t fox_n = 0;
    EntityId fox_id = {ECS_INVALID_INDEX, 0};
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const EnemyComp *en = game_enemy(g, id);
        if (en != NULL && en->def_id == ENEMY_OFUDA_FOX) {
            ++fox_n;
            fox_id = id;
        }
    }
    TEST_ASSERT_EQUAL_INT32(1, fox_n);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, fox_id.index);

    /* Elite exclusion: enemy_apply_elite must not turn Ofuda Fox into an elite. */
    enemy_apply_elite(g, fox_id, ELITE_RIGID | ELITE_FIREPROOF);
    TEST_ASSERT_FALSE(game_enemy(g, fox_id)->is_elite);

    /* Concurrent cap 1: spawning many Act 3 reinforcements with 1 Fox alive never spawns a 2nd Fox.
     */
    g->internal.mods.max_alive_add = 20;
    for (int t = 0; t < 40; ++t) {
        g->internal.spawn_cd = 0.0f;
        waves_step_spawns(g);
    }
    int32_t fox_after = 0;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        const EnemyComp *en = (id.index != ECS_INVALID_INDEX) ? game_enemy(g, id) : NULL;
        if (en != NULL && en->def_id == ENEMY_OFUDA_FOX) {
            ++fox_after;
            TEST_ASSERT_FALSE(en->is_elite);
        }
    }
    TEST_ASSERT_EQUAL_INT32(1, fox_after);

    /* 2. Ofuda Barrier: Fox grants 4.0s barrier (-50% dmg) to nearby ally within 360px. */
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);
    const EntityId fox = game_debug_spawn_enemy(g, ENEMY_OFUDA_FOX, 300.0f, 360.0f);
    const EntityId ally = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 420.0f, 360.0f);
    EnemyComp *fen = game_enemy_mut(g, fox);
    TEST_ASSERT_NOT_NULL(fen);
    fen->absorbed_ink = 0.01f;
    fen->fire_cd = 5.0f;
    game_step(g, NULL, NULL);

    const StatusComp *ast = game_status(g, ally);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 4.0f, ast->barrier_t);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 6.0f, fen->absorbed_ink);

    /* Shielded ally takes -50% damage (40 -> 20). */
    game_status_mut(g, ally)->ink_mark_t = 0.0f;
    const int32_t shielded_dmg = combat_apply_damage(g, ally, 40, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(20, shielded_dmg);

    /* 3. Homing Ofuda: 140px/s floor exception, 90 deg/s homing steer, burns in fire > 0.30,
     *    and deals 20 physical damage after deflection. */
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    /* Spawn 30 deg off-axis from shrine (cos 30 deg = 0.866 > 0.28 so it won't auto-deflect). */
    const float init_ang = 0.5235988f;
    const EntityId ofuda = game_debug_spawn_projectile(
        g, PROJ_HOMING_OFUDA, sx - 260.0f, sy, cosf(init_ang) * 140.0f, sinf(init_ang) * 140.0f);
    const VelocityComp *ovel = game_velocity(g, ofuda);
    const ProjComp *oproj = game_proj(g, ofuda);
    TEST_ASSERT_NOT_NULL(ovel);
    TEST_ASSERT_NOT_NULL(oproj);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.6f, ovel->fluid_coupling);

    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, ofuda));
    TEST_ASSERT_FALSE(oproj->deflected);
    const float speed_after = hypotf(ovel->vx, ovel->vy);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 140.0f, speed_after); /* Must stay 140px/s, not 180px/s */
    const float ang_after = atan2f(ovel->vy, ovel->vx);
    TEST_ASSERT_TRUE(ang_after < init_ang - 0.01f); /* Steered back toward shrine (0 rad) */

    /* Passing through fire > 0.30 burns up undeflected ofuda with 0 shrine dmg & no EV_DEFLECT. */
    const TransformComp *otr = game_transform(g, ofuda);
    const int32_t shrine_hp_before = g->shrines[0].hp;
    const float fire_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.90f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, otr->x, otr->y, 4.0f, 0.0f, 0.0f, fire_dye);
    game_step(g, NULL, NULL);
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, ofuda));
    TEST_ASSERT_EQUAL_INT32(shrine_hp_before, g->shrines[0].hp);
    for (int32_t e = 0; e < g->event_count; ++e) {
        TEST_ASSERT_NOT_EQUAL_INT(EV_DEFLECT, g->events[e].type);
    }

    /* Deflected Homing Ofuda deals 20 physical damage to an enemy. */
    clear_enemies_and_projs(g);
    const EntityId target = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 500.0f, 200.0f);
    fluid_reset(&g->fluid);
    game_status_mut(g, target)->ink_mark_t = 0.0f;
    const int32_t target_hp0 = game_health(g, target)->hp;
    const EntityId defl =
        game_debug_spawn_projectile(g, PROJ_HOMING_OFUDA, 485.0f, 200.0f, 220.0f, 0.0f);
    game_proj_mut(g, defl)->deflected = true;
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(target_hp0 - 20, game_health(g, target)->hp);

    game_destroy(g);
}

static void test_endless_loop_scaling_all_six_items(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* Act 4 (act_index=3, loop n=1): cycles Act 1 terrain + Tengu. */
    game_debug_start_act(g, 3);
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT32(1, g->run.endless_loop);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 340.0f, 190.0f)); /* Act 1 open field */
    /* Quota: 12 + 2*1 = 14; spawn interval: 0.95 * 0.95 = 0.9025. */
    TEST_ASSERT_EQUAL_INT32(14, g->run.wave_quota);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.95f * 0.95f, g->internal.spawn_cd);

    /* Normal enemy HP x(1 + 0.25*1) = x1.25 -> Umbrella 30 -> 38; Elite x2.5 -> 94. */
    clear_enemies_and_projs(g);
    const EntityId u1 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 250.0f, 200.0f);
    TEST_ASSERT_EQUAL_INT32(38, game_health(g, u1)->max_hp);
    enemy_apply_elite(g, u1, ELITE_FIREPROOF);
    TEST_ASSERT_EQUAL_INT32(94, game_health(g, u1)->max_hp);

    /* Act 4 Boss (Tengu): HP 400 * 1.35 = 540; attack CD x0.90. */
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);
    TEST_ASSERT_EQUAL_INT32(540, game_health(g, g->run.boss_id)->max_hp);
    game_enemy_mut(g, g->run.boss_id)->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.95f * 0.90f, game_enemy(g, g->run.boss_id)->fire_cd);

    /* Act 5 (act_index=4, loop n=1): cycles Act 2 terrain (Torii + 4 rocks) + Nue (HP 945). */
    game_debug_start_act(g, 4);
    advance_act_transition(g);
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 384.0f, 328.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 340.0f, 530.0f));
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_NUE);
    TEST_ASSERT_EQUAL_INT32(945, game_health(g, g->run.boss_id)->max_hp);

    /* Act 6 (act_index=5, loop n=1): cycles Act 3 terrain (2 rocks + 3 Spirit Pillars + Mountain
     * Wind) + Multi-Part Ryu (Head 1215, Orbs 405). */
    game_debug_start_act(g, 5);
    advance_act_transition(g);
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 340.0f, 190.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 340.0f, 530.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 640.0f, 130.0f));
    /* Mountain wind runs on Act 6 (stage_act == 2). */
    g->internal.wind_timer = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(g->internal.wind_telegraph_t > 0.0f);

    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_RYU);
    TEST_ASSERT_EQUAL_INT32(1215, game_health(g, g->run.boss_id)->max_hp);
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        TEST_ASSERT_EQUAL_INT32(405, game_health(g, g->ryu_body.orb_ids[s])->max_hp);
    }

    /* Act 7 (act_index=6, loop n=2): Umbrella 45, Tengu 680, Quota 16, Interval 0.95 * 0.95^2. */
    game_debug_start_act(g, 6);
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT32(2, g->run.endless_loop);
    TEST_ASSERT_EQUAL_INT32(16, g->run.wave_quota);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.95f * 0.95f * 0.95f, g->internal.spawn_cd);
    const EntityId u2 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 250.0f, 200.0f);
    TEST_ASSERT_EQUAL_INT32(45, game_health(g, u2)->max_hp);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    TEST_ASSERT_EQUAL_INT32(680, game_health(g, g->run.boss_id)->max_hp);

    /* High loop (act_index=32, n=10, stage_act=2): spawn interval floor 0.6s, boss CD floor x0.70
     * with >= tele_dur + 0.10f guard, and elite rate capped at 30%. */
    game_debug_start_act(g, 32);
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT32(10, g->run.endless_loop);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.6f, g->internal.spawn_cd);

    /* Check elite spawns on Act 4 (stage_act=0, n=1 -> 4% > 0%, unlike Act 1's 0%) and cap at 30%.
     */
    game_debug_start_act(g, 3);
    advance_act_transition(g);
    int32_t act4_elites = 0;
    for (int trial = 0; trial < 80; ++trial) {
        clear_enemies_and_projs(g);
        g->internal.wave_elites_spawned = 0;
        g->internal.spawn_cd = 0.0f;
        waves_step_spawns(g);
        if (g->internal.wave_elites_spawned > 0) {
            ++act4_elites;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT32(0, act4_elites);

    /* Verify boss CD floor x0.70 and telegraph protection on Tengu at n=10 (act_index=30). */
    game_debug_start_act(g, 30);
    advance_act_transition(g);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);
    BossComp *tb = game_boss_mut(g, g->run.boss_id);
    EnemyComp *ten = game_enemy_mut(g, g->run.boss_id);
    tb->phase_idx = 1;
    tb->pattern = 1; /* Fan Arrow (1.65 * 0.8 * 0.70 = 0.924 >= 0.70) */
    ten->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.65f * 0.8f * 0.70f, ten->fire_cd);
    TEST_ASSERT_TRUE(ten->fire_cd >= 0.70f);

    game_destroy(g);
}

static void start_act_wave(GameState *g, int32_t act_index, int32_t wave_index)
{
    game_debug_start_act(g, act_index);
    advance_act_transition(g);
    game_debug_start_wave(g, wave_index);
}

static void test_wave_quotas_ramp_across_waves_and_acts(void)
{
    static const int32_t k_expected_quota[GAME_MAIN_ACT_COUNT][GAME_BOSS_WAVE_INDEX] = {
        {12, 16, 20},
        {20, 24, 28},
        {26, 30, 34},
    };
    static const float k_expected_interval[GAME_MAIN_ACT_COUNT][GAME_BOSS_WAVE_INDEX] = {
        {0.95f, 0.88f, 0.82f},
        {0.80f, 0.76f, 0.72f},
        {0.72f, 0.68f, 0.65f},
    };

    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    int32_t quota[GAME_MAIN_ACT_COUNT][GAME_BOSS_WAVE_INDEX] = {{0}};
    float interval[GAME_MAIN_ACT_COUNT][GAME_BOSS_WAVE_INDEX] = {{0.0f}};
    for (int32_t a = 0; a < GAME_MAIN_ACT_COUNT; ++a) {
        for (int32_t w = 0; w < GAME_BOSS_WAVE_INDEX; ++w) {
            start_act_wave(g, a, w);
            quota[a][w] = g->run.wave_quota;
            interval[a][w] = g->internal.spawn_cd;
            TEST_ASSERT_EQUAL_INT32(k_expected_quota[a][w], quota[a][w]);
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, k_expected_interval[a][w], interval[a][w]);
        }
        start_act_wave(g, a, GAME_BOSS_WAVE_INDEX);
        TEST_ASSERT_EQUAL_INT32(0, g->run.wave_quota);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.4f, g->internal.spawn_cd);
    }

    float prev_interval = interval[0][0];
    for (int32_t a = 0; a < GAME_MAIN_ACT_COUNT; ++a) {
        for (int32_t w = 0; w < GAME_BOSS_WAVE_INDEX; ++w) {
            if (w > 0) {
                TEST_ASSERT_GREATER_THAN_INT32(quota[a][w - 1], quota[a][w]);
            }
            if (a > 0) {
                TEST_ASSERT_GREATER_THAN_INT32(quota[a - 1][w], quota[a][w]);
            }
            TEST_ASSERT_TRUE(interval[a][w] <= prev_interval + 1e-6f);
            prev_interval = interval[a][w];
        }
    }

    /* Verify live max_alive cap on Act 1 Wave 2 (initial roster 5, cap 7). */
    start_act_wave(g, 0, 1);
    TEST_ASSERT_EQUAL_INT32(5, waves_count_alive_escorts(g));
    for (int i = 0; i < 12; ++i) {
        g->internal.spawn_cd = 0.0f;
        waves_step_spawns(g);
    }
    TEST_ASSERT_EQUAL_INT32(7, waves_count_alive_escorts(g));

    /* Endless loop n replays the same act table with +2n quota. */
    for (int32_t n = 1; n <= 2; ++n) {
        for (int32_t a = 0; a < GAME_MAIN_ACT_COUNT; ++a) {
            for (int32_t w = 0; w < GAME_BOSS_WAVE_INDEX; ++w) {
                start_act_wave(g, a + GAME_MAIN_ACT_COUNT * n, w);
                TEST_ASSERT_EQUAL_INT32(n, g->run.endless_loop);
                TEST_ASSERT_EQUAL_INT32(quota[a][w] + 2 * n, g->run.wave_quota);
            }
        }
    }

    game_destroy(g);
}

static void test_tengu_phase2_transition_preserves_telegraph_warning(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    start_act_wave(g, 0, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);

    const EntityId tengu = g->run.boss_id;
    EnemyComp *ten = game_enemy_mut(g, tengu);
    BossComp *tb = game_boss_mut(g, tengu);
    const HealthComp *thp = game_health(g, tengu);
    const int32_t pat_before = tb->pattern;
    ten->fire_cd = 0.05f;
    tb->telegraph = 0.9f;

    (void)combat_apply_damage(g, tengu, 220, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(1, tb->phase_idx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.5f, thp->invuln_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, tb->telegraph);
    TEST_ASSERT_TRUE(ten->fire_cd >= 0.75f);

    g->internal.spawn_cd = 999.0f;
    int invuln_ticks = 0;
    while (thp->invuln_t > 0.0f && invuln_ticks < 100) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        ++invuln_ticks;
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, thp->invuln_t);
    TEST_ASSERT_EQUAL_INT32(pat_before, tb->pattern);
    TEST_ASSERT_TRUE(ten->fire_cd >= 0.70f && ten->fire_cd <= 0.95f);

    /* Must play a full telegraph before firing after invuln ends. */
    float max_telegraph = 0.0f;
    int tele_ticks = 0;
    for (int t = 0; t < 90; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        if (tb->pattern != pat_before) {
            break;
        }
        if (tb->telegraph > max_telegraph) {
            max_telegraph = tb->telegraph;
        }
        if (tb->telegraph > 0.0f) {
            ++tele_ticks;
        }
    }
    TEST_ASSERT_NOT_EQUAL_INT32(pat_before, tb->pattern);
    TEST_ASSERT_TRUE(max_telegraph > 0.90f);
    TEST_ASSERT_TRUE(tele_ticks >= 30);
    game_destroy(g);
}

static void test_nue_threshold_invuln_freezes_warned_stop_and_phase2_overlaps_two_trails(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    start_act_wave(g, 1, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_NUE);
    const EntityId nue = g->run.boss_id;
    EnemyComp *en = game_enemy_mut(g, nue);
    BossComp *bs = game_boss_mut(g, nue);
    HealthComp *hp = game_health_mut(g, nue);
    g->internal.spawn_cd = 999.0f;

    /* Put Nue in warned stop state, then cross Phase 2 threshold (<= 50% HP). */
    en->ai_state = 1;
    en->windup = 0.30f;
    en->fire_cd = 0.40f;
    (void)combat_apply_damage(g, nue, 360, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(1, bs->phase_idx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.5f, hp->invuln_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.65f, en->windup);
    TEST_ASSERT_TRUE(en->fire_cd >= 0.75f - 1e-4f);

    /* During 1.5s invuln, windup & fire_cd stay frozen and Nue never dashes. */
    const float frozen_windup = en->windup;
    const float frozen_cd = en->fire_cd;
    for (int t = 0; t < 60; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        TEST_ASSERT_EQUAL_INT32(1, en->ai_state);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, frozen_windup, en->windup);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, frozen_cd, en->fire_cd);
    }

    /* Let invuln finish on a clean field (no escort lantern reactions) and run through consecutive
     * Phase 2 dashes: two trails overlap live for ~1.5s (~90 ticks) after the second dash starts.
     */
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && id.index != nue.index) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);
    fluid_reset(&g->fluid);
    game_status_mut(g, nue)->ink_mark_t = 0.0f;
    game_status_mut(g, nue)->knockback_t = 0.0f;
    int overlap_ticks = 0;
    for (int t = 0; t < 480; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        if (g->internal.nue_trail_t[0] > 0.0f && g->internal.nue_trail_t[1] > 0.0f) {
            ++overlap_ticks;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT32(75, overlap_ticks);
    game_destroy(g);
}

static void test_bounce_orb_reflects_off_torii_west_face_near_corner(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    start_act_wave(g, 1, 0);
    clear_enemies_and_projs(g);
    fluid_reset(&g->fluid);

    /* Act 2 west torii pillar [376, 392) x [320, 336). */
    const EntityId orb =
        game_debug_spawn_projectile(g, PROJ_BOUNCE_ORB, 375.0f, 327.0f, 60.0f, -220.0f);
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, orb));
    TEST_ASSERT_EQUAL_INT32(1, game_proj(g, orb)->bounces_left);
    TEST_ASSERT_TRUE(game_velocity(g, orb)->vx < 0.0f);
    TEST_ASSERT_TRUE(game_transform(g, orb)->x < 376.0f);
    game_destroy(g);
}

static void test_ryu_coil_orbits_full_shrine_ring_below_y510(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    start_act_wave(g, 2, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_RYU);
    const EntityId ryu = g->run.boss_id;
    g->internal.spawn_cd = 999.0f;
    game_boss_mut(g, ryu)->phase_idx = 1;
    game_health_mut(g, ryu)->invuln_t = 0.0f;
    game_enemy_mut(g, ryu)->fire_cd = 99.0f;
    g->internal.ryu_coil_t = 2.0f;

    float max_y = 0.0f;
    float sum_r = 0.0f;
    int samples = 0;
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    for (int t = 0; t < 115; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_step(g, NULL, NULL);
        const TransformComp *tr = game_transform(g, ryu);
        TEST_ASSERT_NOT_NULL(tr);
        if (tr->y > max_y) {
            max_y = tr->y;
        }
        if (t >= 20 && g->internal.ryu_coil_t > 0.0f) {
            sum_r += hypotf(tr->x - sx, tr->y - sy);
            ++samples;
        }
    }
    TEST_ASSERT_TRUE(max_y > 540.0f);
    TEST_ASSERT_TRUE(max_y <= (float)INK_SCREEN_H - 96.0f + 1e-3f);
    TEST_ASSERT_TRUE(samples > 50);
    TEST_ASSERT_FLOAT_WITHIN(25.0f, 240.0f, sum_r / (float)samples);

    /* Exiting coil while deep below y=510 must glide back smoothly with no vertical snap. */
    g->internal.ryu_coil_t = 2.0f;
    for (int t = 0; t < 180; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_enemy_mut(g, ryu)->fire_cd = 99.0f;
        game_step(g, NULL, NULL);
        if (game_transform(g, ryu)->y > 550.0f) {
            break;
        }
    }
    TEST_ASSERT_TRUE(game_transform(g, ryu)->y > 550.0f);
    g->internal.ryu_coil_t = 0.0f;
    float prev_x = game_transform(g, ryu)->x;
    float prev_y = game_transform(g, ryu)->y;
    float max_exit_jump = 0.0f;
    bool returned_to_band = false;
    for (int t = 0; t < 240; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_enemy_mut(g, ryu)->fire_cd = 99.0f;
        game_step(g, NULL, NULL);
        const TransformComp *tr = game_transform(g, ryu);
        const float jump = hypotf(tr->x - prev_x, tr->y - prev_y);
        if (jump > max_exit_jump) {
            max_exit_jump = jump;
        }
        if (t == 0) {
            /* Must glide from y > 550 instead of snapping immediately to y = 510. */
            TEST_ASSERT_TRUE(tr->y > 530.0f);
        }
        if (tr->y <= 510.0f) {
            returned_to_band = true;
        }
        prev_x = tr->x;
        prev_y = tr->y;
    }
    TEST_ASSERT_TRUE(max_exit_jump < 20.0f);
    TEST_ASSERT_TRUE(returned_to_band);
    game_destroy(g);
}

static void test_tengu_patrol_oscillator_wraps_smoothly_both_sides_of_shrine(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    start_act_wave(g, 0, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_TENGU);
    const EntityId tengu = g->run.boss_id;
    g->internal.spawn_cd = 999.0f;
    game_enemy_mut(g, tengu)->fire_cd = 999.0f;

    const float sx = g->shrines[0].x;
    float min_x = sx;
    float max_x = sx;
    float prev_x = game_transform(g, tengu)->x;
    float prev_y = game_transform(g, tengu)->y;
    float max_step = 0.0f;
    for (int t = 0; t < 600; ++t) {
        g->shrines[0].hp = g->shrines[0].max_hp;
        game_enemy_mut(g, tengu)->fire_cd = 999.0f;
        game_step(g, NULL, NULL);
        const TransformComp *tr = game_transform(g, tengu);
        if (tr->x < min_x) {
            min_x = tr->x;
        }
        if (tr->x > max_x) {
            max_x = tr->x;
        }
        const float step = hypotf(tr->x - prev_x, tr->y - prev_y);
        if (step > max_step) {
            max_step = step;
        }
        prev_x = tr->x;
        prev_y = tr->y;
    }
    TEST_ASSERT_TRUE(min_x < sx - 130.0f);
    TEST_ASSERT_TRUE(max_x > sx + 130.0f);
    TEST_ASSERT_TRUE(max_step < 10.0f);
    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_three_acts_boss_relic_and_transitions);
    RUN_TEST(test_boss_relic_fallback_advances_act);
    RUN_TEST(test_act2_and_act3_terrain_solid_invariants);
    RUN_TEST(test_act3_spirit_pillar_and_mountain_wind);
    RUN_TEST(test_new_normal_enemies_and_projectiles);
    RUN_TEST(test_elite_mutations_and_summoned_exclusion);
    RUN_TEST(test_boss_nue_and_boss_ryu);
    RUN_TEST(test_ofuda_fox_barrier_homing_and_act3_roster);
    RUN_TEST(test_endless_loop_scaling_all_six_items);
    RUN_TEST(test_wave_quotas_ramp_across_waves_and_acts);
    RUN_TEST(test_tengu_phase2_transition_preserves_telegraph_warning);
    RUN_TEST(test_nue_threshold_invuln_freezes_warned_stop_and_phase2_overlaps_two_trails);
    RUN_TEST(test_bounce_orb_reflects_off_torii_west_face_near_corner);
    RUN_TEST(test_ryu_coil_orbits_full_shrine_ring_below_y510);
    RUN_TEST(test_tengu_patrol_oscillator_wraps_smoothly_both_sides_of_shrine);
    return UNITY_END();
}
