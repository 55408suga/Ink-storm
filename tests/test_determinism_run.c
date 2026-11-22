#include "fluid.h"
#include "game.h"
#include "unity.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static const char *const k_section_names[GAME_HASH_SECTION_COUNT] = {
    [GAME_HASH_CORE] = "CORE",         [GAME_HASH_FLUID] = "FLUID",
    [GAME_HASH_ECS] = "ECS",           [GAME_HASH_COMPONENTS] = "COMPONENTS",
    [GAME_HASH_RUN] = "RUN",           [GAME_HASH_EVENTS] = "EVENTS",
    [GAME_HASH_INTERNAL] = "INTERNAL",
};

typedef struct {
    int32_t event_counts[EV_TYPE_COUNT];
    bool saw_ink_below_max;
    int32_t boss_max_hp[GAME_MAIN_ACT_COUNT];
    int32_t boss_min_hp[GAME_MAIN_ACT_COUNT];
    int32_t total_steps;
} RunTelemetry;

static void tally_events(const GameState *g, RunTelemetry *tel)
{
    for (int32_t i = 0; i < g->event_count; ++i) {
        const GameEventType t = g->events[i].type;
        if ((unsigned)t < (unsigned)EV_TYPE_COUNT) {
            ++tel->event_counts[t];
        }
    }
}

static bool is_stopped_phase(RunPhase phase)
{
    return phase == RUN_PHASE_REWARD || phase == RUN_PHASE_VICTORY || phase == RUN_PHASE_GAME_OVER;
}

