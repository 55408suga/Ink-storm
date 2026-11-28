#include "enemies/boss_break.h"
#include "render.h"
#include "unity.h"
#include "vfx_rules.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

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

static void test_shrine_bar_style_full_damaged_alert_hit_and_reduce_flash(void)
{
    typedef struct {
        int32_t hp;
        int32_t max_hp;
        float hit_t;
        float time_s;
        bool reduce_flash;
        bool visible;
        float alpha;
        float scale;
        bool alert;
    } Case;

    /* At t = 0.25 / 2.2 s, sin(2*pi*2.2*t) == +1.0 -> wave == 1.0. */
    const float peak_t = 0.25f / 2.2f;
    const Case cases[] = {
        {0, 8, 0.0f, 0.0f, false, false, 0.0f, 1.0f, false},
        {5, 0, 0.0f, 0.0f, false, false, 0.0f, 1.0f, false},
        {8, 8, 0.0f, 0.0f, false, true, 0.50f, 1.0f, false},
        {6, 8, 0.0f, 0.0f, false, true, 0.80f, 1.0f, false},
        {3, 8, 0.0f, peak_t, false, true, 1.00f, 1.04f, true},
        {3, 8, 0.0f, peak_t, true, true, 0.88f, 1.00f, true},
        {5, 8, 0.45f, 0.0f, false, true, 0.95f, 1.25f, false},
        {2, 8, 0.45f, 0.0f, true, true, 0.95f, 1.00f, true},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const VfxShrineBar got = vfx_shrine_bar_style(cases[i].hp, cases[i].max_hp, cases[i].hit_t,
                                                      cases[i].time_s, cases[i].reduce_flash);
        TEST_ASSERT_EQUAL(cases[i].visible, got.visible);
        TEST_ASSERT_EQUAL(cases[i].alert, got.alert);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, cases[i].alpha, got.alpha);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, cases[i].scale, got.scale);
    }
}

static void test_boss_cue_covers_every_boss_phase_and_break_and_hides_outside_boss_play(void)
{
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_NONE, vfx_boss_cue(NULL));

    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* Non-boss wave hides the cue. */
    game_debug_start_wave(g, 0);
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_NONE, vfx_boss_cue(g));

    /* Act 1 Tengu: P1, P2, Break, and hidden outside RUN_PHASE_PLAYING. */
    game_debug_start_act(g, 0);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    BossComp *tb = game_boss_mut(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(tb);
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_TENGU_P1, vfx_boss_cue(g));
    tb->phase_idx = 1;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_TENGU_P2, vfx_boss_cue(g));
    tb->break_t = 1.5f;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_BREAK, vfx_boss_cue(g));
    g->phase = RUN_PHASE_REWARD;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_NONE, vfx_boss_cue(g));

    /* Act 2 Nue: moving, grounded, Break. */
    game_debug_start_act(g, 1);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    EnemyComp *nen = game_enemy_mut(g, g->run.boss_id);
    BossComp *nb = game_boss_mut(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(nen);
    TEST_ASSERT_NOT_NULL(nb);
    nen->ai_state = 2;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_NUE_MOVING, vfx_boss_cue(g));
    nen->ai_state = 1;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_NUE_GROUNDED, vfx_boss_cue(g));
    nb->break_t = 1.0f;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_BREAK, vfx_boss_cue(g));

    /* Act 3 Ippitsu-Ryu: P1, P1 orb-break flinch, P2, P3, and Stance Break. */
    game_debug_start_act(g, 2);
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    BossComp *rb = game_boss_mut(g, g->run.boss_id);
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_RYU_P1, vfx_boss_cue(g));
    rb->break_t = 1.8f;
    rb->break_kind = (int32_t)BOSS_BREAK_FLINCH;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_RYU_FLINCH, vfx_boss_cue(g));
    rb->break_t = 0.0f;
    rb->break_kind = (int32_t)BOSS_BREAK_NONE;
    rb->phase_idx = 1;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_RYU_P2, vfx_boss_cue(g));
    rb->phase_idx = 2;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_RYU_P3, vfx_boss_cue(g));
    rb->break_t = 1.0f;
    rb->break_kind = (int32_t)BOSS_BREAK_STANCE;
    TEST_ASSERT_EQUAL_INT(VFX_BOSS_CUE_BREAK, vfx_boss_cue(g));

    game_destroy(g);
}

