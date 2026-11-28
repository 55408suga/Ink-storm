#include "core.h"
#include "ecs.h"
#include "fluid.h"
#include "game.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_clamp(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, ink_clampf(-1.0f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.5f, ink_clampf(0.5f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 1.0f, ink_clampf(2.0f, 0.0f, 1.0f));
}

static void test_splitmix64(void)
{
    uint64_t seed = 0;
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xe220a8397b1dcdaf), ink_rng_next(&seed));
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0x6e789e6aa1b965f4), ink_rng_next(&seed));

    seed = 0;
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0x1.c4415p-1f, ink_rng_f01(&seed));
}

static void test_hash_helpers(void)
{
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xcbf29ce484222325), ink_hash_bytes(INK_HASH_INIT, "", 0));
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xaf63dc4c8601ec8c), ink_hash_bytes(INK_HASH_INIT, "a", 1));
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0x85944171f73967e8),
                            ink_hash_bytes(INK_HASH_INIT, "foobar", 6));

    const uint64_t from_u32 = ink_hash_u32(INK_HASH_INIT, UINT32_C(0x64636261));
    const uint64_t from_str = ink_hash_bytes(INK_HASH_INIT, "abcd", 4);
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xfc179f83ee0724dd), from_u32);
    TEST_ASSERT_EQUAL_HEX64(from_str, from_u32);

    const uint64_t h_pos_zero = ink_hash_f32(INK_HASH_INIT, 0.0f);
    const uint64_t h_neg_zero = ink_hash_f32(INK_HASH_INIT, -0.0f);
    TEST_ASSERT_EQUAL_HEX64(h_pos_zero, h_neg_zero);

    const uint32_t nan_bits_a = UINT32_C(0x7fc00000);
    const uint32_t nan_bits_b = UINT32_C(0xffc00001);
    float nan_a = 0.0f;
    float nan_b = 0.0f;
    memcpy(&nan_a, &nan_bits_a, sizeof nan_a);
    memcpy(&nan_b, &nan_bits_b, sizeof nan_b);
    TEST_ASSERT_EQUAL_HEX64(ink_hash_f32(INK_HASH_INIT, nan_a), ink_hash_f32(INK_HASH_INIT, nan_b));
}

static void test_game_init_and_hash_contract(void)
{
    GameState *g1 = game_create(NULL);
    GameState *g2 = game_create(NULL);
    const RunConfig diff_cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(2));
    GameState *g3 = game_create(&diff_cfg);

    TEST_ASSERT_NOT_NULL(g1);
    TEST_ASSERT_NOT_NULL(g2);
    TEST_ASSERT_NOT_NULL(g3);

    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g1->phase);
    TEST_ASSERT_EQUAL_UINT64(0, g1->ticks);
    TEST_ASSERT_EQUAL_INT32(1, g1->shrine_count);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 640.0f, g1->shrines[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 360.0f, g1->shrines[0].y);
    TEST_ASSERT_EQUAL_INT32(8, g1->shrines[0].hp);
    TEST_ASSERT_EQUAL_INT32(8, g1->shrines[0].max_hp);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, GAME_BASE_INK_MAX, g1->player.ink);
    TEST_ASSERT_EQUAL_INT32(1, g1->run.rerolls_left);
    TEST_ASSERT_EQUAL_UINT32(ECS_INVALID_INDEX, g1->run.boss_id.index);

    game_step(g1, NULL, NULL);
    game_step(g1, NULL, NULL);
    game_step(g2, NULL, NULL);
    game_step(g2, NULL, NULL);
    game_step(g3, NULL, NULL);
    game_step(g3, NULL, NULL);

    TEST_ASSERT_EQUAL_UINT64(2, g1->ticks);
    TEST_ASSERT_TRUE(fluid_validate_finite(&g1->fluid));
    TEST_ASSERT_EQUAL_HEX64(game_hash(g1), game_hash(g2));
    TEST_ASSERT_NOT_EQUAL_UINT64(game_hash(g1), game_hash(g3));

    game_destroy(g3);
    game_destroy(g2);
    game_destroy(g1);
}

static void test_abort_run_contract(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    game_step(g, NULL, NULL);
    const uint64_t ticks_before = g->ticks;

    const GameCommand abort_cmd = {.type = CMD_ABORT_RUN, .arg = 0};
    game_step(g, NULL, &abort_cmd);

    TEST_ASSERT_EQUAL_INT(RUN_PHASE_GAME_OVER, g->phase);
    TEST_ASSERT_EQUAL_UINT64(ticks_before, g->ticks);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, g->event_count);
    TEST_ASSERT_EQUAL_INT(EV_GAME_OVER, g->events[0].type);
    TEST_ASSERT_TRUE((g->events[0].flags & EVF_RUN_ABORTED) != 0);

    /* Advance one idle tick in GAME_OVER so the transient event buffer is empty before checking
     * that a repeated CMD_ABORT_RUN is a pure no-op. */
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(0, g->event_count);
    const uint64_t h_before = game_hash(g);

    game_step(g, NULL, &abort_cmd);
    TEST_ASSERT_EQUAL_HEX64(h_before, game_hash(g));
    TEST_ASSERT_EQUAL_INT32(0, g->event_count);

    game_destroy(g);
}

