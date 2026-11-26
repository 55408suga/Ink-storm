#include "combat/damage.h"
#include "core.h"
#include "elements/elements.h"
#include "game.h"
#include "meta.h"
#include "player/player_skills.h"
#include "roguelike/hints.h"
#include "roguelike/relics.h"
#include "roguelike/rewards.h"
#include "roguelike/upgrades.h"
#include "roguelike/waves.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static const MetaState defaults = {
    .version = 1,
    .settings = {80, 70, 85, 100, true, true, UI_LANG_AUTO, false, 100},
};

static const MetaState saved = {
    .version = 1,
    .ink_crystals = 12345,
    .upgrade_lv = {1, 2, 2, 3},
    .unlocked_relic_mask = 0x25,
    .best_score = 34567,
    .best_act = 4,
    .runs_played = 9,
    .clears = 2,
    .seen_hints_mask = UINT32_C(0xdeadcafe),
    .settings = {11, 22, 33, 50, false, false, UI_LANG_EN, true, 125},
};

static void assert_settings(SettingsState expected, SettingsState actual)
{
    TEST_ASSERT_EQUAL_UINT8(expected.master_vol, actual.master_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.bgm_vol, actual.bgm_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.se_vol, actual.se_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.shake_scale, actual.shake_scale);
    TEST_ASSERT_EQUAL(expected.post_shader_on, actual.post_shader_on);
    TEST_ASSERT_EQUAL(expected.hints_on, actual.hints_on);
    TEST_ASSERT_EQUAL_UINT8(expected.ui_lang, actual.ui_lang);
    TEST_ASSERT_EQUAL(expected.reduce_flash, actual.reduce_flash);
    TEST_ASSERT_EQUAL_UINT8(expected.cursor_scale, actual.cursor_scale);
}

static void assert_meta(const MetaState *expected, const MetaState *actual)
{
    TEST_ASSERT_EQUAL_UINT32(expected->version, actual->version);
    TEST_ASSERT_EQUAL_INT32(expected->ink_crystals, actual->ink_crystals);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected->upgrade_lv, actual->upgrade_lv, META_UPG_COUNT);
    TEST_ASSERT_EQUAL_HEX64(expected->unlocked_relic_mask, actual->unlocked_relic_mask);
    TEST_ASSERT_EQUAL_INT32(expected->best_score, actual->best_score);
    TEST_ASSERT_EQUAL_INT32(expected->best_act, actual->best_act);
    TEST_ASSERT_EQUAL_INT32(expected->runs_played, actual->runs_played);
    TEST_ASSERT_EQUAL_INT32(expected->clears, actual->clears);
    TEST_ASSERT_EQUAL_HEX32(expected->seen_hints_mask, actual->seen_hints_mask);
    assert_settings(expected->settings, actual->settings);
}

static void assert_run_config(RunConfig expected, RunConfig actual)
{
    TEST_ASSERT_EQUAL_HEX64(expected.seed, actual.seed);
    TEST_ASSERT_EQUAL(expected.brush_style, actual.brush_style);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.meta_upgrade_lv, actual.meta_upgrade_lv, META_UPG_COUNT);
    TEST_ASSERT_EQUAL_HEX64(expected.unlocked_relic_mask, actual.unlocked_relic_mask);
    TEST_ASSERT_EQUAL_INT32(expected.endless_loop, actual.endless_loop);
}

static void assert_breakdown(MetaSettleBreakdown expected, MetaSettleBreakdown actual)
{
    TEST_ASSERT_EQUAL_INT32(expected.from_acts, actual.from_acts);
    TEST_ASSERT_EQUAL_INT32(expected.from_kills, actual.from_kills);
    TEST_ASSERT_EQUAL_INT32(expected.from_chain, actual.from_chain);
    TEST_ASSERT_EQUAL_INT32(expected.from_reactions, actual.from_reactions);
    TEST_ASSERT_EQUAL_INT32(expected.from_endless, actual.from_endless);
    TEST_ASSERT_EQUAL_INT32(expected.from_bonus, actual.from_bonus);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, actual.total_crystals);
}

static void test_default_meta_and_settings(void)
{
    MetaState meta = saved;
    meta.version = UINT32_MAX;
    meta_init_default(&meta);
    assert_meta(&defaults, &meta);
    assert_settings(defaults.settings, meta_settings_default());
}

static void test_run_config_copies_progress_and_preset_stays_independent(void)
{
    const uint64_t seed = UINT64_C(0x123456789abcdef0);
    const RunConfig preset = {.seed = seed, .brush_style = BRUSH_STYLE_QUILL};
    const RunConfig upgraded = {.seed = seed,
                                .brush_style = BRUSH_STYLE_QUILL,
                                .meta_upgrade_lv = {1, 2, 2, 3},
                                .unlocked_relic_mask = 0x25};
    MetaState meta = saved;
    meta.unlocked_relic_mask |= UINT64_C(1) << 63;
    const RunConfig config = meta_build_run_config(&meta, BRUSH_STYLE_QUILL, seed);
    assert_run_config(preset, meta_run_config_preset(BRUSH_STYLE_QUILL, seed));
    assert_run_config(preset, meta_build_run_config(NULL, BRUSH_STYLE_QUILL, seed));
    meta_init_default(&meta);
    assert_run_config(upgraded, config);
    TEST_ASSERT_EQUAL(BRUSH_STYLE_HEAVY,
                      meta_build_run_config(&meta, BRUSH_STYLE_COUNT, seed).brush_style);
    TEST_ASSERT_EQUAL(BRUSH_STYLE_HEAVY,
                      meta_run_config_preset((BrushStyleId)99, seed).brush_style);
}

static void test_shop_prices_and_all_purchases(void)
{
    const uint8_t levels[] = {3, 3, 2, 3};
    const int32_t prices[][3] = {{80, 140, 200}, {60, 100, 140}, {100, 200}, {60, 100, 140}};
    MetaState meta = defaults;
    meta.ink_crystals = 1920;
    MetaState expected = meta;
    for (MetaUpgradeKind kind = META_UPG_SHRINE_HP; kind < META_UPG_COUNT; kind++) {
        TEST_ASSERT_EQUAL_UINT8(levels[kind], meta_upgrade_max_level(kind));
        for (uint8_t level = 0; level < levels[kind]; level++) {
            const int32_t price = prices[kind][level];
            TEST_ASSERT_EQUAL_INT32(price, meta_upgrade_next_cost(&meta, kind));
            MetaState poor = meta;
            poor.ink_crystals = price - 1;
            const MetaState before = poor;
            TEST_ASSERT_FALSE(meta_buy_upgrade(&poor, kind));
            assert_meta(&before, &poor);
            TEST_ASSERT_TRUE(meta_buy_upgrade(&meta, kind));
            expected.ink_crystals -= price;
            expected.upgrade_lv[kind]++;
            assert_meta(&expected, &meta);
        }
        TEST_ASSERT_EQUAL_INT32(-1, meta_upgrade_next_cost(&meta, kind));
        TEST_ASSERT_FALSE(meta_buy_upgrade(&meta, kind));
        assert_meta(&expected, &meta);
    }
    for (uint32_t slot = 0; slot < META_RELIC_UNLOCK_COUNT; slot++) {
        TEST_ASSERT_EQUAL_INT32(100, meta_relic_unlock_cost(slot));
        TEST_ASSERT_FALSE(meta_is_relic_unlocked(&meta, slot));
        MetaState poor = meta;
        poor.ink_crystals = 99;
        const MetaState before = poor;
        TEST_ASSERT_FALSE(meta_buy_relic_unlock(&poor, slot));
        assert_meta(&before, &poor);
        TEST_ASSERT_TRUE(meta_buy_relic_unlock(&meta, slot));
        TEST_ASSERT_TRUE(meta_is_relic_unlocked(&meta, slot));
        expected.ink_crystals -= 100;
        expected.unlocked_relic_mask |= UINT64_C(1) << slot;
        TEST_ASSERT_FALSE(meta_buy_relic_unlock(&meta, slot));
        assert_meta(&expected, &meta);
    }
    TEST_ASSERT_EQUAL_INT32(0, meta.ink_crystals);
}

static void test_invalid_shop_items(void)
{
    MetaState meta = saved;
    const MetaUpgradeKind kinds[] = {META_UPG_COUNT, (MetaUpgradeKind)99};
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, meta_upgrade_max_level(kinds[i]));
        TEST_ASSERT_EQUAL_INT32(-1, meta_upgrade_next_cost(&meta, kinds[i]));
        TEST_ASSERT_FALSE(meta_buy_upgrade(&meta, kinds[i]));
        assert_meta(&saved, &meta);
    }
    const uint32_t slots[] = {6, UINT32_MAX};
    for (size_t i = 0; i < sizeof slots / sizeof slots[0]; i++) {
        TEST_ASSERT_EQUAL_INT32(-1, meta_relic_unlock_cost(slots[i]));
        TEST_ASSERT_FALSE(meta_is_relic_unlocked(&meta, slots[i]));
        TEST_ASSERT_FALSE(meta_buy_relic_unlock(&meta, slots[i]));
        assert_meta(&saved, &meta);
    }
}

static void test_crystal_breakdowns(void)
{
    const struct {
        const char *name;
        MetaRunResult run;
        MetaSettleBreakdown reward;
    } cases[] = {
        {"empty run", {0}, {0}},
        {"second act defeat",
         {.acts_cleared = 1, .kills = 74, .max_chain = 8},
         {30, 14, 16, 0, 0, 0, 60}},
        {"first clear",
         {.acts_cleared = 3, .kills = 152, .max_chain = 12, .thunderbursts = 9, .cataclysms = 4},
         {120, 30, 24, 20, 0, 0, 194}},
        {"out of range",
         {.acts_cleared = 7,
          .kills = -5,
          .max_chain = 20,
          .thunderbursts = 7,
          .cataclysms = 5,
          .endless_acts_cleared = -1},
         {120, 0, 30, 20, 0, 0, 170}},
        {"negative thunderbursts",
         {.acts_cleared = 2, .max_chain = -2, .thunderbursts = -5, .cataclysms = 2},
         {70, 0, 0, 6, 0, 0, 76}},
        {"negative cataclysms",
         {.acts_cleared = -1, .thunderbursts = 2, .cataclysms = -5},
         {0, 0, 0, 2, 0, 0, 2}},
        {"odd base with bonus",
         {.acts_cleared = 3,
          .kills = 200,
          .max_chain = 15,
          .thunderbursts = 11,
          .crystal_bonus_x15 = true},
         {120, 40, 30, 11, 0, 100, 301}},
        {"endless acts",
         {.acts_cleared = 3, .endless_acts_cleared = 2},
         {120, 0, 0, 0, 80, 0, 200}},
        {"maximum counters",
         {.acts_cleared = INT32_MAX,
          .endless_acts_cleared = INT32_MAX,
          .reached_act = INT32_MAX,
          .kills = INT32_MAX,
          .max_chain = INT32_MAX,
          .thunderbursts = INT32_MAX,
          .cataclysms = INT32_MAX,
          .score = INT32_MAX,
          .cleared = true,
          .crystal_bonus_x15 = true},
         {120, 429496729, 30, 20, 12000, 214754449, META_MAX_CRYSTALS}},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        UNITY_SET_DETAIL(cases[i].name);
        assert_breakdown(cases[i].reward, meta_calc_crystals(&cases[i].run));
    }
    UNITY_CLR_DETAILS();
    assert_breakdown((MetaSettleBreakdown){0}, meta_calc_crystals(NULL));
}

static void test_settlement_saturates_after_adding_reward(void)
{
    const MetaRunResult run = {.acts_cleared = 3,
                               .kills = 152,
                               .max_chain = 12,
                               .thunderbursts = 9,
                               .cataclysms = 4,
                               .score = INT32_MAX,
                               .reached_act = INT32_MAX,
                               .cleared = true};
    const struct {
        int32_t balance, expected;
    } cases[] = {{META_MAX_CRYSTALS - 10, META_MAX_CRYSTALS},
                 {INT32_MAX, META_MAX_CRYSTALS},
                 {-100, 94},
                 {INT32_MIN, 0}};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        MetaState meta = defaults;
        meta.ink_crystals = cases[i].balance;
        meta.runs_played = META_MAX_COUNTERS;
        meta.clears = INT32_MAX;
        bool settled = false;
        TEST_ASSERT_TRUE(meta_settle_result(&meta, &run, &settled, NULL));
        TEST_ASSERT_TRUE(settled);
        TEST_ASSERT_EQUAL_INT32(cases[i].expected, meta.ink_crystals);
        TEST_ASSERT_EQUAL_INT32(META_MAX_COUNTERS, meta.runs_played);
        TEST_ASSERT_EQUAL_INT32(META_MAX_COUNTERS, meta.clears);
        TEST_ASSERT_EQUAL_INT32(META_MAX_SCORE, meta.best_score);
        TEST_ASSERT_EQUAL_INT32(META_MAX_BEST_ACT, meta.best_act);
    }
}

