#include "unity.h"
#include "vfx_rules.h"

void setUp(void) {}

void tearDown(void) {}

static void test_only_milestones_bypass_the_major_effect_budget(void)
{
    for (int t = 0; t < (int)EV_TYPE_COUNT; ++t) {
        const GameEventType type = (GameEventType)t;
        const bool milestone =
            type == EV_WAVE_CLEAR || type == EV_ACT_CLEAR || type == EV_BOSS_ENTER;
        TEST_ASSERT_EQUAL(milestone, vfx_event_bypasses_fx_budget(type));
    }
}

static void test_seal_ranking_prefers_priority_then_newest(void)
{
    const VfxSealRank seals[] = {
        {.active = true, .priority = 1, .age = 0.05f}, {.active = true, .priority = 3, .age = 0.4f},
        {.active = false, .priority = 3, .age = 0.0f}, {.active = true, .priority = 2, .age = 0.3f},
        {.active = true, .priority = 1, .age = 0.01f}, {.active = true, .priority = 3, .age = 0.1f},
    };
    int32_t out[2];
    TEST_ASSERT_EQUAL_INT32(2, vfx_rank_top_two(seals, 6, out));
    TEST_ASSERT_EQUAL_INT32(5, out[0]);
    TEST_ASSERT_EQUAL_INT32(1, out[1]);

    TEST_ASSERT_EQUAL_INT32(2, vfx_rank_top_two(seals, 5, out));
    TEST_ASSERT_EQUAL_INT32(1, out[0]);
    TEST_ASSERT_EQUAL_INT32(3, out[1]);

    TEST_ASSERT_EQUAL_INT32(1, vfx_rank_top_two(seals, 1, out));
    TEST_ASSERT_EQUAL_INT32(0, out[0]);
    TEST_ASSERT_EQUAL_INT32(-1, out[1]);
}

static void test_enemy_intro_only_runs_while_it_can_be_seen(void)
{
    for (int p = RUN_PHASE_PLAYING; p <= RUN_PHASE_GAME_OVER; ++p) {
        TEST_ASSERT_EQUAL(p == RUN_PHASE_PLAYING, vfx_enemy_intro_shown_in((RunPhase)p));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_milestones_bypass_the_major_effect_budget);
    RUN_TEST(test_seal_ranking_prefers_priority_then_newest);
    RUN_TEST(test_enemy_intro_only_runs_while_it_can_be_seen);
    return UNITY_END();
}