static void step_pair_and_assert(GameState *a, GameState *b, const GameCommand *cmd,
                                 RunTelemetry *tel)
{
    const uint64_t ticks_before = a->ticks;
    const BrushInput in_a = game_autoplay_input(a);
    const BrushInput in_b = game_autoplay_input(b);
    game_step(a, &in_a, cmd);
    game_step(b, &in_b, cmd);
    ++tel->total_steps;

    const uint64_t delta = a->ticks - ticks_before;
    TEST_ASSERT_TRUE(delta == 0 || delta == 1);
    if (delta == 0) {
        TEST_ASSERT_TRUE(is_stopped_phase(a->phase));
    }
    TEST_ASSERT_TRUE(fluid_validate_finite(&a->fluid));
    if (a->player.ink < a->player.ink_max) {
        tel->saw_ink_below_max = true;
    }
    tally_events(a, tel);

    uint64_t sec_a[GAME_HASH_SECTION_COUNT];
    uint64_t sec_b[GAME_HASH_SECTION_COUNT];
    game_hash_sections(a, sec_a);
    game_hash_sections(b, sec_b);
    for (int s = 0; s < GAME_HASH_SECTION_COUNT; ++s) {
        if (sec_a[s] != sec_b[s]) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "hash mismatch at step=%d tick=%" PRIu64 " section=%s a=%016" PRIx64
                     " b=%016" PRIx64,
                     tel->total_steps, a->ticks, k_section_names[s], sec_a[s], sec_b[s]);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

static void advance_until_phase(GameState *a, GameState *b, RunPhase target, int max_steps,
                                RunTelemetry *tel)
{
    for (int i = 0; i < max_steps && a->phase != target; ++i) {
        step_pair_and_assert(a, b, NULL, tel);
    }
    TEST_ASSERT_EQUAL_INT(target, a->phase);
    TEST_ASSERT_EQUAL_INT(target, b->phase);
}

static void run_act_deterministically(GameState *a, GameState *b, int32_t act_idx,
                                      RunTelemetry *tel)
{
    /* Act 1 transition runs 120 ticks from game_init; Act 2/3 run 1 tick inside CMD_PICK_REWARD
     * plus 119 subsequent ticks (120 ticks total). */
    int trans_ticks = 0;
    while (a->phase == RUN_PHASE_ACT_TRANSITION && trans_ticks < 200) {
        step_pair_and_assert(a, b, NULL, tel);
        ++trans_ticks;
    }
    const int expected_trans = (act_idx == 0) ? 120 : 119;
    TEST_ASSERT_EQUAL_INT(expected_trans, trans_ticks);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, a->phase);
    TEST_ASSERT_EQUAL_INT32(act_idx, a->run.act_index);

    /* Three normal waves per act. */
    for (int32_t w = 0; w < GAME_BOSS_WAVE_INDEX; ++w) {
        const int wave_ticks = act_idx == 0 ? 420 : 300;
        for (int i = 0; i < wave_ticks && a->phase == RUN_PHASE_PLAYING; ++i) {
            /* Keep shrine healthy across all 3 acts during automated soak. */
            if (a->shrines[0].hp <= 2) {
                a->shrines[0].hp = a->shrines[0].max_hp;
                b->shrines[0].hp = b->shrines[0].max_hp;
            }
            step_pair_and_assert(a, b, NULL, tel);
        }
        if (a->phase == RUN_PHASE_PLAYING) {
            game_debug_force_wave_clear(a);
            game_debug_force_wave_clear(b);
        }
        advance_until_phase(a, b, RUN_PHASE_REWARD, 120, tel);

        if (act_idx == 0 && w == 0) {
            const GameCommand reroll = {.type = CMD_REROLL_REWARD, .arg = 0};
            step_pair_and_assert(a, b, &reroll, tel);
        }
        if (act_idx == 0 && w == 1 && !a->run.reward.repair_available) {
            /* Verify no-op CMD_REPAIR_SHRINE preserves exact hash when shrine is full. */
            step_pair_and_assert(a, b, NULL, tel);
            const uint64_t h_before = game_hash(a);
            const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
            step_pair_and_assert(a, b, &repair, tel);
            TEST_ASSERT_EQUAL_HEX64(h_before, game_hash(a));
        }

        const GameCommand cmd = game_autoplay_command(a);
        step_pair_and_assert(a, b, &cmd, tel);
        if (a->phase == RUN_PHASE_REWARD) {
            /* Autoplay may reroll first; issue the follow-up pick command. */
            const GameCommand pick = game_autoplay_command(a);
            step_pair_and_assert(a, b, &pick, tel);
        }
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, a->phase);
    }

    /* Boss wave. */
    const HealthComp *boss_hp0 = game_health(a, a->run.boss_id);
    TEST_ASSERT_NOT_NULL(boss_hp0);
    tel->boss_max_hp[act_idx] = boss_hp0->max_hp;
    tel->boss_min_hp[act_idx] = boss_hp0->hp;

    const int boss_ticks = act_idx == 0 ? 600 : 480;
    for (int i = 0; i < boss_ticks && a->phase == RUN_PHASE_PLAYING; ++i) {
        if (a->shrines[0].hp <= 2) {
            a->shrines[0].hp = a->shrines[0].max_hp;
            b->shrines[0].hp = b->shrines[0].max_hp;
        }
        step_pair_and_assert(a, b, NULL, tel);
        if (a->run.boss_id.index != ECS_INVALID_INDEX) {
            const HealthComp *bhp = game_health(a, a->run.boss_id);
            if (bhp != NULL && bhp->hp < tel->boss_min_hp[act_idx]) {
                tel->boss_min_hp[act_idx] = bhp->hp;
            }
        }
    }
    if (a->phase == RUN_PHASE_PLAYING) {
        game_debug_force_wave_clear(a);
        game_debug_force_wave_clear(b);
    }

    if (act_idx + 1 < GAME_MAIN_ACT_COUNT) {
        /* Boss defeat -> Shrine HP +3 -> Relic 3-choice -> next Act transition. */
        advance_until_phase(a, b, RUN_PHASE_REWARD, 120, tel);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, a->run.reward.kind);
        const GameCommand pick_relic = {.type = CMD_PICK_REWARD, .arg = 0};
        step_pair_and_assert(a, b, &pick_relic, tel);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, a->phase);
    } else {
        advance_until_phase(a, b, RUN_PHASE_VICTORY, 120, tel);
    }
}

static void test_full_run_determinism_sanity_and_settlement(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1234));
    GameState *a = game_create(&cfg);
    GameState *b = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_INT32(GAME_MAIN_ACT_COUNT, a->run.act_count);

    RunTelemetry tel = {0};

    for (int32_t act = 0; act < GAME_MAIN_ACT_COUNT; ++act) {
        run_act_deterministically(a, b, act, &tel);
    }

    /* Full 3-act campaign sanity assertions. */
    TEST_ASSERT_EQUAL_INT32(3, a->run.acts_cleared);
    TEST_ASSERT_EQUAL_INT32(2, a->run.relic_count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, tel.event_counts[EV_KILL]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, tel.event_counts[EV_DEFLECT]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, tel.event_counts[EV_CYCLONE]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, tel.event_counts[EV_GALE]);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, tel.event_counts[EV_IGNITE] +
                                              tel.event_counts[EV_STORM_ZAP]);
    TEST_ASSERT_EQUAL_INT32(3, tel.event_counts[EV_BOSS_ENTER]);
    TEST_ASSERT_EQUAL_INT32(3, tel.event_counts[EV_ACT_CLEAR]);
    TEST_ASSERT_EQUAL_INT32(1, tel.event_counts[EV_VICTORY]);
    TEST_ASSERT_TRUE(tel.saw_ink_below_max);
    TEST_ASSERT_TRUE(tel.boss_min_hp[0] < tel.boss_max_hp[0]);

    /* Settlement after full 3-act VICTORY: from_acts == 30 + 40 + 50 == 120. */
    MetaState meta;
    meta_init_default(&meta);
    const MetaRunResult res = game_run_result(a);
    TEST_ASSERT_TRUE(res.cleared);
    TEST_ASSERT_EQUAL_INT32(3, res.acts_cleared);
    const MetaSettleBreakdown expected = meta_calc_crystals(&res);
    MetaSettleBreakdown actual = {0};
    TEST_ASSERT_TRUE(meta_settle_run(&meta, a, &actual));
    TEST_ASSERT_EQUAL_INT32(120, actual.from_acts);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, actual.total_crystals);
    TEST_ASSERT_EQUAL_INT32(expected.from_kills, actual.from_kills);
    TEST_ASSERT_EQUAL_INT32(expected.from_chain, actual.from_chain);
    TEST_ASSERT_FALSE(meta_settle_run(&meta, a, &actual));

    game_destroy(b);
    game_destroy(a);
}