static void test_one_settlement_per_run(void)
{
    MetaState meta = saved;
    const MetaRunResult run = {.acts_cleared = 3,
                               .endless_acts_cleared = 2,
                               .reached_act = 5,
                               .score = 100,
                               .kills = 10,
                               .max_chain = 2,
                               .thunderbursts = 3,
                               .cleared = true,
                               .crystal_bonus_x15 = true};
    bool settled = false;
    MetaSettleBreakdown out;
    TEST_ASSERT_TRUE(meta_settle_result(&meta, &run, &settled, &out));
    TEST_ASSERT_TRUE(settled);
    assert_breakdown((MetaSettleBreakdown){120, 2, 4, 3, 80, 104, 313}, out);
    MetaState expected = saved;
    expected.best_act = 5;
    expected.ink_crystals += 313;
    expected.runs_played++;
    expected.clears++;
    assert_meta(&expected, &meta);

    TEST_ASSERT_FALSE(meta_settle_result(&meta, &run, &settled, &out));
    TEST_ASSERT_TRUE(settled);
    assert_breakdown((MetaSettleBreakdown){0}, out);
    assert_meta(&expected, &meta);

    settled = false;
    TEST_ASSERT_TRUE(meta_settle_result(&meta, &(MetaRunResult){.score = 45678}, &settled, NULL));
    expected.best_score = 45678;
    expected.runs_played++;
    assert_meta(&expected, &meta);
}

static void test_null_settlement_inputs(void)
{
    MetaState meta = saved;
    const MetaRunResult run = {.acts_cleared = 1};
    bool settled = false;
    const MetaSettleBreakdown sentinel = {1, 2, 3, 4, 5, 6, 7};
    MetaSettleBreakdown out = sentinel;
    TEST_ASSERT_FALSE(meta_settle_result(NULL, &run, &settled, &out));
    TEST_ASSERT_FALSE(settled);
    assert_breakdown((MetaSettleBreakdown){0}, out);
    out = sentinel;
    TEST_ASSERT_FALSE(meta_settle_result(&meta, NULL, &settled, &out));
    TEST_ASSERT_FALSE(settled);
    assert_breakdown((MetaSettleBreakdown){0}, out);
    assert_meta(&saved, &meta);
    out = sentinel;
    TEST_ASSERT_FALSE(meta_settle_result(&meta, &run, NULL, &out));
    assert_breakdown((MetaSettleBreakdown){0}, out);
    assert_meta(&saved, &meta);
}

static void test_hint_bit_bounds(void)
{
    MetaState meta = defaults;
    TEST_ASSERT_FALSE(meta_is_hint_seen(&meta, 0));
    meta_mark_hint_seen(&meta, 0);
    TEST_ASSERT_TRUE(meta_is_hint_seen(&meta, 0));
    TEST_ASSERT_FALSE(meta_is_hint_seen(&meta, 31));
    meta_mark_hint_seen(&meta, 31);
    TEST_ASSERT_TRUE(meta_is_hint_seen(&meta, 31));
    const uint32_t invalid[] = {32, UINT32_MAX};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        meta_mark_hint_seen(&meta, invalid[i]);
        TEST_ASSERT_FALSE(meta_is_hint_seen(&meta, invalid[i]));
        TEST_ASSERT_EQUAL_HEX32(UINT32_C(0x80000001), meta.seen_hints_mask);
    }
}

static void test_serialized_save_matches_architecture_example(void)
{
    const MetaState example = {.version = 1,
                               .ink_crystals = 120,
                               .upgrade_lv = {1, 2, 1, 0},
                               .unlocked_relic_mask = 3,
                               .best_score = 14850,
                               .best_act = 3,
                               .runs_played = 5,
                               .clears = 1,
                               .seen_hints_mask = 15,
                               .settings = {80, 70, 85, 100, true, true, UI_LANG_AUTO, false, 100}};
    const char golden[] = "version=1\n"
                          "ink_crystals=120\n"
                          "upg_shrine_hp=1\n"
                          "upg_ink_max=2\n"
                          "upg_rerolls=1\n"
                          "upg_ink_regen=0\n"
                          "unlocked_relic_mask=0x0000000000000003\n"
                          "best_score=14850\n"
                          "best_act=3\n"
                          "runs_played=5\n"
                          "clears=1\n"
                          "seen_hints_mask=0x0000000f\n"
                          "opt_master_vol=80\n"
                          "opt_bgm_vol=70\n"
                          "opt_se_vol=85\n"
                          "opt_shake_scale=100\n"
                          "opt_post_shader=1\n"
                          "opt_hints=1\n"
                          "opt_lang=0\n"
                          "opt_reduce_flash=0\n"
                          "opt_cursor_scale=100\n";
    char buf[META_SERIALIZE_CAP];
    const size_t length = sizeof golden - 1;
    TEST_ASSERT_EQUAL_size_t(length, meta_serialize(&example, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING(golden, buf);
    TEST_ASSERT_EQUAL_size_t(length, meta_serialize(&example, NULL, 0));
    TEST_ASSERT_EQUAL_size_t(length, meta_serialize(&example, NULL, sizeof buf));

    char truncated[16];
    memset(truncated, '!', sizeof truncated);
    TEST_ASSERT_EQUAL_size_t(length, meta_serialize(&example, truncated, sizeof truncated));
    TEST_ASSERT_EQUAL_CHAR_ARRAY(golden, truncated, sizeof truncated - 1);
    TEST_ASSERT_EQUAL_CHAR('\0', truncated[sizeof truncated - 1]);

    /* P1 save backward compatibility: omitting the 3 new opt_* keys restores P2 defaults. */
    const char p1_save[] = "version=1\n"
                           "ink_crystals=120\n"
                           "upg_shrine_hp=1\n"
                           "upg_ink_max=2\n"
                           "upg_rerolls=1\n"
                           "upg_ink_regen=0\n"
                           "unlocked_relic_mask=0x0000000000000003\n"
                           "best_score=14850\n"
                           "best_act=3\n"
                           "runs_played=5\n"
                           "clears=1\n"
                           "seen_hints_mask=0x0000000f\n"
                           "opt_master_vol=80\n"
                           "opt_bgm_vol=70\n"
                           "opt_se_vol=85\n"
                           "opt_shake_scale=100\n"
                           "opt_post_shader=1\n"
                           "opt_hints=1\n";
    MetaState restored = saved;
    TEST_ASSERT_TRUE(meta_deserialize(&restored, p1_save));
    assert_meta(&example, &restored);

    /* opt_cursor_scale threshold normalization (<113 -> 100, 113..137 -> 125, >=138 -> 150). */
    const struct {
        const char *text;
        uint8_t expected;
    } cursor_cases[] = {
        {"version=1\nopt_cursor_scale=-10", 100}, {"version=1\nopt_cursor_scale=112", 100},
        {"version=1\nopt_cursor_scale=113", 125}, {"version=1\nopt_cursor_scale=137", 125},
        {"version=1\nopt_cursor_scale=138", 150}, {"version=1\nopt_cursor_scale=250", 150},
    };
    for (size_t i = 0; i < sizeof cursor_cases / sizeof cursor_cases[0]; ++i) {
        MetaState m;
        TEST_ASSERT_TRUE_MESSAGE(meta_deserialize(&m, cursor_cases[i].text), cursor_cases[i].text);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(cursor_cases[i].expected, m.settings.cursor_scale,
                                        cursor_cases[i].text);
    }
}

static void test_all_persisted_fields_round_trip(void)
{
    char buf[META_SERIALIZE_CAP];
    TEST_ASSERT_TRUE(meta_serialize(&saved, buf, sizeof buf) < sizeof buf);
    MetaState restored;
    TEST_ASSERT_TRUE(meta_deserialize(&restored, buf));
    assert_meta(&saved, &restored);
}

static void test_save_syntax_and_missing_defaults(void)
{
    const char text[] = "  \t# ignored comment\r\n\t\r\n"
                        "future_field=not=a=number and longer than thirty-one bytes\r\n"
                        "empty_future_field=\r\n"
                        "ink_crystals=123\r\n"
                        "\tversion \t=\t1  \r\n"
                        "  ink_crystals\t =  010\t";
    MetaState meta = saved;
    TEST_ASSERT_TRUE(meta_deserialize(&meta, text));
    MetaState expected = defaults;
    expected.ink_crystals = 10;
    assert_meta(&expected, &meta);
}

static void test_save_clamps_and_normalizes_each_field(void)
{
    const char text[] = "version=1\nink_crystals=99999999999\nupg_shrine_hp=9\nupg_ink_max=-9\n"
                        "upg_rerolls=9\nupg_ink_regen=9\nunlocked_relic_mask=0Xfffffffb\n"
                        "best_score=9999999999999999999999999999999\nbest_act=99999999999\n"
                        "runs_played=99999999999\nclears=-1\nseen_hints_mask=0x123456789abcdef0\n"
                        "opt_master_vol=150\nopt_bgm_vol=-1\nopt_se_vol=101\nopt_shake_scale=74\n"
                        "opt_post_shader=0\nopt_hints=-4\n";
    const MetaState expected = {
        .version = 1,
        .ink_crystals = META_MAX_CRYSTALS,
        .upgrade_lv = {3, 0, 2, 3},
        .unlocked_relic_mask = 0x3b,
        .best_score = META_MAX_SCORE,
        .best_act = META_MAX_BEST_ACT,
        .runs_played = META_MAX_COUNTERS,
        .seen_hints_mask = UINT32_C(0x9abcdef0),
        .settings = {100, 0, 100, 50, false, true, UI_LANG_AUTO, false, 100}};
    MetaState meta;
    TEST_ASSERT_TRUE(meta_deserialize(&meta, text));
    assert_meta(&expected, &meta);

    const char extremes[] =
        "version=1\nink_crystals=-999999999999999999999999999999\n"
        "unlocked_relic_mask=010\nseen_hints_mask=9999999999999999999999999999999";
    TEST_ASSERT_TRUE(meta_deserialize(&meta, extremes));
    TEST_ASSERT_EQUAL_INT32(0, meta.ink_crystals);
    TEST_ASSERT_EQUAL_HEX64(10, meta.unlocked_relic_mask);
    TEST_ASSERT_EQUAL_HEX32(UINT32_MAX, meta.seen_hints_mask);
}

static void test_shake_scale_thresholds(void)
{
    const struct {
        const char *text;
        uint8_t expected;
    } cases[] = {{"version=1\nopt_shake_scale=24", 0},    {"version=1\nopt_shake_scale=25", 50},
                 {"version=1\nopt_shake_scale=74", 50},   {"version=1\nopt_shake_scale=75", 100},
                 {"version=1\nopt_shake_scale=200", 100}, {"version=1\nopt_shake_scale=-5", 0}};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        MetaState meta;
        TEST_ASSERT_TRUE_MESSAGE(meta_deserialize(&meta, cases[i].text), cases[i].text);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(cases[i].expected, meta.settings.shake_scale,
                                        cases[i].text);
    }
}

static void test_invalid_saves_reset_every_field(void)
{
    const char *const invalid[] = {"version=2",
                                   "ink_crystals=15",
                                   "version=1\nink_crystals=12abc",
                                   "version=1\nunknown",
                                   "version=1\nunlocked_relic_mask=-1",
                                   "version=1\nseen_hints_mask=-0",
                                   "version=1\nink_crystals=42\nopt_master_vol= \t",
                                   "version=1\n \t=15",
                                   "version=1\nbest_score=00000000000000000000000000000000",
                                   "version=1\nink_crystals=0x10",
                                   "version=1\nseen_hints_mask=0x",
                                   "",
                                   NULL};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        MetaState meta = saved;
        UNITY_SET_DETAIL(invalid[i]);
        TEST_ASSERT_FALSE(meta_deserialize(&meta, invalid[i]));
        assert_meta(&defaults, &meta);
    }
}

