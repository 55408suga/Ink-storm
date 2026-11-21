#include "game.h"
#include "roguelike/relics.h"
#include "unity.h"

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

static void assert_boss_alive(const GameState *g)
{
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, g->run.boss_id.index);
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, g->run.boss_id));
    const HealthComp *bhp = game_health(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(bhp);
    TEST_ASSERT_GREATER_THAN_INT32(0, bhp->hp);
}

static void test_three_acts_boss_relic_and_transitions(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT32(GAME_MAIN_ACT_COUNT, g->run.act_count);
    TEST_ASSERT_EQUAL_INT32(0, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.relic_count);

    /* Act 1 boss defeat -> Shrine HP +3 (capped at max_hp), aftermath 1.0s -> relic 3-choice. */
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive(g);
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

    /* Complete normal Act 2 transition before jumping to Act 2 boss wave. */
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);

    /* Act 2 boss defeat -> Shrine HP clamped to max_hp, relic reroll + pick -> Act 3 transition. */
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive(g);
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

    /* Complete normal Act 3 transition before jumping to Act 3 boss wave. */
    advance_act_transition(g);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT32(2, g->run.act_index);
    TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);

    /* Act 3 boss defeat -> aftermath 1.0s -> VICTORY (no relic offer, cleared == true). */
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    assert_boss_alive(g);
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
    assert_boss_alive(g);
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_three_acts_boss_relic_and_transitions);
    RUN_TEST(test_boss_relic_fallback_advances_act);
    return UNITY_END();
}
