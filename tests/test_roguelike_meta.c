#include "meta.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static const MetaState defaults = {.version = 1, .settings = {80, 70, 85, 100, true, true}};

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
    .settings = {11, 22, 33, 50, false, false},
};

static void assert_settings(SettingsState expected, SettingsState actual)
{
    TEST_ASSERT_EQUAL_UINT8(expected.master_vol, actual.master_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.bgm_vol, actual.bgm_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.se_vol, actual.se_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.shake_scale, actual.shake_scale);
    TEST_ASSERT_EQUAL(expected.post_shader_on, actual.post_shader_on);
    TEST_ASSERT_EQUAL(expected.hints_on, actual.hints_on);
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
                               .settings = {80, 70, 85, 100, true, true}};
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
                          "opt_hints=1\n";
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
    const MetaState expected = {.version = 1,
                                .ink_crystals = META_MAX_CRYSTALS,
                                .upgrade_lv = {3, 0, 2, 3},
                                .unlocked_relic_mask = 0x3b,
                                .best_score = META_MAX_SCORE,
                                .best_act = META_MAX_BEST_ACT,
                                .runs_played = META_MAX_COUNTERS,
                                .seen_hints_mask = UINT32_C(0x9abcdef0),
                                .settings = {100, 0, 100, 50, false, true}};
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
    return UNITY_END();
}