static void test_null_meta_inputs(void)
{
    meta_init_default(NULL);
    TEST_ASSERT_EQUAL_INT32(-1, meta_upgrade_next_cost(NULL, META_UPG_INK_MAX));
    TEST_ASSERT_FALSE(meta_buy_upgrade(NULL, META_UPG_INK_MAX));
    TEST_ASSERT_FALSE(meta_buy_relic_unlock(NULL, 0));
    TEST_ASSERT_FALSE(meta_is_relic_unlocked(NULL, 0));
    meta_mark_hint_seen(NULL, 0);
    TEST_ASSERT_FALSE(meta_is_hint_seen(NULL, 0));
    char buf[] = "x";
    TEST_ASSERT_EQUAL_size_t(0, meta_serialize(NULL, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_FALSE(meta_deserialize(NULL, "version=1"));
}

static void enter_reward_phase(GameState *g)
{
    game_debug_start_wave(g, 0);
    g->phase = RUN_PHASE_REWARD;
    g->phase_t = 0.0f;
    rewards_generate_offer(g);
}

static void test_run_reward_offer_rules(void)
{
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, seed);
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);

        enter_reward_phase(g);
        const RewardOffer *off = &g->run.reward;
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_UPGRADE, off->kind);
        TEST_ASSERT_EQUAL_INT32(3, off->choice_count);

        /* All 3 choices distinct and below max level. */
        TEST_ASSERT_NOT_EQUAL_INT32(off->choice_ids[0], off->choice_ids[1]);
        TEST_ASSERT_NOT_EQUAL_INT32(off->choice_ids[0], off->choice_ids[2]);
        TEST_ASSERT_NOT_EQUAL_INT32(off->choice_ids[1], off->choice_ids[2]);

        bool has_recommended = false;
        UpgradeAxis axes[3];
        for (int i = 0; i < 3; ++i) {
            const UpgradeId id = (UpgradeId)off->choice_ids[i];
            TEST_ASSERT_TRUE(game_upgrade_level(g, id) < game_upgrade_max_level(id));
            axes[i] = game_upgrade_axis(id);
            if (game_is_recommended_axis(BRUSH_STYLE_HEAVY, axes[i])) {
                has_recommended = true;
            }
        }
        TEST_ASSERT_TRUE(has_recommended);
        /* At least 2 distinct axes among the 3 choices. */
        TEST_ASSERT_TRUE(axes[0] != axes[1] || axes[0] != axes[2] || axes[1] != axes[2]);

        /* Maxed (Lv3) upgrade must never be offered. */
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        }
        rewards_generate_offer(g);
        for (int i = 0; i < off->choice_count; ++i) {
            TEST_ASSERT_NOT_EQUAL_INT32((int32_t)UPG_EMBER_TIP, off->choice_ids[i]);
        }

        game_destroy(g);
    }
}

static void test_run_reward_reproducibility(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x4242));
    GameState *a = game_create(&cfg);
    GameState *b = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    enter_reward_phase(a);
    enter_reward_phase(b);
    TEST_ASSERT_EQUAL_UINT32(a->run.reward.serial, b->run.reward.serial);
    TEST_ASSERT_EQUAL_INT32_ARRAY(a->run.reward.choice_ids, b->run.reward.choice_ids,
                                  GAME_MAX_REWARD_CHOICES);

    const GameCommand reroll = {.type = CMD_REROLL_REWARD, .arg = 0};
    game_step(a, NULL, &reroll);
    game_step(b, NULL, &reroll);
    TEST_ASSERT_EQUAL_UINT32(a->run.reward.serial, b->run.reward.serial);
    TEST_ASSERT_EQUAL_INT32_ARRAY(a->run.reward.choice_ids, b->run.reward.choice_ids,
                                  GAME_MAX_REWARD_CHOICES);

    const GameCommand pick = {.type = CMD_PICK_REWARD, .arg = 1};
    game_step(a, NULL, &pick);
    game_step(b, NULL, &pick);
    TEST_ASSERT_EQUAL_HEX64(game_hash(a), game_hash(b));

    game_destroy(b);
    game_destroy(a);
}

static void test_run_reward_reroll(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    enter_reward_phase(g);

    TEST_ASSERT_EQUAL_INT32(1, g->run.rerolls_left);
    const uint32_t serial0 = g->run.reward.serial;
    const GameCommand reroll = {.type = CMD_REROLL_REWARD, .arg = 0};
    game_step(g, NULL, &reroll);

    TEST_ASSERT_EQUAL_INT32(0, g->run.rerolls_left);
    TEST_ASSERT_EQUAL_UINT32(serial0 + 1u, g->run.reward.serial);
    TEST_ASSERT_EQUAL_INT32(1, g->event_count);
    TEST_ASSERT_EQUAL_INT(EV_REWARD_REROLL, g->events[0].type);
    TEST_ASSERT_EQUAL_INT32(0, g->events[0].value);

    /* With 0 rerolls left, another reroll is a strict no-op. */
    game_step(g, NULL, NULL);
    const uint64_t h_before = game_hash(g);
    game_step(g, NULL, &reroll);
    TEST_ASSERT_EQUAL_HEX64(h_before, game_hash(g));

    game_destroy(g);
}

static void test_run_reward_repair_shrine(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* Full HP: repair is not available and command is a no-op. */
    enter_reward_phase(g);
    TEST_ASSERT_FALSE(g->run.reward.repair_available);
    game_step(g, NULL, NULL);
    const uint64_t h_full = game_hash(g);
    const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
    game_step(g, NULL, &repair);
    TEST_ASSERT_EQUAL_HEX64(h_full, game_hash(g));
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_REWARD, g->phase);

    /* Damaged shrine: repair heals +1, emits EV_REWARD_PICK(-1, EVF_REWARD_REPAIR), starts next
     * wave. */
    g->shrines[0].hp = g->shrines[0].max_hp - 2;
    rewards_generate_offer(g);
    TEST_ASSERT_TRUE(g->run.reward.repair_available);
    const int32_t hp_before = g->shrines[0].hp;

    game_step(g, NULL, &repair);
    TEST_ASSERT_EQUAL_INT32(hp_before + 1, g->shrines[0].hp);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    bool found_repair_ev = false;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_REWARD_PICK && g->events[i].value == -1 &&
            (g->events[i].flags & EVF_REWARD_REPAIR) != 0) {
            found_repair_ev = true;
        }
    }
    TEST_ASSERT_TRUE(found_repair_ev);

    game_destroy(g);
}

static void test_run_reward_invalid_commands_noop(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    enter_reward_phase(g);
    game_step(g, NULL, NULL);

    const uint64_t h_reward = game_hash(g);
    const uint64_t rng_reward = g->rng;
    const GameCommand invalid_cmds[] = {
        {.type = CMD_PICK_REWARD, .arg = -1},
        {.type = CMD_PICK_REWARD, .arg = g->run.reward.choice_count},
        {.type = CMD_PICK_REWARD, .arg = 3},
        {.type = CMD_START_ENDLESS, .arg = 0},
        {.type = CMD_PICK_ENDLESS_CRYSTAL, .arg = 0},
    };
    for (size_t i = 0; i < sizeof invalid_cmds / sizeof invalid_cmds[0]; ++i) {
        game_step(g, NULL, &invalid_cmds[i]);
        TEST_ASSERT_EQUAL_HEX64(h_reward, game_hash(g));
        TEST_ASSERT_EQUAL_HEX64(rng_reward, g->rng);
    }

    /* CMD_PICK_REWARD during PLAYING is also a no-op compared to CMD_NONE. */
    GameState *p1 = game_create(NULL);
    GameState *p2 = game_create(NULL);
    game_debug_start_wave(p1, 0);
    game_debug_start_wave(p2, 0);
    const GameCommand pick0 = {.type = CMD_PICK_REWARD, .arg = 0};
    game_step(p1, NULL, NULL);
    game_step(p2, NULL, &pick0);
    TEST_ASSERT_EQUAL_HEX64(game_hash(p1), game_hash(p2));

    game_destroy(p2);
    game_destroy(p1);
    game_destroy(g);
}

static void test_run_reward_candidate_shortage(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    /* Max out 7 upgrades -> only 1 choice offered. */
    for (int id = 0; id < (int)UPG_COUNT - 1; ++id) {
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, (UpgradeId)id));
        }
    }
    enter_reward_phase(g);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_UPGRADE, g->run.reward.kind);
    TEST_ASSERT_EQUAL_INT32(1, g->run.reward.choice_count);
    TEST_ASSERT_EQUAL_INT32((int32_t)(UPG_COUNT - 1), g->run.reward.choice_ids[0]);

    /* Max out the 8th upgrade -> REWARD_KIND_FALLBACK, 0 choices, repair_available even at full HP.
     */
    for (int lv = 0; lv < 3; ++lv) {
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, (UpgradeId)(UPG_COUNT - 1)));
    }
    g->shrines[0].hp = g->shrines[0].max_hp;
    rewards_generate_offer(g);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
    TEST_ASSERT_EQUAL_INT32(0, g->run.reward.choice_count);
    TEST_ASSERT_TRUE(g->run.reward.repair_available);

    const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
    game_step(g, NULL, &repair);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT32(1, g->run.wave_index);
    TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp, g->shrines[0].hp);

    game_destroy(g);
}

static void test_run_reward_empty_filter_fallback(void)
{
    /* 0 owned upgrades: filter 2.4 (< 0.4 owned) falls back cleanly and yields 3 choices. */
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, seed);
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_EQUAL_INT32(0, g->run.upgrade_count);
        enter_reward_phase(g);
        TEST_ASSERT_EQUAL_INT32(3, g->run.reward.choice_count);
        game_destroy(g);
    }

    /* All recommended-axis upgrades maxed on first offer: filter 2.2 falls back and yields 3
     * choices. */
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(77));
    GameState *g = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(g);
    for (int id = 0; id < (int)UPG_COUNT; ++id) {
        if (game_is_recommended_axis(BRUSH_STYLE_HEAVY, game_upgrade_axis((UpgradeId)id))) {
            for (int lv = 0; lv < 3; ++lv) {
                TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, (UpgradeId)id));
            }
        }
    }
    enter_reward_phase(g);
    TEST_ASSERT_EQUAL_INT(REWARD_KIND_UPGRADE, g->run.reward.kind);
    TEST_ASSERT_EQUAL_INT32(3, g->run.reward.choice_count);
    game_destroy(g);
}

static void test_run_modifiers_calculation(void)
{
    RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(1));
    GameState *g = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(g);

    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_GREAT_VORTEX));
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_GREAT_VORTEX));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.30f, g->internal.mods.cyclone_radius_mul);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.4f, g->internal.mods.cyclone_duration_add);

    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_LASTING_SURGE));
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_LASTING_SURGE));
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_LASTING_SURGE));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 6.6f, g->internal.mods.surge_duration);
    game_destroy(g);

    cfg.meta_upgrade_lv[META_UPG_INK_MAX] = 2;
    GameState *g_meta = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_meta);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 120.0f, g_meta->player.ink_max);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 120.0f, g_meta->player.ink);
    game_destroy(g_meta);
}

static void clear_test_entities(GameState *g)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
}

static int count_ev(const GameState *g, GameEventType type)
{
    int n = 0;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == type) {
            ++n;
        }
    }
    return n;
}

static float sample_channel(const GameState *g, int ch, float x, float y)
{
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, x, y, NULL, NULL, dye);
    return dye[ch];
}

