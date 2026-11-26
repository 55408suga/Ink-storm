#include "unity.h"
#include "vfx_rules.h"

#include <math.h>
#include <stddef.h>

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

static void assert_thunderburst_fx(VfxThunderburstFx fx, bool full, bool callout, float amp,
                                   float dur)
{
    TEST_ASSERT_EQUAL(full, fx.full_burst);
    TEST_ASSERT_EQUAL(callout, fx.show_callout);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, amp, fx.shake_amp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, dur, fx.shake_dur);
}

static void test_thunderburst_chain_spaces_out_enso_callout_and_softens_shake(void)
{
    VfxThunderburstThrottle st = {0};
    const float tick = 1.0f / 60.0f;

    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(&st), true, true, 4.2f, 0.16f);

    vfx_thunderburst_throttle_update(&st, tick);
    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(&st), false, false, 0.0f, 0.0f);

    vfx_thunderburst_throttle_update(&st, 0.45f - tick);
    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(&st), true, false, 0.0f, 0.0f);

    vfx_thunderburst_throttle_update(&st, 0.25f);
    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(&st), false, false, 2.0f, 0.10f);

    vfx_thunderburst_throttle_update(&st, 1.41f);
    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(&st), true, true, 4.2f, 0.16f);
}

static void test_thunderburst_throttle_tolerates_null_and_non_finite_or_non_positive_dt(void)
{
    assert_thunderburst_fx(vfx_thunderburst_throttle_consume(NULL), true, true, 4.2f, 0.16f);
    vfx_thunderburst_throttle_update(NULL, 0.5f);

    VfxThunderburstThrottle st = {0};
    (void)vfx_thunderburst_throttle_consume(&st);
    const VfxThunderburstThrottle armed = st;
    vfx_thunderburst_throttle_update(&st, 0.0f);
    vfx_thunderburst_throttle_update(&st, -1.0f);
    vfx_thunderburst_throttle_update(&st, NAN);
    vfx_thunderburst_throttle_update(&st, INFINITY);
    vfx_thunderburst_throttle_update(&st, -INFINITY);
    TEST_ASSERT_EQUAL_MEMORY(&armed, &st, sizeof st);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_milestones_bypass_the_major_effect_budget);
    RUN_TEST(test_seal_ranking_prefers_priority_then_newest);
    RUN_TEST(test_enemy_intro_only_runs_while_it_can_be_seen);
    RUN_TEST(test_thunderburst_chain_spaces_out_enso_callout_and_softens_shake);
    RUN_TEST(test_thunderburst_throttle_tolerates_null_and_non_finite_or_non_positive_dt);
    return UNITY_END();
}