static void test_component_accessors_contract(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    const EntityId invalid_zero = game_spawn_entity(g, 0);
    const EntityId invalid_bits = game_spawn_entity(g, UINT64_C(1) << 60);
    TEST_ASSERT_EQUAL_UINT32(ECS_INVALID_INDEX, invalid_zero.index);
    TEST_ASSERT_EQUAL_UINT32(ECS_INVALID_INDEX, invalid_bits.index);

    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_HEALTH);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, id.index);
    TEST_ASSERT_NOT_NULL(game_transform(g, id));
    TEST_ASSERT_NOT_NULL(game_health(g, id));
    TEST_ASSERT_NULL(game_proj(g, id));

    ecs_destroy(&g->ecs, id);
    TEST_ASSERT_NULL(game_transform(g, id));
    TEST_ASSERT_NULL(game_health(g, id));

    game_destroy(g);
}

static void test_event_queue_capacity_contract(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    const GameEvent ev = {.type = EV_KILL, .value = (int32_t)ENEMY_ARCHER};
    for (int i = 0; i < 130; ++i) {
        const bool pushed = game_push_event(g, ev);
        if (i < GAME_MAX_EVENTS) {
            TEST_ASSERT_TRUE(pushed);
        } else {
            TEST_ASSERT_FALSE(pushed);
        }
    }
    TEST_ASSERT_EQUAL_INT32(GAME_MAX_EVENTS, g->event_count);
    TEST_ASSERT_EQUAL_UINT32(2, g->dropped_events);

    game_destroy(g);
}

static void test_phase4_pre_contracts(void)
{
    for (int id = 0; id < (int)UPG_COUNT; ++id) {
        TEST_ASSERT_EQUAL_UINT8(3, game_upgrade_max_level((UpgradeId)id));
        TEST_ASSERT_TRUE((unsigned)game_upgrade_axis((UpgradeId)id) < (unsigned)AXIS_COUNT);
    }
    TEST_ASSERT_EQUAL_UINT8(0, game_upgrade_max_level(UPG_COUNT));
    TEST_ASSERT_EQUAL_UINT8(0, game_upgrade_max_level((UpgradeId)-1));

    TEST_ASSERT_TRUE(game_is_boss_enemy(ENEMY_BOSS_TENGU));
    TEST_ASSERT_TRUE(game_is_boss_enemy(ENEMY_BOSS_NUE));
    TEST_ASSERT_TRUE(game_is_boss_enemy(ENEMY_BOSS_RYU));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_ARCHER));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_LANTERN_FIRE));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_UMBRELLA));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_LANTERN_STORM));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_KAMAITACHI));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_STONE_ONI));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_INK_LEECH));
    TEST_ASSERT_FALSE(game_is_boss_enemy(ENEMY_DEF_COUNT));

    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT32(GAME_MAIN_ACT_COUNT, g->run.act_count);
    TEST_ASSERT_FALSE(game_has_relic(g, RELIC_VERMILION_INKSTONE));
    TEST_ASSERT_FALSE(game_has_relic(g, RELIC_COUNT));
    TEST_ASSERT_FALSE(game_has_relic(NULL, RELIC_VERMILION_INKSTONE));

    game_debug_start_act(g, 1);
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);

    const uint64_t h_before = game_hash(g);
    game_debug_start_act(g, -1);
    game_debug_start_act(g, META_MAX_BEST_ACT);
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
    TEST_ASSERT_EQUAL_HEX64(h_before, game_hash(g));

    game_destroy(g);
}