static void test_brush_styles_and_stacking(void)
{
    /* 1. Heavy Brush */
    {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(11));
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.25f, g->internal.mods.brush_radius_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.20f, g->internal.mods.brush_force_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.15f, g->internal.mods.stroke_cost_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.25f, g->internal.mods.gale_impulse_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.mods.stroke_ember_interval);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 160.0f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 120.0f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 80.0f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, g->internal.mods.stroke_ember_mul);
        game_destroy(g);
    }

    /* 2. Ember Brush: 72px ember (non-dry only), gale fire +0.35, Ember Tip stacking without
     * double injection. */
    {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_EMBER, UINT64_C(12));
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.12f, g->internal.mods.stroke_cost_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 72.0f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 3.0f, g->internal.mods.stroke_ember_rad);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.64f, g->internal.mods.stroke_ember_amt);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.35f, g->internal.mods.gale_fire_add);

        /* A 160px stroke lays embers where the 72px marks fall along the segment (272, 344), not
         * in one blob at the brush tip. */
        const BrushInput stroke1 = {
            .pmx = 200.0f, .pmy = 300.0f, .mx = 360.0f, .my = 300.0f, .paint = true};
        player_skills_step(g, &stroke1);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 272.0f, 300.0f) > 0.10f);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 344.0f, 300.0f) > 0.10f);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 16.0f, g->internal.ember_tip_dist);

        /* Dry stroke of 150px must NOT inject fire. */
        fluid_reset(&g->fluid);
        g->internal.ember_tip_dist = 0.0f;
        g->player.ink = 10.0f;
        const BrushInput dry_stroke = {
            .pmx = 200.0f, .pmy = 300.0f, .mx = 350.0f, .my = 300.0f, .paint = true};
        player_skills_step(g, &dry_stroke);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sample_channel(g, 1, 350.0f, 300.0f));

        /* Gale injects fire (0.35) along its trajectory. */
        fluid_reset(&g->fluid);
        g->player.ink = 100.0f;
        g->player.gale_cd = 0.0f;
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        const BrushInput gale_in = {
            .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .gale = true};
        player_skills_step(g, &gale_in);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 460.0f, 300.0f) > 0.10f);

        /* Ember Brush x Ember Tip Lv1/2/3: 54 / 40.5 / 30.375px, no double injection. */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 54.0f, g->internal.mods.stroke_ember_interval);
        fluid_reset(&g->fluid);
        g->internal.ember_tip_dist = 0.0f;
        g->player.ink = 100.0f;
        const BrushInput st95 = {
            .pmx = 200.0f, .pmy = 200.0f, .mx = 295.0f, .my = 200.0f, .paint = true};
        player_skills_step(g, &st95);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 41.0f, g->internal.ember_tip_dist);
        const float fire_once = sample_channel(g, 1, 254.0f, 200.0f);
        TEST_ASSERT_TRUE(fire_once > 0.10f && fire_once < 0.80f);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 40.5f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_EMBER_TIP));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 30.375f, g->internal.mods.stroke_ember_interval);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, g->internal.mods.stroke_ember_mul);

        /* Ember Brush x Flame Gale: 0.35 + 0.25 / 0.35 / 0.50 */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_FLAME_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.60f, g->internal.mods.gale_fire_add);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_FLAME_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.70f, g->internal.mods.gale_fire_add);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_FLAME_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.85f, g->internal.mods.gale_fire_add);
        game_destroy(g);
    }

    /* 3. Quill Brush: CD 1.1s, storm 0.72 + sustain 0.18, Charged Seal at its normal strength,
     * Ink Magnet x1.3. */
    {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_QUILL, UINT64_C(13));
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.80f, g->internal.mods.brush_radius_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.90f, g->internal.mods.brush_force_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.90f, g->internal.mods.stroke_cost_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.10f, g->internal.mods.cyclone_cd);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.72f, g->internal.mods.cyclone_storm_add);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.18f, g->internal.mods.cyclone_storm_sustain);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 169.0f, g->internal.mods.orb_pull_radius);

        const BrushInput cyc_in = {
            .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .cyclone = true};
        player_skills_step(g, &cyc_in);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.10f, g->player.cyclone_cd);
        TEST_ASSERT_TRUE(sample_channel(g, 2, 420.0f, 300.0f) > 0.05f);

        /* Quill x Charged Seal Lv1/2/3: 1.02 / 1.17 / 1.32 + Lv3 sustain +0.60 */
        const float expected_storm[3] = {1.02f, 1.17f, 1.32f};
        for (int lv = 1; lv <= 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_CHARGED_SEAL));
            float storm_add = g->internal.mods.cyclone_storm_add;
            float storm_sustain = g->internal.mods.cyclone_storm_sustain;
            upgrades_fire_on_cyclone(g, 400.0f, 300.0f, &storm_add, &storm_sustain);
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, expected_storm[lv - 1], storm_add);
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, lv >= 3 ? 0.78f : 0.18f, storm_sustain);
        }

        /* Quill x Ink Magnet Lv1/2/3: 221 / 273 / 325px */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INK_MAGNET));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 170.0f * 1.3f, g->internal.mods.orb_pull_radius);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INK_MAGNET));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 210.0f * 1.3f, g->internal.mods.orb_pull_radius);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INK_MAGNET));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 325.0f, g->internal.mods.orb_pull_radius);
        game_destroy(g);
    }
}

static void test_p1_upgrades_all_16(void)
{
    /* Flame Gale Lv3 endpoint blast */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_FLAME_GALE));
        }
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.50f, g->internal.mods.gale_fire_add);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 20.0f, g->internal.mods.gale_end_blast_dmg);

        const EntityId e = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 300.0f + 220.0f, 300.0f);
        fluid_reset(&g->fluid);
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        const BrushInput in = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 300.0f, .my = 300.0f, .gale = true};
        player_skills_step(g, &in);
        TEST_ASSERT_EQUAL_INT32(70, game_health(g, e)->hp);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 520.0f, 300.0f) > 0.25f);
        game_destroy(g);
    }

    /* Inferno Lv1/2/3 burn dmg + Lv3 fire-kill ember splat */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        const float expected[3] = {14.0f, 18.0f, 22.0f};
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INFERNO));
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, expected[lv], g->internal.mods.burn_base_dmg);
        }
        TEST_ASSERT_TRUE(g->internal.mods.inferno_ember_on_kill);
        const EntityId e = game_debug_spawn_enemy(g, ENEMY_ARCHER, 400.0f, 300.0f);
        fluid_reset(&g->fluid);
        (void)combat_apply_damage(g, e, 25, DMG_FIRE, 0.0f, 0.0f);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 400.0f, 300.0f) > 0.50f);
        game_destroy(g);
    }

    /* Phoenix Arrow + Thunder Arrow + Swift Thunder + Mighty Stroke + Tailwind + Chain Primer +
     * Deep Ink + Deep Mark */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        for (int lv = 1; lv <= 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_PHOENIX_ARROW));
            TEST_ASSERT_EQUAL_INT32(lv + 2, g->internal.mods.burning_proj_pierce);
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_THUNDER_ARROW));
            TEST_ASSERT_EQUAL_INT32(lv == 1 ? 2 : 3, g->internal.mods.thunder_proj_targets);
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_SWIFT_THUNDER));
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MIGHTY_STROKE));
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_TAILWIND));
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_CHAIN_PRIMER));
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DEEP_INK));
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DEEP_MARK));
        }
        TEST_ASSERT_TRUE(g->internal.mods.burning_proj_hit_splat);
        TEST_ASSERT_TRUE(g->internal.mods.deflect_auto_electrify);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.30f, g->internal.mods.storm_zap_cd);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.20f * 1.45f, g->internal.mods.brush_force_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.00f, g->internal.mods.homing_steer_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.20f, g->internal.mods.deflected_max_speed_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 4.50f, g->internal.mods.chain_grace);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 145.0f, g->player.ink_max);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.60f, g->internal.mods.ink_mark_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 6.0f, g->internal.mods.ink_mark_duration);

        /* Gale deflect auto-electrifies PROJ_ARROW when Thunder Arrow Lv3 is owned. */
        const EntityId p =
            game_debug_spawn_projectile(g, PROJ_ARROW, 380.0f, 300.0f, -150.0f, 0.0f);
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        const BrushInput gin = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 300.0f, .my = 300.0f, .gale = true};
        player_skills_step(g, &gin);
        TEST_ASSERT_TRUE(game_proj(g, p)->deflected);
        TEST_ASSERT_TRUE(game_proj(g, p)->electrified);
        game_destroy(g);
    }

    /* Maelstrom Bind Lv1 (light), Lv2 (medium), Lv3 (collision 40 dmg + no re-trigger) */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
        const EntityId light_e = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 460.0f, 300.0f);
        const EntityId med_e = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 460.0f, 340.0f);
        g->internal.cyclones[0] = (ActiveCyclone){.active = true,
                                                  .x = 400.0f,
                                                  .y = 300.0f,
                                                  .rad_cells = 13.0f,
                                                  .life = 0.5f,
                                                  .max_life = 0.72f};
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(g->internal.maelstrom_bind_t[light_e.index] > 0.5f);
        TEST_ASSERT_TRUE(game_status(g, light_e)->stun_t >= 0.15f - 1e-5f);
        TEST_ASSERT_TRUE(game_velocity(g, light_e)->vx < 0.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.maelstrom_bind_t[med_e.index]);

        /* Lv2 binds medium too. */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(g->internal.maelstrom_bind_t[med_e.index] > 0.5f);

        /* Lv3: two bound enemies within 32px collide -> 40 dmg each, bind consumed, no repeat. */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
        clear_test_entities(g);
        g->internal.cyclones[0].active = false;
        const EntityId a = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 400.0f, 300.0f);
        const EntityId b = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 420.0f, 300.0f);
        game_health_mut(g, a)->hp = 100;
        game_health_mut(g, b)->hp = 100;
        g->internal.maelstrom_bind_t[a.index] = 1.0f;
        g->internal.maelstrom_bind_t[b.index] = 1.0f;

        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_EQUAL_INT32(60, game_health(g, a)->hp);
        TEST_ASSERT_EQUAL_INT32(60, game_health(g, b)->hp);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.maelstrom_bind_t[a.index]);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.maelstrom_bind_t[b.index]);

        /* Second tick must not deal collision damage again. */
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_EQUAL_INT32(60, game_health(g, a)->hp);
        TEST_ASSERT_EQUAL_INT32(60, game_health(g, b)->hp);
        game_destroy(g);
    }

    /* Ink Magnet Lv3: cyclone pulls orbs across the entire screen (> 220px) at 360px/s. */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INK_MAGNET));
        }
        const EntityId orb = game_debug_spawn_orb(g, 900.0f, 300.0f);
        game_velocity_mut(g, orb)->vx = 0.0f;
        game_velocity_mut(g, orb)->vy = 0.0f;
        const BrushInput cyc = {
            .pmx = 200.0f, .pmy = 300.0f, .mx = 200.0f, .my = 300.0f, .cyclone = true};
        player_skills_step(g, &cyc);
        TEST_ASSERT_FLOAT_WITHIN(1.0f, -360.0f, game_velocity(g, orb)->vx);
        game_destroy(g);
    }

    /* Dry Mastery Lv1/2/3: dry zero cost, painting at ink == 0, dry deflected dmg x1.2. */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DRY_MASTERY));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.75f, g->internal.mods.dry_force_override);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DRY_MASTERY));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.00f, g->internal.mods.dry_force_override);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DRY_MASTERY));
        TEST_ASSERT_TRUE(g->internal.mods.dry_zero_cost);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.20f, g->internal.mods.dry_deflect_dmg_mul);

        g->player.ink = 0.0f;
        const BrushInput st = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 340.0f, .my = 300.0f, .paint = true};
        player_skills_step(g, &st);
        TEST_ASSERT_TRUE(g->player.painting);
        TEST_ASSERT_TRUE(g->player.dry);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->player.ink);

        g->player.ink = 15.0f;
        player_skills_step(g, &st);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 15.0f, g->player.ink);

        const EntityId e = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 500.0f, 300.0f);
        const int32_t dealt = combat_apply_damage(g, e, 20, DMG_DEFLECTED | DMG_PHYSICAL, 0, 0);
        TEST_ASSERT_EQUAL_INT32(24, dealt);
        game_destroy(g);
    }

    /* Shrine Ward Lv1/2/3: max_hp +1/+2/+3 and immediate +1 heal after recompute. */
    {
        GameState *g = game_create(NULL);
        g->shrines[0].hp = 6;
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_SHRINE_WARD));
        TEST_ASSERT_EQUAL_INT32(9, g->shrines[0].max_hp);
        TEST_ASSERT_EQUAL_INT32(7, g->shrines[0].hp);
        g->shrines[0].hp = 9;
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_SHRINE_WARD));
        TEST_ASSERT_EQUAL_INT32(10, g->shrines[0].max_hp);
        TEST_ASSERT_EQUAL_INT32(10, g->shrines[0].hp);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_SHRINE_WARD));
        TEST_ASSERT_EQUAL_INT32(11, g->shrines[0].max_hp);
        TEST_ASSERT_EQUAL_INT32(11, g->shrines[0].hp);
        game_destroy(g);
    }

    /* Barrier Lv1/2/3: invuln +0.3/+0.6/+0.9 and Lv3 12s shield absorbs 1 enemy projectile. */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_BARRIER));
        }
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.90f, g->internal.mods.shrine_invuln_add);
        TEST_ASSERT_TRUE(g->internal.mods.barrier_shield_enabled);

        /* First tick charges the shield immediately (cd == 0). */
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(g->internal.barrier_shield_ready);

        /* Un-deflected projectile hits shrine -> absorbed, shrine HP intact, 12s CD starts. */
        const int32_t hp0 = g->shrines[0].hp;
        (void)game_debug_spawn_projectile(g, PROJ_ARROW, g->shrines[0].x - 10.0f, g->shrines[0].y,
                                          200.0f, 0.0f);
        game_step(g, NULL, NULL);
        TEST_ASSERT_EQUAL_INT32(hp0, g->shrines[0].hp);
        TEST_ASSERT_FALSE(g->internal.barrier_shield_ready);
        TEST_ASSERT_TRUE(g->internal.barrier_shield_cd > 11.9f);

        /* 718 more ticks (< 12s total) -> still not ready; 719th tick -> ready again. */
        for (int t = 0; t < 718; ++t) {
            upgrades_fire_on_tick(g, INK_FIXED_DT);
        }
        TEST_ASSERT_FALSE(g->internal.barrier_shield_ready);
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(g->internal.barrier_shield_ready);
        game_destroy(g);
    }
}

