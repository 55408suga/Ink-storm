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

void setUp(void) {}

void tearDown(void) {}

static void advance_aftermath(GameState *g)
{
    for (int i = 0; i < 65 && g->phase == RUN_PHASE_WAVE_AFTERMATH; ++i) {
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
    game_step(g, NULL, NULL);

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
    game_step(g, NULL, NULL);
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
    game_step(g, NULL, NULL);
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
    game_step(g, NULL, NULL);
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

    /* --- Act 3 Boss: Ippitsu-Ryu (Simplified P1) --- */
    g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 2);
    advance_act_transition(g);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive_kind(g, ENEMY_BOSS_RYU);

    const EntityId ryu = g->run.boss_id;
    HealthComp *rhp = game_health_mut(g, ryu);
    const BossComp *rboss = game_boss(g, ryu);
    EnemyComp *ren = game_enemy_mut(g, ryu);
    TEST_ASSERT_EQUAL_INT32(1500, rhp->max_hp);
    TEST_ASSERT_EQUAL_INT32(0, rboss->part_id);

    /* Drop below 66% (<= 1000) -> Storm Orb (part_id == 1) & spawns lightning strikes. */
    (void)combat_apply_damage(g, ryu, 520, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(1, rboss->part_id);
    TEST_ASSERT_EQUAL_INT32(0, rboss->phase_idx);

    ren->fire_cd = 0.01f;
    game_step(g, NULL, NULL);
    /* Boss spawn swirl must not self-trigger Cataclysm or Thunderburst without player input. */
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

    /* Test Lightning Strike rod with ink > 0.3 vs no ink hitting shrine. */
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

    /* Drop below 33% (<= 500) -> Awakening (EV_BOSS_PHASE, part_id==2, phase_idx==1) + Cataclysm
     * x2. */
    g->event_count = 0;
    (void)combat_apply_damage(g, ryu, 520, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(2, rboss->part_id);
    TEST_ASSERT_EQUAL_INT32(1, rboss->phase_idx);
    bool saw_ryu_phase = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_BOSS_PHASE && g->events[e].value == 2) {
            saw_ryu_phase = true;
        }
    }
    TEST_ASSERT_TRUE(saw_ryu_phase);

    /* Clear phase invuln and verify Cataclysm damage x2: 80 -> 160. */
    rhp->invuln_t = 0.0f;
    game_status_mut(g, ryu)->ink_mark_t = 0.0f;
    const int32_t cata_dmg =
        combat_apply_damage(g, ryu, 80, DMG_FIRE | DMG_STORM | DMG_CATACLYSM, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(160, cata_dmg);

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
    return UNITY_END();
}