static void test_different_seed_diverges(void)
{
    const RunConfig cfg_a = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1234));
    const RunConfig cfg_b = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x5678));
    GameState *a = game_create(&cfg_a);
    GameState *b = game_create(&cfg_b);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    game_debug_start_wave(a, 0);
    game_debug_start_wave(b, 0);

    bool diverged = false;
    for (int t = 0; t < 180; ++t) {
        const BrushInput in_a = game_autoplay_input(a);
        const BrushInput in_b = game_autoplay_input(b);
        game_step(a, &in_a, NULL);
        game_step(b, &in_b, NULL);

        uint64_t sec_a[GAME_HASH_SECTION_COUNT];
        uint64_t sec_b[GAME_HASH_SECTION_COUNT];
        game_hash_sections(a, sec_a);
        game_hash_sections(b, sec_b);
        if (sec_a[GAME_HASH_COMPONENTS] != sec_b[GAME_HASH_COMPONENTS] ||
            sec_a[GAME_HASH_FLUID] != sec_b[GAME_HASH_FLUID]) {
            diverged = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(diverged);

    game_destroy(b);
    game_destroy(a);
}

static void test_multi_tick_per_frame_event_tally(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1234));
    GameState *single = game_create(&cfg);
    GameState *batched = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(single);
    TEST_ASSERT_NOT_NULL(batched);

    game_debug_start_wave(single, 0);
    game_debug_start_wave(batched, 0);

    RunTelemetry tel_single = {0};
    RunTelemetry tel_batched = {0};

    static const int k_pattern[6] = {0, 2, 1, 3, 0, 1};
    const int target_ticks = 350;
    int ticks_done = 0;
    int frame = 0;
    while (ticks_done < target_ticks) {
        int n = k_pattern[frame % 6];
        if (ticks_done + n > target_ticks) {
            n = target_ticks - ticks_done;
        }
        for (int k = 0; k < n; ++k) {
            const BrushInput in_s = game_autoplay_input(single);
            const GameCommand cmd_s = game_autoplay_command(single);
            game_step(single, &in_s, &cmd_s);
            tally_events(single, &tel_single);

            const BrushInput in_b = game_autoplay_input(batched);
            const GameCommand cmd_b = game_autoplay_command(batched);
            game_step(batched, &in_b, &cmd_b);
            tally_events(batched, &tel_batched);
            ++ticks_done;
        }
        ++frame;
    }

    TEST_ASSERT_EQUAL_HEX64(game_hash(single), game_hash(batched));
    TEST_ASSERT_EQUAL_INT32_ARRAY(tel_single.event_counts, tel_batched.event_counts, EV_TYPE_COUNT);

    game_destroy(batched);
    game_destroy(single);
}

static void test_event_queue_overflow_and_reset(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->event_count = 0;

    const GameEvent ev = {.type = EV_KILL, .value = (int32_t)ENEMY_ARCHER};
    for (int i = 0; i < 130; ++i) {
        const bool ok = game_push_event(g, ev);
        TEST_ASSERT_EQUAL(i < GAME_MAX_EVENTS, ok);
    }
    TEST_ASSERT_EQUAL_INT32(GAME_MAX_EVENTS, g->event_count);
    TEST_ASSERT_EQUAL_UINT32(2, g->dropped_events);

    /* Next step starts with event_count = 0 while dropped_events remains cumulative. */
    game_step(g, NULL, NULL);
    TEST_ASSERT_TRUE(g->event_count < GAME_MAX_EVENTS);
    TEST_ASSERT_EQUAL_UINT32(2, g->dropped_events);

    game_destroy(g);
}