static void test_p1_relics_all_9(void)
{
    /* 1. Vermilion Inkstone + 8. Lingering Ink (dye_damp sync & combination) */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_VERMILION_INKSTONE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.980f, g->fluid.cfg.dye_damp[0]);
        const BrushInput st205 = {
            .pmx = 200.0f, .pmy = 200.0f, .mx = 405.0f, .my = 200.0f, .paint = true};
        player_skills_step(g, &st205);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 405.0f, 200.0f) > 0.10f);

        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_LINGERING_INK));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.988f, g->fluid.cfg.dye_damp[0]);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 5.0f, g->internal.mods.ink_mark_duration);
        game_destroy(g);

        GameState *g2 = game_create(NULL);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g2, RELIC_LINGERING_INK));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.993f, g2->fluid.cfg.dye_damp[0]);
        game_destroy(g2);
    }

    /* 2. Thunder Drum: cyclone lightning 40 dmg + storm 1.0 + EV_STORM_ZAP */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_THUNDER_DRUM));
        const EntityId e = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 420.0f, 300.0f);
        fluid_reset(&g->fluid);
        g->event_count = 0;
        const BrushInput cyc = {
            .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .cyclone = true};
        player_skills_step(g, &cyc);
        TEST_ASSERT_EQUAL_INT32(50, game_health(g, e)->hp);
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_STORM_ZAP));
        TEST_ASSERT_TRUE(sample_channel(g, 2, 400.0f, 300.0f) > 0.25f);
        game_destroy(g);
    }

    /* 3. Tengu Fan: 3-way gale, no duplicate knockback on single enemy, +6 ink cost */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_TENGU_FAN));
        const EntityId e = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 400.0f, 300.0f);
        game_velocity_mut(g, e)->vx = 0.0f;
        game_velocity_mut(g, e)->vy = 0.0f;
        g->player.ink = 100.0f;
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        g->event_count = 0;
        const BrushInput gin = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 300.0f, .my = 300.0f, .gale = true};
        player_skills_step(g, &gin);
        TEST_ASSERT_EQUAL_INT(3, count_ev(g, EV_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 78.0f, g->player.ink);
        /* Fan knockback keeps 0.78 of the shove: 560 * 0.78 * 1.0 * 1.25 = 546 px/s (applied ONCE).
         */
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 546.0f, game_velocity(g, e)->vx);
        game_destroy(g);
    }

    /* 4. Ink-Dyed Shrine: emits ink around shrine every tick */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_INK_DYED_SHRINE));
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(sample_channel(g, 0, g->shrines[0].x, g->shrines[0].y) > 0.01f);
        game_destroy(g);
    }

    /* 5. Twin Brush: symmetric ink + opposite velocity + cost x1.3 + symmetric ember */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_TWIN_BRUSH));
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_VERMILION_INKSTONE));
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.15f * 1.30f, g->internal.mods.stroke_cost_mul);

        const float sx = g->shrines[0].x;
        const float sy = g->shrines[0].y;
        const BrushInput st = {
            .pmx = 200.0f, .pmy = 200.0f, .mx = 405.0f, .my = 200.0f, .paint = true};
        player_skills_step(g, &st);
        const float sym_x = 2.0f * sx - 405.0f;
        const float sym_y = 2.0f * sy - 200.0f;
        TEST_ASSERT_TRUE(sample_channel(g, 0, 405.0f, 200.0f) > 0.05f);
        TEST_ASSERT_TRUE(sample_channel(g, 0, sym_x, sym_y) > 0.05f);
        TEST_ASSERT_TRUE(sample_channel(g, 1, 405.0f, 200.0f) > 0.05f);
        TEST_ASSERT_TRUE(sample_channel(g, 1, sym_x, sym_y) > 0.05f);
        /* Distance counter accumulated only once (205 - 200 = 5). */
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 5.0f, g->internal.relic_ember_dist);
        game_destroy(g);
    }

    /* 6. Rosary Swirl: normal orb pickup creates swirl around pickup position */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_ROSARY_SWIRL));
        g->player.cursor_x = 400.0f;
        g->player.cursor_y = 300.0f;
        const EntityId orb = game_debug_spawn_orb(g, 400.0f, 300.0f);
        game_transform_mut(g, orb)->x = 400.0f;
        game_transform_mut(g, orb)->y = 300.0f;
        combat_orbs_step(g);
        float fu = 0.0f;
        float fv = 0.0f;
        float dye[FLUID_MAX_CHANNELS] = {0};
        fluid_sample(&g->fluid, 424.0f, 300.0f, &fu, &fv, dye);
        TEST_ASSERT_TRUE(dye[0] > 0.05f);
        TEST_ASSERT_TRUE(fabsf(fv) > 1.0f);
        game_destroy(g);
    }

    /* 7. Pact of Fire: fire dmg x2, shrine max_hp -2, hp clamped to new max_hp */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        TEST_ASSERT_EQUAL_INT32(8, g->shrines[0].max_hp);
        TEST_ASSERT_EQUAL_INT32(8, g->shrines[0].hp);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_PACT_OF_FIRE));
        TEST_ASSERT_EQUAL_INT32(6, g->shrines[0].max_hp);
        TEST_ASSERT_EQUAL_INT32(6, g->shrines[0].hp);
        const EntityId e = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 400.0f, 300.0f);
        TEST_ASSERT_EQUAL_INT32(40, combat_apply_damage(g, e, 20, DMG_FIRE, 0, 0));
        game_destroy(g);
    }

    /* 9. Wind Return: auto free gale on enemy projectile within 54px, 0 ink, 0 player CD, 12s CD */
    {
        GameState *g = game_create(NULL);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_WIND_RETURN));
        const float sx = g->shrines[0].x;
        const float sy = g->shrines[0].y;
        const EntityId p = game_debug_spawn_projectile(g, PROJ_ARROW, sx + 45.0f, sy, -150.0f, 0);
        g->player.ink = 50.0f;
        g->player.gale_cd = 0.0f;
        g->event_count = 0;

        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_GALE));
        TEST_ASSERT_TRUE(game_proj(g, p)->deflected);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 50.0f, g->player.ink);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->player.gale_cd);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f, g->internal.wind_return_cd);

        /* During 12s CD, another nearby projectile does not trigger Wind Return. */
        const EntityId p2 = game_debug_spawn_projectile(g, PROJ_ARROW, sx - 45.0f, sy, 150.0f, 0);
        g->event_count = 0;
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_GALE));
        TEST_ASSERT_FALSE(game_proj(g, p2)->deflected);
        game_destroy(g);
    }
}

static void test_hook_acquisition_order_and_relic_rewards(void)
{
    /* 1. Acquisition order determinism: upgrades run before relics, and different acquisition
     * orders preserve deterministic execution order. */
    {
        const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(99));
        GameState *g = game_create(&cfg);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_CHARGED_SEAL));
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_THUNDER_DRUM));
        g->event_count = 0;
        const BrushInput cyc = {
            .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .cyclone = true};
        player_skills_step(g, &cyc);
        /* Thunder Drum on_cyclone emits EV_STORM_ZAP before cast_cyclone emits EV_CYCLONE. */
        int zap_i = -1;
        int cyc_i = -1;
        for (int32_t i = 0; i < g->event_count; ++i) {
            if (g->events[i].type == EV_STORM_ZAP && zap_i < 0) {
                zap_i = i;
            }
            if (g->events[i].type == EV_CYCLONE && cyc_i < 0) {
                cyc_i = i;
            }
        }
        TEST_ASSERT_TRUE(zap_i >= 0 && cyc_i >= 0 && zap_i < cyc_i);
        game_destroy(g);

        /* Two states with same acquisition order match hash; swapped acquisition order preserves
         * distinct owned arrays deterministically. */
        GameState *a = game_create(&cfg);
        GameState *b = game_create(&cfg);
        GameState *c = game_create(&cfg);
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(a, UPG_RICH_INK));
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(a, UPG_INFERNO));
        TEST_ASSERT_TRUE(roguelike_grant_relic(a, RELIC_INK_DYED_SHRINE));
        TEST_ASSERT_TRUE(roguelike_grant_relic(a, RELIC_WIND_RETURN));

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(b, UPG_RICH_INK));
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(b, UPG_INFERNO));
        TEST_ASSERT_TRUE(roguelike_grant_relic(b, RELIC_INK_DYED_SHRINE));
        TEST_ASSERT_TRUE(roguelike_grant_relic(b, RELIC_WIND_RETURN));

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(c, UPG_INFERNO));
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(c, UPG_RICH_INK));
        TEST_ASSERT_TRUE(roguelike_grant_relic(c, RELIC_WIND_RETURN));
        TEST_ASSERT_TRUE(roguelike_grant_relic(c, RELIC_INK_DYED_SHRINE));

        TEST_ASSERT_EQUAL_HEX64(game_hash(a), game_hash(b));
        TEST_ASSERT_EQUAL_INT(UPG_INFERNO, c->run.upgrades[0].id);
        TEST_ASSERT_EQUAL_INT(UPG_RICH_INK, c->run.upgrades[1].id);
        TEST_ASSERT_EQUAL_INT(RELIC_WIND_RETURN, c->run.relics[0]);
        TEST_ASSERT_EQUAL_INT(RELIC_INK_DYED_SHRINE, c->run.relics[1]);
        game_destroy(c);
        game_destroy(b);
        game_destroy(a);
    }

    /* 2. Relic 3-choice offer, reroll, pick -> ACT_TRANSITION, 14-owned -> 1 choice, 15-owned ->
     * FALLBACK + repair -> ACT_TRANSITION. */
    {
        RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(777));
        cfg.unlocked_relic_mask = (UINT64_C(1) << META_RELIC_UNLOCK_COUNT) - 1u;
        GameState *g = game_create(&cfg);
        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        g->phase_t = 0.0f;
        rewards_generate_relic_offer(g);

        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(3, g->run.reward.choice_count);
        TEST_ASSERT_FALSE(g->run.reward.repair_available);
        TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[0], g->run.reward.choice_ids[1]);
        TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[0], g->run.reward.choice_ids[2]);
        TEST_ASSERT_NOT_EQUAL_INT32(g->run.reward.choice_ids[1], g->run.reward.choice_ids[2]);

        /* Reroll consumes shared act reroll. */
        const uint32_t s0 = g->run.reward.serial;
        const GameCommand reroll = {.type = CMD_REROLL_REWARD, .arg = 0};
        game_step(g, NULL, &reroll);
        TEST_ASSERT_EQUAL_INT32(0, g->run.rerolls_left);
        TEST_ASSERT_EQUAL_UINT32(s0 + 1u, g->run.reward.serial);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);

        /* Pick relic -> grants it, emits EV_REWARD_PICK with EVF_REWARD_RELIC, advances act. */
        const RelicId picked = (RelicId)g->run.reward.choice_ids[0];
        const GameCommand pick = {.type = CMD_PICK_REWARD, .arg = 0};
        game_step(g, NULL, &pick);
        TEST_ASSERT_TRUE(game_has_relic(g, picked));
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
        TEST_ASSERT_EQUAL_INT32(1, g->run.act_index);
        TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);
        bool saw_relic_ev = false;
        for (int32_t i = 0; i < g->event_count; ++i) {
            if (g->events[i].type == EV_REWARD_PICK && g->events[i].value == (int32_t)picked &&
                (g->events[i].flags & EVF_REWARD_RELIC) != 0) {
                saw_relic_ev = true;
            }
        }
        TEST_ASSERT_TRUE(saw_relic_ev);

        /* Grant remaining relics up to 8 -> offer shows only the 1 unowned relic. */
        RelicId last_unowned = RELIC_COUNT;
        for (int r = 0; r < (int)RELIC_COUNT; ++r) {
            if (!game_has_relic(g, (RelicId)r)) {
                if (g->run.relic_count < (int32_t)RELIC_COUNT - 1) {
                    TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)r));
                } else {
                    last_unowned = (RelicId)r;
                }
            }
        }
        TEST_ASSERT_EQUAL_INT32((int32_t)RELIC_COUNT - 1, g->run.relic_count);
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        rewards_generate_relic_offer(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(1, g->run.reward.choice_count);
        TEST_ASSERT_EQUAL_INT32((int32_t)last_unowned, g->run.reward.choice_ids[0]);

        /* Grant 9th relic -> REWARD_KIND_FALLBACK, repair advances to next act. */
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, last_unowned));
        rewards_generate_relic_offer(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(0, g->run.reward.choice_count);
        TEST_ASSERT_TRUE(g->run.reward.repair_available);

        const int32_t act_before = g->run.act_index;
        const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
        game_step(g, NULL, &repair);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
        TEST_ASSERT_EQUAL_INT32(act_before + 1, g->run.act_index);
        TEST_ASSERT_EQUAL_INT32(0, g->run.wave_index);
        game_destroy(g);
    }
}