static void test_bottom_dock_lifts_cleanly_above_brush_badge_relics_and_showcase_badge(void)
{
    const float h_intro = 62.0f;
    const float h_hint = 46.0f;
    const float base_bottom = 702.0f;
    const float lifted_bottom = 664.0f;

    /* Standard centered strip with no relics sits at the normal bottom dock baseline. */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, base_bottom - h_intro,
                             vfx_bottom_dock_y(500.0f, h_intro, 0, false));

    /* Two relics leave right clearance up to 1016px (600px strip ends at 940px -> no lift). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, base_bottom - h_intro,
                             vfx_bottom_dock_y(600.0f, h_intro, 2, false));

    /* Three relics shrink right clearance to 896px (600px strip ends at 940px -> lifts). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lifted_bottom - h_intro,
                             vfx_bottom_dock_y(600.0f, h_intro, 3, false));

    /* Six+ relics clamp at 6 tiles and still lift a narrow 380px hint strip above relics. */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lifted_bottom - h_hint,
                             vfx_bottom_dock_y(380.0f, h_hint, 9, false));

    /* Wide strip crossing the left Brush Badge margin (strip_left = 210 < 214) lifts. */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lifted_bottom - h_hint,
                             vfx_bottom_dock_y(860.0f, h_hint, 0, false));

    /* Strip with strip_left = 216 clears Brush Badge (214) but lifts for showcase badge (218). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, base_bottom - h_hint,
                             vfx_bottom_dock_y(848.0f, h_hint, 0, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lifted_bottom - h_hint,
                             vfx_bottom_dock_y(848.0f, h_hint, 0, true));

    /* Negative relic count is treated safely as zero relics. */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, base_bottom - h_hint,
                             vfx_bottom_dock_y(420.0f, h_hint, -3, false));
}

static void test_explanation_banner_durations_and_fade_curves(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.5f, VFX_ENEMY_INTRO_SECONDS);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.2f, VFX_BOSS_BANNER_SECONDS);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 4.0f, VFX_HINT_SHOW_SECONDS);

    /* Hint strip: counts down from 4.0s (0.25s fade-in, 3.35s solid, 0.40s fade-out). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_hint_strip_alpha(4.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_hint_strip_alpha(3.875f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_hint_strip_alpha(3.75f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_hint_strip_alpha(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_hint_strip_alpha(0.40f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_hint_strip_alpha(0.20f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_hint_strip_alpha(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_hint_strip_alpha(NAN));

    /* Enemy intro: counts up from 0.0s to 3.5s (0.25s fade-in, 2.85s solid, 0.40s fade-out). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_enemy_intro_alpha(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_enemy_intro_alpha(0.125f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_enemy_intro_alpha(0.25f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_enemy_intro_alpha(2.50f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_enemy_intro_alpha(3.10f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_enemy_intro_alpha(3.30f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_enemy_intro_alpha(3.50f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_enemy_intro_alpha(NAN));

    /* Boss entrance banner: counts down from 2.2s (0.20s fade-in, 1.60s solid, 0.40s fade-out). */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_boss_banner_alpha(2.2f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_boss_banner_alpha(2.1f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_boss_banner_alpha(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, vfx_boss_banner_alpha(0.40f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, vfx_boss_banner_alpha(0.20f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_boss_banner_alpha(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, vfx_boss_banner_alpha(NAN));
}

static void test_render_reset_vfx_clears_all_skill_deny_timers(void)
{
    RenderState *r = calloc(1, sizeof *r);
    TEST_ASSERT_NOT_NULL(r);
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    for (int s = 0; s < SKILL_COUNT; ++s) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_SKILL_DENIED,
                                     .value = s,
                                 });
    }
    render_consume_events(r, g);
    for (int s = 0; s < SKILL_COUNT; ++s) {
        TEST_ASSERT_TRUE(r->skill_deny_t[s] > 0.0f);
    }
    TEST_ASSERT_TRUE(r->skill_deny_t[SKILL_CYCLONE_CCW] > 0.0f);

    render_reset_vfx(r, UINT64_C(42));
    for (int s = 0; s < SKILL_COUNT; ++s) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, r->skill_deny_t[s]);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r->skill_deny_t[SKILL_CYCLONE_CCW]);

    game_destroy(g);
    free(r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_milestones_bypass_the_major_effect_budget);
    RUN_TEST(test_seal_ranking_prefers_priority_then_newest);
    RUN_TEST(test_enemy_intro_only_runs_while_it_can_be_seen);
    RUN_TEST(test_thunderburst_chain_spaces_out_enso_callout_and_softens_shake);
    RUN_TEST(test_thunderburst_throttle_tolerates_null_and_non_finite_or_non_positive_dt);
    RUN_TEST(test_shrine_bar_style_full_damaged_alert_hit_and_reduce_flash);
    RUN_TEST(test_boss_cue_covers_every_boss_phase_and_break_and_hides_outside_boss_play);
    RUN_TEST(test_bottom_dock_lifts_cleanly_above_brush_badge_relics_and_showcase_badge);
    RUN_TEST(test_explanation_banner_durations_and_fade_curves);
    RUN_TEST(test_render_reset_vfx_clears_all_skill_deny_timers);
    return UNITY_END();
}