static void test_abort_and_transition_priority(void)
{
    /* Abort from PLAYING -> GAME_OVER with EVF_RUN_ABORTED; repeat is no-op. */
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->event_count = 0;

    const GameCommand abort_cmd = {.type = CMD_ABORT_RUN, .arg = 0};
    game_step(g, NULL, &abort_cmd);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_GAME_OVER, g->phase);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, g->event_count);
    TEST_ASSERT_EQUAL_INT(EV_GAME_OVER, g->events[0].type);
    TEST_ASSERT_TRUE((g->events[0].flags & EVF_RUN_ABORTED) != 0);

    game_step(g, NULL, NULL);
    const uint64_t h_after = game_hash(g);
    game_step(g, NULL, &abort_cmd);
    TEST_ASSERT_EQUAL_HEX64(h_after, game_hash(g));
    game_destroy(g);

    /* Simultaneous Shrine HP == 0 and wave_kills == wave_quota -> SHRINE_BREAK wins -> 90 ticks ->
     * GAME_OVER. */
    GameState *p = game_create(NULL);
    TEST_ASSERT_NOT_NULL(p);
    game_debug_start_wave(p, 0);
    p->shrines[0].hp = 0;
    p->run.wave_kills = p->run.wave_quota;

    game_step(p, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_SHRINE_BREAK, p->phase);

    for (int t = 0; t < 89; ++t) {
        game_step(p, NULL, NULL);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_SHRINE_BREAK, p->phase);
    }
    game_step(p, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_GAME_OVER, p->phase);

    game_destroy(p);
}

static void test_invalid_inputs_and_extreme_coords(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x999));
    GameState *ref = game_create(&cfg);
    GameState *nan_g = game_create(&cfg);
    GameState *inf_g = game_create(&cfg);
    GameState *ext_g = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(ref);
    TEST_ASSERT_NOT_NULL(nan_g);
    TEST_ASSERT_NOT_NULL(inf_g);
    TEST_ASSERT_NOT_NULL(ext_g);

    game_debug_start_wave(ref, 0);
    game_debug_start_wave(nan_g, 0);
    game_debug_start_wave(inf_g, 0);
    game_debug_start_wave(ext_g, 0);

    const BrushInput nan_in = {
        .mx = NAN, .my = 100.0f, .pmx = 100.0f, .pmy = 100.0f, .paint = true};
    const BrushInput inf_in = {
        .mx = 100.0f, .my = INFINITY, .pmx = 100.0f, .pmy = 100.0f, .cyclone = true};
    const BrushInput ext_in = {
        .mx = 1e30f, .my = -1e30f, .pmx = -1e30f, .pmy = 1e30f, .paint = true, .gale = true};
    const GameCommand bogus_cmd = {.type = (GameCommandType)99, .arg = 123};

    game_step(ref, NULL, NULL);
    game_step(nan_g, &nan_in, NULL);
    game_step(inf_g, &inf_in, NULL);
    TEST_ASSERT_EQUAL_HEX64(game_hash(ref), game_hash(nan_g));
    TEST_ASSERT_EQUAL_HEX64(game_hash(ref), game_hash(inf_g));

    for (int t = 0; t < 10; ++t) {
        game_step(ext_g, &ext_in, &bogus_cmd);
        TEST_ASSERT_TRUE(fluid_validate_finite(&ext_g->fluid));
    }

    game_destroy(ext_g);
    game_destroy(inf_g);
    game_destroy(nan_g);
    game_destroy(ref);
}

static void test_entity_capacity_full(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->event_count = 0;

    int spawned = 0;
    while (spawned < ECS_MAX_ENTITIES + 10) {
        const EntityId id = game_debug_spawn_orb(g, 200.0f, 200.0f);
        if (id.index == ECS_INVALID_INDEX) {
            break;
        }
        ++spawned;
    }
    TEST_ASSERT_EQUAL_UINT32(ECS_MAX_ENTITIES, ecs_alive_count(&g->ecs));

    for (int t = 0; t < 120; ++t) {
        const BrushInput in = game_autoplay_input(g);
        game_step(g, &in, NULL);
        TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));
        TEST_ASSERT_TRUE(ecs_alive_count(&g->ecs) <= ECS_MAX_ENTITIES);
    }

    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_run_determinism_sanity_and_settlement);
    RUN_TEST(test_different_seed_diverges);
    RUN_TEST(test_multi_tick_per_frame_event_tally);
    RUN_TEST(test_event_queue_overflow_and_reset);
    RUN_TEST(test_abort_and_transition_priority);
    RUN_TEST(test_invalid_inputs_and_extreme_coords);
    RUN_TEST(test_entity_capacity_full);
    return UNITY_END();
}