static void test_first_time_hints_all_8(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_test_entities(g);

    /* 1. HINT_STROKE on first PLAYING tick of Act 1 Wave 1 */
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_HINT_TRIGGER));
    TEST_ASSERT_EQUAL_INT32((int32_t)HINT_STROKE, g->events[0].value);
    /* Second call must NOT re-trigger HINT_STROKE. */
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_HINT_TRIGGER));

    /* 2. HINT_CYCLONE_FIREBALL */
    (void)game_debug_spawn_projectile(g, PROJ_FIREBALL, 300.0f, 300.0f, 100.0f, 0.0f);
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_HINT_TRIGGER));
    TEST_ASSERT_EQUAL_INT32((int32_t)HINT_CYCLONE_FIREBALL, g->events[0].value);
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_HINT_TRIGGER));

    /* 3. HINT_GALE_UMBRELLA */
    (void)game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 500.0f, 300.0f);
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_HINT_TRIGGER));
    TEST_ASSERT_EQUAL_INT32((int32_t)HINT_GALE_UMBRELLA, g->events[0].value);
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_HINT_TRIGGER));

    /* 4. HINT_INK_REGEN */
    g->player.ink = 15.0f;
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_HINT_TRIGGER));
    TEST_ASSERT_EQUAL_INT32((int32_t)HINT_INK_REGEN, g->events[0].value);
    g->event_count = 0;
    hints_check_tick(g);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_HINT_TRIGGER));

    /* 5-8. Reaction hints: BLAZING, TEMPEST, THUNDERBURST, CATACLYSM */
    const struct {
        GameEventType ev_type;
        HintId hint_id;
    } react_cases[4] = {
        {EV_IGNITE, HINT_REACT_BLAZING},
        {EV_STORM_ZAP, HINT_REACT_TEMPEST},
        {EV_THUNDERBURST, HINT_REACT_THUNDERBURST},
        {EV_CATACLYSM, HINT_REACT_CATACLYSM},
    };
    for (int i = 0; i < 4; ++i) {
        g->event_count = 0;
        (void)game_push_event(
            g, (GameEvent){.type = react_cases[i].ev_type, .x = 400.0f, .y = 300.0f});
        hints_check_tick(g);
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_HINT_TRIGGER));
        TEST_ASSERT_EQUAL_INT32((int32_t)react_cases[i].hint_id, g->events[1].value);

        /* Same event a second time must NOT re-trigger. */
        g->event_count = 0;
        (void)game_push_event(
            g, (GameEvent){.type = react_cases[i].ev_type, .x = 400.0f, .y = 300.0f});
        hints_check_tick(g);
        TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_HINT_TRIGGER));
    }

    TEST_ASSERT_EQUAL_HEX32((UINT32_C(1) << HINT_COUNT) - 1u, g->internal.triggered_hints_mask);
    game_destroy(g);
}

static int count_entities_with_mask(const GameState *g, uint64_t mask)
{
    int n = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, mask)) {
            ++n;
        }
    }
    return n;
}

static void test_p2_upgrades_new_4(void)
{
    /* 1. Lingering Embers Lv1/2/3: fluid.cfg.dye_damp[1] == 0.945 / 0.955 / 0.965 */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.935f, g->fluid.cfg.dye_damp[1]);
        const float expected[3] = {0.945f, 0.955f, 0.965f};
        for (int lv = 0; lv < 3; ++lv) {
            TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_LINGERING_EMBERS));
            TEST_ASSERT_FLOAT_WITHIN(1e-5f, expected[lv], g->fluid.cfg.dye_damp[1]);
        }
        game_destroy(g);
    }

    /* 2. Storm Brush Lv1 (chain x2), Lv2 (x1.5 storm), Lv3 (surge non-dry stroke storm + twin) */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_STORM_BRUSH));
        TEST_ASSERT_EQUAL_INT32(2, g->internal.mods.chain_storm_interval);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, g->internal.mods.chain_storm_mul);

        const EntityId e1 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 300.0f, 300.0f);
        const EntityId e2 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 420.0f, 300.0f);
        fluid_reset(&g->fluid);
        (void)combat_apply_damage(g, e1, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sample_channel(g, 2, 300.0f, 300.0f));
        (void)combat_apply_damage(g, e2, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        const float storm_lv1 = sample_channel(g, 2, 444.0f, 300.0f);
        TEST_ASSERT_TRUE(storm_lv1 > 0.05f);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_STORM_BRUSH));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, g->internal.mods.chain_storm_mul);
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        g->run.chain = 0;
        g->run.chain_t = 0.0f;
        const EntityId e3 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 300.0f, 300.0f);
        const EntityId e4 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 420.0f, 300.0f);
        fluid_reset(&g->fluid);
        (void)combat_apply_damage(g, e3, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        (void)combat_apply_damage(g, e4, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        const float storm_lv2 = sample_channel(g, 2, 444.0f, 300.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, storm_lv1 * 1.5f, storm_lv2);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_STORM_BRUSH));
        TEST_ASSERT_TRUE(g->internal.mods.surge_stroke_storm);
        clear_test_entities(g);

        const BrushInput st = {
            .pmx = 260.0f, .pmy = 260.0f, .mx = 320.0f, .my = 260.0f, .paint = true};

        /* Non-surge stroke: no storm injected. */
        fluid_reset(&g->fluid);
        g->player.ink = 100.0f;
        g->player.surge_t = 0.0f;
        player_skills_step(g, &st);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sample_channel(g, 2, 320.0f, 260.0f));

        /* Dry surge stroke: no storm injected. */
        fluid_reset(&g->fluid);
        g->player.ink = 10.0f;
        g->player.surge_t = 3.0f;
        player_skills_step(g, &st);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sample_channel(g, 2, 320.0f, 260.0f));

        /* Surging non-dry stroke with Twin Brush: storm injected at both cursor and mirror. */
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_TWIN_BRUSH));
        fluid_reset(&g->fluid);
        g->player.ink = 100.0f;
        g->player.surge_t = 3.0f;
        player_skills_step(g, &st);
        const float sx = g->shrines[0].x;
        const float sy = g->shrines[0].y;
        TEST_ASSERT_TRUE(sample_channel(g, 2, 320.0f, 260.0f) > 0.10f);
        TEST_ASSERT_TRUE(sample_channel(g, 2, 2.0f * sx - 320.0f, 2.0f * sy - 260.0f) > 0.10f);
        game_destroy(g);
    }

    /* 3. Double Gale Lv1 (0.85s), Lv2 (0.70s), Lv3 (2 charges, back-to-back cast, 0.70s/charge) */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DOUBLE_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.85f, g->internal.mods.gale_cd);
        TEST_ASSERT_EQUAL_INT32(1, g->player.gale_max_charges);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DOUBLE_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.70f, g->internal.mods.gale_cd);
        TEST_ASSERT_EQUAL_INT32(1, g->player.gale_max_charges);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DOUBLE_GALE));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.70f, g->internal.mods.gale_cd);
        TEST_ASSERT_EQUAL_INT32(2, g->internal.mods.gale_max_charges);
        TEST_ASSERT_EQUAL_INT32(2, g->player.gale_max_charges);
        TEST_ASSERT_EQUAL_INT32(2, g->player.gale_charges);

        g->player.ink = 100.0f;
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        const BrushInput gin = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 300.0f, .my = 300.0f, .gale = true};

        /* Cast 1: 2 -> 1 charge, CD starts at 0.70s. */
        g->event_count = 0;
        player_skills_step(g, &gin);
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_GALE));
        TEST_ASSERT_EQUAL_INT32(1, g->player.gale_charges);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.70f, g->player.gale_cd);

        /* Cast 2 immediately on next tick: 1 -> 0 charges, timer keeps ticking from Cast 1. */
        g->event_count = 0;
        player_skills_step(g, &gin);
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_GALE));
        TEST_ASSERT_EQUAL_INT32(0, g->player.gale_charges);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.70f - INK_FIXED_DT, g->player.gale_cd);

        /* Cast 3 immediately: denied (0 charges). */
        g->event_count = 0;
        player_skills_step(g, &gin);
        TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_GALE));
        TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_SKILL_DENIED));
        TEST_ASSERT_EQUAL_INT32(0, g->player.gale_charges);

        /* 39 more ticks (total 41 decays since Cast 1): still 0 charges. */
        for (int t = 0; t < 39; ++t) {
            player_skills_step(g, NULL);
        }
        TEST_ASSERT_EQUAL_INT32(0, g->player.gale_charges);

        /* 42nd decay (== 0.70s): restores 1st charge and starts 0.70s timer for 2nd charge. */
        player_skills_step(g, NULL);
        TEST_ASSERT_EQUAL_INT32(1, g->player.gale_charges);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.70f, g->player.gale_cd);

        /* 41 more ticks: still 1 charge; 42nd tick: restores 2nd charge and clears CD to 0. */
        for (int t = 0; t < 41; ++t) {
            player_skills_step(g, NULL);
        }
        TEST_ASSERT_EQUAL_INT32(1, g->player.gale_charges);
        player_skills_step(g, NULL);
        TEST_ASSERT_EQUAL_INT32(2, g->player.gale_charges);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->player.gale_cd);
        game_destroy(g);
    }

    /* 4. Inkstone Blessing Lv1/2/3: +20%/+40%/+60% regen, Lv3 immediate 26*1.6/s at idle_t == 0 */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INKSTONE_BLESSING));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.20f, g->internal.mods.ink_regen_mul);
        TEST_ASSERT_FALSE(g->internal.mods.ink_regen_no_delay);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INKSTONE_BLESSING));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.40f, g->internal.mods.ink_regen_mul);
        TEST_ASSERT_FALSE(g->internal.mods.ink_regen_no_delay);
        g->player.ink = 50.0f;
        g->player.idle_t = 0.0f;
        player_skills_step(g, NULL);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50.0f + 15.0f * 1.40f * INK_FIXED_DT, g->player.ink);

        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_INKSTONE_BLESSING));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.60f, g->internal.mods.ink_regen_mul);
        TEST_ASSERT_TRUE(g->internal.mods.ink_regen_no_delay);
        g->player.ink = 50.0f;
        g->player.idle_t = 0.0f;
        player_skills_step(g, NULL);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50.0f + 26.0f * 1.60f * INK_FIXED_DT, g->player.ink);
        game_destroy(g);
    }
}