static void test_phase5_pre_contracts(void)
{
    TEST_ASSERT_EQUAL_INT(28, (int)UPG_COUNT);
    TEST_ASSERT_EQUAL_INT(15, (int)RELIC_COUNT);
    TEST_ASSERT_EQUAL_INT(12, (int)ENEMY_DEF_COUNT);
    TEST_ASSERT_EQUAL_INT(6, (int)PROJ_KIND_COUNT);

    for (int id = 0; id < (int)UPG_COUNT; ++id) {
        TEST_ASSERT_EQUAL_UINT8(3, game_upgrade_max_level((UpgradeId)id));
        TEST_ASSERT_TRUE((unsigned)game_upgrade_axis((UpgradeId)id) < (unsigned)AXIS_COUNT);
    }

    for (int id = 0; id < (int)RELIC_COUNT; ++id) {
        const int32_t expected_slot =
            id < (int)RELIC_SEAL_OF_TRINITY ? -1 : (id - (int)RELIC_SEAL_OF_TRINITY);
        TEST_ASSERT_EQUAL_INT32(expected_slot, game_relic_unlock_slot((RelicId)id));
    }
    TEST_ASSERT_EQUAL_INT32(-1, game_relic_unlock_slot(RELIC_COUNT));
    TEST_ASSERT_EQUAL_INT32(-1, game_relic_unlock_slot((RelicId)-1));

    RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(55));
    cfg.unlocked_relic_mask = UINT64_C(0x15); /* slots 0, 2, 4 */
    GameState *g = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(g);

    for (int id = 0; id < (int)RELIC_SEAL_OF_TRINITY; ++id) {
        TEST_ASSERT_TRUE(game_is_relic_unlocked(g, (RelicId)id));
    }
    TEST_ASSERT_TRUE(game_is_relic_unlocked(g, RELIC_SEAL_OF_TRINITY));
    TEST_ASSERT_FALSE(game_is_relic_unlocked(g, RELIC_INKSTORM_SECRET));
    TEST_ASSERT_TRUE(game_is_relic_unlocked(g, RELIC_THUNDERFIRE_GEM));
    TEST_ASSERT_FALSE(game_is_relic_unlocked(g, RELIC_ONI_TILE));
    TEST_ASSERT_TRUE(game_is_relic_unlocked(g, RELIC_ENDLESS_BRUSH));
    TEST_ASSERT_FALSE(game_is_relic_unlocked(g, RELIC_NIGHT_PARADE_SCROLL));
    TEST_ASSERT_FALSE(game_is_relic_unlocked(g, RELIC_COUNT));
    TEST_ASSERT_FALSE(game_is_relic_unlocked(NULL, RELIC_SEAL_OF_TRINITY));

    for (int id = 0; id < (int)ENEMY_DEF_COUNT; ++id) {
        if ((EnemyDefId)id == ENEMY_BOSS_RYU_ORB) {
            TEST_ASSERT_TRUE(game_is_boss_part((EnemyDefId)id));
        } else {
            TEST_ASSERT_FALSE(game_is_boss_part((EnemyDefId)id));
        }
    }
    TEST_ASSERT_FALSE(game_is_boss_part(ENEMY_DEF_COUNT));

    /* RyuBodyState initial values and hash coverage */
    TEST_ASSERT_EQUAL_UINT32(ECS_INVALID_INDEX, g->ryu_body.head_id.index);
    for (int k = 0; k < GAME_RYU_ORB_COUNT; ++k) {
        TEST_ASSERT_EQUAL_UINT32(ECS_INVALID_INDEX, g->ryu_body.orb_ids[k].index);
    }
    TEST_ASSERT_EQUAL_INT32(-1, g->ryu_body.active_orb_slot);
    TEST_ASSERT_EQUAL_INT32(0, g->ryu_body.trail_count);
    const uint64_t h_ryu_before = game_hash(g);
    g->ryu_body.trail[0] = (GameVec2){120.0f, 180.0f};
    g->ryu_body.trail_count = 1;
    TEST_ASSERT_NOT_EQUAL_UINT64(h_ryu_before, game_hash(g));
    g->ryu_body.trail_count = 0;
    TEST_ASSERT_EQUAL_HEX64(h_ryu_before, game_hash(g));

    /* game_debug_start_act endless range & out-of-range no-op */
    game_debug_start_act(g, 3);
    TEST_ASSERT_EQUAL_INT32(3, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(1, g->run.endless_loop);
    const uint64_t h_act3 = game_hash(g);
    game_debug_start_act(g, -1);
    game_debug_start_act(g, META_MAX_BEST_ACT);
    TEST_ASSERT_EQUAL_INT32(3, g->run.act_index);
    TEST_ASSERT_EQUAL_HEX64(h_act3, game_hash(g));

    /* CMD_START_ENDLESS: no-op outside VICTORY, transitions in VICTORY when unsettled & loop 0 */
    GameState *g2 = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g2);
    const GameCommand start_endless = {.type = CMD_START_ENDLESS, .arg = 0};
    GameState *g2_ref = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g2_ref);
    game_step(g2_ref, NULL, NULL);
    game_step(g2, NULL, &start_endless);
    TEST_ASSERT_EQUAL_HEX64(game_hash(g2_ref), game_hash(g2));

    g2->phase = RUN_PHASE_VICTORY;
    g2->run.run_settled = false;
    g2->run.endless_loop = 0;
    g2->run.act_index = GAME_MAIN_ACT_COUNT - 1;
    g2->shrines[0].hp = g2->shrines[0].max_hp - 4;
    const int32_t hp_before = g2->shrines[0].hp;
    game_step(g2, NULL, &start_endless);
    TEST_ASSERT_EQUAL_INT32(hp_before + 3, g2->shrines[0].hp);
    TEST_ASSERT_EQUAL_INT32(1, g2->run.endless_loop);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_REWARD, g2->phase);
    TEST_ASSERT_TRUE(g2->run.reward.kind == REWARD_KIND_RELIC ||
                     g2->run.reward.kind == REWARD_KIND_FALLBACK);

    game_destroy(g2_ref);
    game_destroy(g2);
    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clamp);
    RUN_TEST(test_splitmix64);
    RUN_TEST(test_hash_helpers);
    RUN_TEST(test_game_init_and_hash_contract);
    RUN_TEST(test_abort_run_contract);
    RUN_TEST(test_component_accessors_contract);
    RUN_TEST(test_event_queue_capacity_contract);
    RUN_TEST(test_phase4_pre_contracts);
    RUN_TEST(test_phase5_pre_contracts);
    return UNITY_END();
}