static void test_p2_relics_unlockable_and_mask(void)
{
    /* 1. Oni Tile: Stone Oni gale at MASS_MEDIUM (0.6x), explosion (0.75x), Maelstrom Bind Lv2,
     * Kamaitachi instant slam kill, Stone Oni slammed for 80 + stun (stopped Nue remains 60). */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        game_debug_start_wave(g, 0);
        clear_test_entities(g);
        fluid_reset(&g->fluid);

        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_ONI_TILE));
        TEST_ASSERT_EQUAL_INT(MASS_MEDIUM, game_effective_mass_tier(g, MASS_HEAVY));

        const EntityId oni = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 400.0f, 300.0f);
        game_velocity_mut(g, oni)->vx = 0.0f;
        game_velocity_mut(g, oni)->vy = 0.0f;
        g->player.aim_x = 1.0f;
        g->player.aim_y = 0.0f;
        const BrushInput gin = {
            .pmx = 300.0f, .pmy = 300.0f, .mx = 300.0f, .my = 300.0f, .gale = true};
        player_skills_step(g, &gin);
        /* Heavy brush gale_impulse_mul = 1.25 -> 560 * 0.6 * 1.25 = 420 px/s. */
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 420.0f, game_velocity(g, oni)->vx);

        /* Maelstrom Bind Lv2 binds Stone Oni under Oni Tile. */
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
        game_transform_mut(g, oni)->x = 460.0f;
        game_transform_mut(g, oni)->y = 300.0f;
        g->internal.cyclones[0] = (ActiveCyclone){.active = true,
                                                  .x = 400.0f,
                                                  .y = 300.0f,
                                                  .rad_cells = 13.0f,
                                                  .life = 0.5f,
                                                  .max_life = 0.72f};
        upgrades_fire_on_tick(g, INK_FIXED_DT);
        TEST_ASSERT_TRUE(g->internal.maelstrom_bind_t[oni.index] > 0.5f);

        /* Thunderburst knocks back Stone Oni with MASS_MEDIUM multiplier (520 * 0.75 = 390). */
        clear_test_entities(g);
        fluid_reset(&g->fluid);
        const float tb_cx = 43.0f * FLUID_CELL_PX;
        const float tb_cy = 40.5f * FLUID_CELL_PX;
        const EntityId oni_tb = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, tb_cx + 40.0f, tb_cy);
        game_velocity_mut(g, oni_tb)->vx = 0.0f;
        game_velocity_mut(g, oni_tb)->vy = 0.0f;
        fluid_reset(&g->fluid);
        float *fire = fluid_channel_mut(&g->fluid, 1);
        float *storm = fluid_channel_mut(&g->fluid, 2);
        for (int k = 0; k < 6; ++k) {
            fire[40 * FLUID_W + 40 + k] = 0.45f;
            storm[40 * FLUID_W + 40 + k] = 0.45f;
        }
        elements_step(g);
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 390.0f, game_velocity(g, oni_tb)->vx);

        /* Wall slam instant kills Kamaitachi; Stone Oni keeps its raw Heavy slam (80 + stun);
         * stopped Nue takes 60 dmg. */
        clear_test_entities(g);
        const EntityId kama = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 48.0f, 200.0f);
        const EntityId oni_slam = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 48.0f, 340.0f);
        const EntityId nue = game_debug_spawn_enemy(g, ENEMY_BOSS_NUE, 48.0f, 480.0f);
        game_enemy_mut(g, nue)->ai_state = 1;
        game_velocity_mut(g, kama)->vx = -300.0f;
        game_status_mut(g, kama)->knockback_t = 1.0f;
        game_velocity_mut(g, oni_slam)->vx = -300.0f;
        game_status_mut(g, oni_slam)->knockback_t = 1.0f;
        game_velocity_mut(g, nue)->vx = -300.0f;
        game_status_mut(g, nue)->knockback_t = 1.0f;

        combat_physics_and_slams_step(g);
        ecs_flush_destroy(&g->ecs);
        TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, kama));
        TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, oni_slam));
        TEST_ASSERT_EQUAL_INT32(10, game_health(g, oni_slam)->hp);
        TEST_ASSERT_TRUE(game_status(g, oni_slam)->stun_t >= 1.0f - 1e-5f);
        TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, nue));
        TEST_ASSERT_EQUAL_INT32(640, game_health(g, nue)->hp);
        TEST_ASSERT_TRUE(game_status(g, nue)->stun_t >= 1.0f - 1e-5f);
        game_destroy(g);
    }

    /* 2. Endless Brush: regen x2.5, ink_max == 60 (clamped), lower bound 30.0f clamp. */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 100.0f, g->player.ink_max);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 100.0f, g->player.ink);

        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_ENDLESS_BRUSH));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.5f, g->internal.mods.ink_regen_mul);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 60.0f, g->player.ink_max);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 60.0f, g->player.ink);
        TEST_ASSERT_FALSE(g->player.dry);

        /* Verify 30.0f lower clamp when ink_max_add drops below -70. */
        g->run.relics[g->run.relic_count++] = RELIC_ENDLESS_BRUSH;
        roguelike_recompute_modifiers(g);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 30.0f, g->player.ink_max);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 30.0f, g->player.ink);
        game_destroy(g);
    }

    /* 3. Night Parade Scroll: +1 kill orb, spawn_interval x0.75, max_alive +2, x1.5 crystals. */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_NIGHT_PARADE_SCROLL));
        TEST_ASSERT_TRUE(g->internal.mods.night_parade_scroll);
        TEST_ASSERT_EQUAL_INT32(1, g->internal.mods.bonus_kill_orbs);
        TEST_ASSERT_EQUAL_INT32(2, g->internal.mods.max_alive_add);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.75f, g->internal.mods.spawn_interval_mul);

        waves_start_wave(g, 0);
        /* Act 1 Wave 1 base spawn_interval = 0.95f -> 0.95f * 0.75f = 0.7125f. */
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.95f * 0.75f, g->internal.spawn_cd);

        /* Kill 1 Archer (base 1 orb + 1 bonus = 2 orbs). */
        clear_test_entities(g);
        const EntityId a = game_debug_spawn_enemy(g, ENEMY_ARCHER, 300.0f, 300.0f);
        (void)combat_apply_damage(g, a, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        ecs_flush_destroy(&g->ecs);
        TEST_ASSERT_EQUAL_INT(2, count_entities_with_mask(g, COMP_INK_ORB));

        /* Act 1 Wave 1 base max_alive = 6 -> with +2 cap is 8. */
        clear_test_entities(g);
        for (int k = 0; k < 7; ++k) {
            (void)game_debug_spawn_enemy(g, ENEMY_ARCHER, 100.0f + (float)k * 105.0f, 140.0f);
        }
        g->internal.spawn_cd = 0.0f;
        waves_step_spawns(g);
        TEST_ASSERT_EQUAL_INT(8, count_entities_with_mask(g, COMP_ENEMY));
        g->internal.spawn_cd = 0.0f;
        waves_step_spawns(g);
        TEST_ASSERT_EQUAL_INT(8, count_entities_with_mask(g, COMP_ENEMY));

        /* Run result crystal_bonus_x15 == true -> floor(base * 1.5). */
        g->run.acts_cleared = 1;
        g->run.kills = 5;
        g->run.max_chain = 0;
        g->run.thunderbursts = 0;
        g->run.cataclysms = 0;
        g->run.endless_acts_cleared = 0;
        g->run.bonus_crystals = 0;
        const MetaRunResult res = game_run_result(g);
        TEST_ASSERT_TRUE(res.crystal_bonus_x15);
        const MetaSettleBreakdown bd = meta_calc_crystals(&res);
        /* base = 30 + 1 = 31, bonus = floor(31 / 2) = 15, total = 46. */
        TEST_ASSERT_EQUAL_INT32(15, bd.from_bonus);
        TEST_ASSERT_EQUAL_INT32(46, bd.total_crystals);
        game_destroy(g);
    }

    /* 4. Unlock mask control: mask == 0 -> owning 9 base relics hits FALLBACK;
     * unlocking slot 0 (0x1) offers only RELIC_SEAL_OF_TRINITY. */
    {
        RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(5150));
        cfg.unlocked_relic_mask = 0u;
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        for (int r = 0; r < (int)RELIC_SEAL_OF_TRINITY; ++r) {
            TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)r));
        }
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        rewards_generate_relic_offer(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(0, g->run.reward.choice_count);
        TEST_ASSERT_EQUAL_INT32(3, g->run.reward.repair_amount);
        TEST_ASSERT_EQUAL_INT32(30, g->run.reward.crystal_reward);

        g->config.unlocked_relic_mask = UINT64_C(1) << 0;
        rewards_generate_relic_offer(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(1, g->run.reward.choice_count);
        TEST_ASSERT_EQUAL_INT32((int32_t)RELIC_SEAL_OF_TRINITY, g->run.reward.choice_ids[0]);
        game_destroy(g);
    }
}

static void test_p2_fallback_offers_and_overflow_conversion(void)
{
    /* 1. All 28 upgrades Lv3 -> FALLBACK (repair +1 / crystal +10) + CMD_PICK_ENDLESS_CRYSTAL. */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        for (int id = 0; id < (int)UPG_COUNT; ++id) {
            for (int lv = 0; lv < 3; ++lv) {
                TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, (UpgradeId)id));
            }
        }
        enter_reward_phase(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(0, g->run.reward.choice_count);
        TEST_ASSERT_TRUE(g->run.reward.repair_available);
        TEST_ASSERT_EQUAL_INT32(1, g->run.reward.repair_amount);
        TEST_ASSERT_EQUAL_INT32(10, g->run.reward.crystal_reward);

        const GameCommand pick_crys = {.type = CMD_PICK_ENDLESS_CRYSTAL, .arg = 0};
        game_step(g, NULL, &pick_crys);
        TEST_ASSERT_EQUAL_INT32(10, g->run.bonus_crystals);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
        TEST_ASSERT_EQUAL_INT32(1, g->run.wave_index);
        bool saw_crys_ev = false;
        for (int32_t i = 0; i < g->event_count; ++i) {
            if (g->events[i].type == EV_REWARD_PICK && g->events[i].value == 10 &&
                (g->events[i].flags & EVF_REWARD_CRYSTAL) != 0) {
                saw_crys_ev = true;
            }
        }
        TEST_ASSERT_TRUE(saw_crys_ev);
        game_destroy(g);
    }

    /* 2. Boss fallback: repair +3 with max_hp - 1 heals +1 and converts overflow 2 HP -> +30. */
    {
        RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(6001));
        cfg.unlocked_relic_mask = 0u;
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        for (int r = 0; r < (int)RELIC_SEAL_OF_TRINITY; ++r) {
            TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)r));
        }
        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        g->phase_t = 0.0f;
        g->shrines[0].hp = g->shrines[0].max_hp - 1;
        rewards_generate_relic_offer(g);

        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_TRUE(g->run.reward.repair_available);
        TEST_ASSERT_EQUAL_INT32(3, g->run.reward.repair_amount);
        TEST_ASSERT_EQUAL_INT32(30, g->run.reward.crystal_reward);

        const int32_t act_before = g->run.act_index;
        const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
        game_step(g, NULL, &repair);
        TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp, g->shrines[0].hp);
        TEST_ASSERT_EQUAL_INT32(30, g->run.bonus_crystals);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
        TEST_ASSERT_EQUAL_INT32(act_before + 1, g->run.act_index);
        game_destroy(g);
    }

    /* 3. Boss fallback: CMD_PICK_ENDLESS_CRYSTAL adds +30, advances act, reflects in settlement. */
    {
        RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(6002));
        cfg.unlocked_relic_mask = 0u;
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        for (int r = 0; r < (int)RELIC_SEAL_OF_TRINITY; ++r) {
            TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)r));
        }
        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        g->phase_t = 0.0f;
        rewards_generate_relic_offer(g);

        const int32_t act_before = g->run.act_index;
        const GameCommand pick_crys = {.type = CMD_PICK_ENDLESS_CRYSTAL, .arg = 0};
        game_step(g, NULL, &pick_crys);
        TEST_ASSERT_EQUAL_INT32(30, g->run.bonus_crystals);
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, g->phase);
        TEST_ASSERT_EQUAL_INT32(act_before + 1, g->run.act_index);

        MetaState meta = defaults;
        MetaSettleBreakdown bd = {0};
        TEST_ASSERT_TRUE(meta_settle_run(&meta, g, &bd));
        TEST_ASSERT_EQUAL_INT32(30, bd.from_endless);
        TEST_ASSERT_TRUE(bd.total_crystals >= 30);
        TEST_ASSERT_EQUAL_INT32(bd.total_crystals, meta.ink_crystals);
        game_destroy(g);
    }

    /* 4. Non-fallback CMD_PICK_ENDLESS_CRYSTAL is a pure no-op (UPGRADE, RELIC, PLAYING). */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        const GameCommand pick_crys = {.type = CMD_PICK_ENDLESS_CRYSTAL, .arg = 0};

        enter_reward_phase(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_UPGRADE, g->run.reward.kind);
        game_step(g, NULL, NULL);
        const uint64_t h_upg = game_hash(g);
        game_step(g, NULL, &pick_crys);
        TEST_ASSERT_EQUAL_HEX64(h_upg, game_hash(g));
        TEST_ASSERT_EQUAL_INT32(0, g->run.bonus_crystals);

        g->internal.wave_clear_is_boss = true;
        rewards_generate_relic_offer(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_RELIC, g->run.reward.kind);
        game_step(g, NULL, NULL);
        const uint64_t h_rel = game_hash(g);
        game_step(g, NULL, &pick_crys);
        TEST_ASSERT_EQUAL_HEX64(h_rel, game_hash(g));
        TEST_ASSERT_EQUAL_INT32(0, g->run.bonus_crystals);

        GameState *p1 = game_create(NULL);
        GameState *p2 = game_create(NULL);
        game_debug_start_wave(p1, 0);
        game_debug_start_wave(p2, 0);
        game_step(p1, NULL, NULL);
        game_step(p2, NULL, &pick_crys);
        TEST_ASSERT_EQUAL_HEX64(game_hash(p1), game_hash(p2));
        game_destroy(p2);
        game_destroy(p1);
        game_destroy(g);
    }
}

static float ink_spent_by_step(GameState *g, const BrushInput *in, float surge_t)
{
    g->player.ink = g->player.ink_max;
    g->player.surge_t = surge_t;
    g->player.cyclone_cd = 0.0f;
    g->player.gale_charges = 1;
    g->player.idle_t = 1.0f;
    player_skills_step(g, in);
    return g->player.ink_max - g->player.ink;
}

static void test_ink_economy_orb_chain_refund_and_surge_cost(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_test_entities(g);
    fluid_reset(&g->fluid);

    /* Collecting one Ink Orb restores 10 ink. */
    const EntityId orb = game_debug_spawn_orb(g, 400.0f, 300.0f);
    TEST_ASSERT_EQUAL_INT32(10, game_orb(g, orb)->value);
    TransformComp *otr = game_transform_mut(g, orb);
    otr->x = g->player.cursor_x;
    otr->y = g->player.cursor_y;
    g->player.ink = 50.0f;
    combat_orbs_step(g);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 60.0f, g->player.ink);

    /* Chain kills refund 3 ink per chain step from x2, capped at x3 (+6, +9, +9, ...). */
    static const float k_expected_refund[] = {0.0f, 6.0f, 9.0f, 9.0f, 9.0f};
    g->run.chain = 0;
    g->run.chain_t = 0.0f;
    for (int k = 0; k < (int)(sizeof k_expected_refund / sizeof k_expected_refund[0]); ++k) {
        const EntityId e =
            game_debug_spawn_enemy(g, ENEMY_ARCHER, 260.0f + 90.0f * (float)k, 300.0f);
        g->player.ink = 50.0f;
        (void)combat_apply_damage(g, e, 50, DMG_PHYSICAL, 0.0f, 0.0f);
        TEST_ASSERT_EQUAL_INT32(k + 1, g->run.chain);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50.0f + k_expected_refund[k], g->player.ink);
    }
    clear_test_entities(g);

    /* Ink Surge multiplies every skill's ink cost by 0.72: stroke, Cyclone, and Gale alike. */
    const BrushInput stroke = {
        .pmx = 300.0f, .pmy = 300.0f, .mx = 340.0f, .my = 300.0f, .paint = true};
    const BrushInput cyclone = {
        .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .cyclone = true};
    const BrushInput gale = {
        .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .gale = true};

    const float stroke_plain = ink_spent_by_step(g, &stroke, 0.0f);
    const float stroke_surge = ink_spent_by_step(g, &stroke, 3.0f);
    TEST_ASSERT_TRUE(stroke_plain > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, stroke_plain * 0.72f, stroke_surge);

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 26.0f, ink_spent_by_step(g, &cyclone, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 26.0f * 0.72f, ink_spent_by_step(g, &cyclone, 3.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 16.0f, ink_spent_by_step(g, &gale, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 16.0f * 0.72f, ink_spent_by_step(g, &gale, 3.0f));

    g->player.surge_t = 3.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 26.0f * 0.72f, player_skills_cyclone_cost(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 16.0f * 0.72f, player_skills_gale_cost(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 26.0f * 0.72f, player_skills_next_tick_cyclone_cost(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 16.0f * 0.72f, player_skills_next_tick_gale_cost(g));
    g->player.surge_t = 0.01f;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 26.0f, player_skills_next_tick_cyclone_cost(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 16.0f, player_skills_next_tick_gale_cost(g));
    g->player.surge_t = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 26.0f, player_skills_cyclone_cost(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 16.0f, player_skills_gale_cost(g));

    game_destroy(g);
}

static void test_autoplay_skill_budget_and_surge_expiry(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_test_entities(g);
    fluid_reset(&g->fluid);

    const float cx = g->shrines[0].x;
    const float cy = g->shrines[0].y;
    g->player.cursor_x = cx + 120.0f;
    g->player.cursor_y = cy;
    g->player.idle_t = 0.0f;
    g->player.cyclone_cd = 0.0f;
    g->player.gale_charges = 1;

    /* Set up both an urgent projectile (triggers Cyclone) and a nearby slam target (wants Gale). */
    const EntityId umbrella = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cx + 120.0f, cy);
    const EntityId arrow =
        game_debug_spawn_projectile(g, PROJ_ARROW, cx + 100.0f, cy, -120.0f, 0.0f);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, umbrella.index);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, arrow.index);

    /* During Surge (Cyclone 18.72 + Gale 11.52 = 30.24), ink == 29 must not request both. */
    g->player.surge_t = 3.0f;
    g->player.ink = 29.0f;
    const BrushInput in_29 = game_autoplay_input(g);
    TEST_ASSERT_TRUE(in_29.cyclone);
    TEST_ASSERT_FALSE(in_29.gale);
    g->event_count = 0;
    player_skills_step(g, &in_29);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_CYCLONE));
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_SKILL_DENIED));

    /* When Surge expires during the tick (0.01s <= INK_FIXED_DT), Cyclone costs 26 again, so
     * ink == 24 must NOT request Cyclone (while active Surge with ink == 24 does). */
    g->player.cursor_x = cx + 120.0f;
    g->player.cursor_y = cy;
    g->player.idle_t = 0.0f;
    g->player.cyclone_cd = 0.0f;
    g->player.gale_charges = 0;
    g->player.ink = 24.0f;
    g->player.surge_t = 3.0f;
    TEST_ASSERT_TRUE(game_autoplay_input(g).cyclone);

    g->player.surge_t = 0.01f;
    const BrushInput in_expiring = game_autoplay_input(g);
    TEST_ASSERT_FALSE(in_expiring.cyclone);
    g->event_count = 0;
    player_skills_step(g, &in_expiring);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_CYCLONE));
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_SKILL_DENIED));

    /* Expiring Surge also prices Gale at full base cost (16), preventing a denied slam Gale. */
    g->player.cursor_x = cx + 120.0f;
    g->player.cursor_y = cy;
    g->player.idle_t = 0.0f;
    g->player.cyclone_cd = 1.0f;
    g->player.gale_charges = 1;
    g->player.ink = 15.0f;
    g->player.surge_t = 3.0f;
    TEST_ASSERT_TRUE(game_autoplay_input(g).gale);
    g->player.surge_t = 0.01f;
    const BrushInput in_gale_expiring = game_autoplay_input(g);
    TEST_ASSERT_FALSE(in_gale_expiring.gale);

    /* With enough ink during Surge (36 >= 18.72 + 11.52 + margins), both succeed cleanly. */
    g->player.cursor_x = cx + 120.0f;
    g->player.cursor_y = cy;
    g->player.idle_t = 0.0f;
    g->player.cyclone_cd = 0.0f;
    g->player.gale_charges = 1;
    g->player.ink = 36.0f;
    g->player.surge_t = 3.0f;
    const BrushInput in_36 = game_autoplay_input(g);
    TEST_ASSERT_TRUE(in_36.cyclone);
    TEST_ASSERT_TRUE(in_36.gale);
    g->event_count = 0;
    player_skills_step(g, &in_36);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_CYCLONE));
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_GALE));
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_SKILL_DENIED));

    game_destroy(g);
}

static void test_wind_return_ignores_departing_projectile(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_test_entities(g);
    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_WIND_RETURN));

    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    const EntityId departing =
        game_debug_spawn_projectile(g, PROJ_ARROW, sx + 40.0f, sy, 150.0f, 0.0f);
    g->event_count = 0;
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    TEST_ASSERT_EQUAL_INT(0, count_ev(g, EV_GALE));
    TEST_ASSERT_FALSE(game_proj(g, departing)->deflected);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.wind_return_cd);

    game_velocity_mut(g, departing)->vx = -150.0f;
    g->event_count = 0;
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    TEST_ASSERT_EQUAL_INT(1, count_ev(g, EV_GALE));
    TEST_ASSERT_TRUE(game_proj(g, departing)->deflected);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f, g->internal.wind_return_cd);
    game_destroy(g);
}

static void test_fallback_repair_overflow_contract_regular_vs_relic(void)
{
    /* 1. Regular wave fallback (all upgrades Lv3): repair +1 at full HP awards 0 overflow crystals
     * so it never outpays the dedicated +10 crystal fallback card. */
    {
        GameState *g = game_create(NULL);
        TEST_ASSERT_NOT_NULL(g);
        for (int id = 0; id < (int)UPG_COUNT; ++id) {
            for (int lv = 0; lv < 3; ++lv) {
                TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, (UpgradeId)id));
            }
        }

        g->shrines[0].hp = g->shrines[0].max_hp;
        enter_reward_phase(g);
        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(1, g->run.reward.repair_amount);
        TEST_ASSERT_EQUAL_INT32(10, g->run.reward.crystal_reward);
        TEST_ASSERT_FALSE(rewards_repair_converts_excess(g));

        const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
        game_step(g, NULL, &repair);
        TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp, g->shrines[0].hp);
        TEST_ASSERT_EQUAL_INT32(0, g->run.bonus_crystals);
        game_destroy(g);
    }

    /* 2. Relic-exhausted boss fallback: repair +3 at full HP converts all 3 excess HP -> +45 Cr. */
    {
        RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(6003));
        cfg.unlocked_relic_mask = 0u;
        GameState *g = game_create(&cfg);
        TEST_ASSERT_NOT_NULL(g);
        for (int r = 0; r < (int)RELIC_SEAL_OF_TRINITY; ++r) {
            TEST_ASSERT_TRUE(roguelike_grant_relic(g, (RelicId)r));
        }

        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
        g->internal.wave_clear_is_boss = true;
        g->phase = RUN_PHASE_REWARD;
        g->phase_t = 0.0f;
        g->shrines[0].hp = g->shrines[0].max_hp;
        rewards_generate_relic_offer(g);

        TEST_ASSERT_EQUAL_INT(REWARD_KIND_FALLBACK, g->run.reward.kind);
        TEST_ASSERT_EQUAL_INT32(3, g->run.reward.repair_amount);
        TEST_ASSERT_EQUAL_INT32(30, g->run.reward.crystal_reward);
        TEST_ASSERT_TRUE(rewards_repair_converts_excess(g));

        const GameCommand repair = {.type = CMD_REPAIR_SHRINE, .arg = 0};
        game_step(g, NULL, &repair);
        TEST_ASSERT_EQUAL_INT32(g->shrines[0].max_hp, g->shrines[0].hp);
        TEST_ASSERT_EQUAL_INT32(45, g->run.bonus_crystals);
        game_destroy(g);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_meta_and_settings);
    RUN_TEST(test_run_config_copies_progress_and_preset_stays_independent);
    RUN_TEST(test_shop_prices_and_all_purchases);
    RUN_TEST(test_invalid_shop_items);
    RUN_TEST(test_crystal_breakdowns);
    RUN_TEST(test_settlement_saturates_after_adding_reward);
    RUN_TEST(test_one_settlement_per_run);
    RUN_TEST(test_null_settlement_inputs);
    RUN_TEST(test_hint_bit_bounds);
    RUN_TEST(test_serialized_save_matches_architecture_example);
    RUN_TEST(test_all_persisted_fields_round_trip);
    RUN_TEST(test_save_syntax_and_missing_defaults);
    RUN_TEST(test_save_clamps_and_normalizes_each_field);
    RUN_TEST(test_shake_scale_thresholds);
    RUN_TEST(test_invalid_saves_reset_every_field);
    RUN_TEST(test_null_meta_inputs);
    RUN_TEST(test_run_reward_offer_rules);
    RUN_TEST(test_run_reward_reproducibility);
    RUN_TEST(test_run_reward_reroll);
    RUN_TEST(test_run_reward_repair_shrine);
    RUN_TEST(test_run_reward_invalid_commands_noop);
    RUN_TEST(test_run_reward_candidate_shortage);
    RUN_TEST(test_run_reward_empty_filter_fallback);
    RUN_TEST(test_run_modifiers_calculation);
    RUN_TEST(test_brush_styles_and_stacking);
    RUN_TEST(test_p1_upgrades_all_16);
    RUN_TEST(test_p1_relics_all_9);
    RUN_TEST(test_hook_acquisition_order_and_relic_rewards);
    RUN_TEST(test_first_time_hints_all_8);
    RUN_TEST(test_p2_upgrades_new_4);
    RUN_TEST(test_p2_relics_unlockable_and_mask);
    RUN_TEST(test_p2_fallback_offers_and_overflow_conversion);
    RUN_TEST(test_ink_economy_orb_chain_refund_and_surge_cost);
    RUN_TEST(test_autoplay_skill_budget_and_surge_expiry);
    RUN_TEST(test_wind_return_ignores_departing_projectile);
    RUN_TEST(test_fallback_repair_overflow_contract_regular_vs_relic);
    return UNITY_END();
}
