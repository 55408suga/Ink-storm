#include "text_layout.h"
#include "text_table.h"
#include "unity.h"

#include <stddef.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_text_table_entries_valid(void)
{
    TEST_ASSERT_EQUAL_STRING("", text_get(TEXT_NONE, true));
    TEST_ASSERT_EQUAL_STRING("", text_get(TEXT_NONE, false));
    TEST_ASSERT_EQUAL_STRING("", text_get(TEXT_COUNT, true));

    for (int id = (int)TEXT_NONE + 1; id < (int)TEXT_COUNT; ++id) {
        const char *ja = text_get((TextId)id, true);
        const char *en = text_get((TextId)id, false);
        TEST_ASSERT_NOT_NULL(ja);
        TEST_ASSERT_NOT_NULL(en);
        TEST_ASSERT_TRUE(ja[0] != '\0');
        TEST_ASSERT_TRUE(en[0] != '\0');
        for (size_t i = 0; en[i] != '\0'; ++i) {
            const unsigned char ch = (unsigned char)en[i];
            TEST_ASSERT_TRUE(ch >= 0x20u && ch <= 0x7eu);
        }
    }
}

static void test_text_table_content_mappings(void)
{
    /* All 12 enemies & 3 boss epithets */
    TEST_ASSERT_EQUAL_INT(12, ENEMY_DEF_COUNT);
    for (int id = 0; id < (int)ENEMY_DEF_COUNT; ++id) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_name((EnemyDefId)id));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_intro((EnemyDefId)id));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_ENEMY_OFUDA_FOX_NAME, text_enemy_name(ENEMY_OFUDA_FOX));
    TEST_ASSERT_EQUAL_INT(TEXT_ENEMY_OFUDA_FOX_INTRO, text_enemy_intro(ENEMY_OFUDA_FOX));
    TEST_ASSERT_EQUAL_INT(TEXT_ENEMY_BOSS_RYU_ORB_NAME, text_enemy_name(ENEMY_BOSS_RYU_ORB));
    TEST_ASSERT_EQUAL_INT(TEXT_ENEMY_BOSS_RYU_ORB_INTRO, text_enemy_intro(ENEMY_BOSS_RYU_ORB));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_name(ENEMY_DEF_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_intro(ENEMY_DEF_COUNT));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_TENGU));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_NUE));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_RYU));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_ARCHER));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_DEF_COUNT));

    /* All 28 upgrades x Lv1..3 (verify distinct IDs for P2 upgrades 24..27) */
    TEST_ASSERT_EQUAL_INT(28, UPG_COUNT);
    for (int id = 0; id < (int)UPG_COUNT; ++id) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_upgrade_name((UpgradeId)id));
        for (int lv = 1; lv <= 3; ++lv) {
            TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_upgrade_effect((UpgradeId)id, lv));
        }
        TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect((UpgradeId)id, 0));
        TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect((UpgradeId)id, 4));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_UPG_LINGERING_EMBERS_NAME, text_upgrade_name(UPG_LINGERING_EMBERS));
    TEST_ASSERT_EQUAL_INT(TEXT_UPG_STORM_BRUSH_LV3, text_upgrade_effect(UPG_STORM_BRUSH, 3));
    TEST_ASSERT_EQUAL_INT(TEXT_UPG_DOUBLE_GALE_LV3, text_upgrade_effect(UPG_DOUBLE_GALE, 3));
    TEST_ASSERT_EQUAL_INT(TEXT_UPG_INKSTONE_BLESSING_LV3,
                          text_upgrade_effect(UPG_INKSTONE_BLESSING, 3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_name(UPG_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect(UPG_COUNT, 1));

    /* All 5 axes */
    for (int ax = 0; ax < (int)AXIS_COUNT; ++ax) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_axis_name((UpgradeAxis)ax));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_axis_name(AXIS_COUNT));

    /* All 3 acts (title, short name, subtitle, trait & tactical desc) */
    for (int act = 0; act < 3; ++act) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_title(act));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_short(act));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_subtitle(act));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_trait(act));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_desc(act));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_short(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_short(3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_subtitle(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_subtitle(3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_trait(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_trait(3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_desc(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_desc(3));

    /* All 15 relics (name & effect; tradeoffs on the 7 with drawbacks: 4 initial + 3 unlocked) */
    TEST_ASSERT_EQUAL_INT(15, RELIC_COUNT);
    int tradeoff_count = 0;
    for (int rid = 0; rid < (int)RELIC_COUNT; ++rid) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_name((RelicId)rid));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_effect((RelicId)rid));
        if (text_relic_tradeoff((RelicId)rid) != TEXT_NONE) {
            ++tradeoff_count;
        }
    }
    TEST_ASSERT_EQUAL_INT(7, tradeoff_count);
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_name(RELIC_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_effect(RELIC_COUNT));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_VERMILION_INKSTONE));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_TENGU_FAN));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_TWIN_BRUSH));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_PACT_OF_FIRE));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_INKSTORM_SECRET));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_ENDLESS_BRUSH));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_NIGHT_PARADE_SCROLL));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_THUNDER_DRUM));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_SEAL_OF_TRINITY));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_THUNDERFIRE_GEM));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_ONI_TILE));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_COUNT));

    /* Expanded settings, notices, endless, fallback, HUD, Result & Guide strings */
    static const TextId p2_ui_ids[] = {
        TEXT_SETTINGS_LANG,
        TEXT_SETTINGS_LANG_AUTO,
        TEXT_SETTINGS_LANG_JA,
        TEXT_SETTINGS_LANG_EN,
        TEXT_SETTINGS_REDUCE_FLASH,
        TEXT_SETTINGS_CURSOR_SCALE,
        TEXT_SETTINGS_RESET_HINTS,
        TEXT_SETTINGS_RESET_DEFAULTS,
        TEXT_SETTINGS_NOTICE_HINTS,
        TEXT_SETTINGS_NOTICE_DEFAULTS,
        TEXT_REWARD_CRYSTAL_TITLE,
        TEXT_REWARD_REPAIR_EXCESS,
        TEXT_RESULT_ENDLESS,
        TEXT_RESULT_CRYSTAL_ENDLESS,
        TEXT_RESULT_CRYSTAL_BONUS,
        TEXT_RESULT_CRYSTAL_BONUS_SHORT,
        TEXT_RESULT_REACTIONS_HDR,
        TEXT_RESULT_BREAKDOWN_HDR,
        TEXT_RESULT_CRYSTALS_PREVIEW_SHORT,
        TEXT_RESULT_MORE_ITEMS,
        TEXT_ENDLESS_ACT_PREFIX,
        TEXT_HUD_REMAINING_UNIT,
        TEXT_HUD_WAVE_CLEAR_SHORT,
        TEXT_HUD_LOOP_SHORT,
        TEXT_LOBBY_GUIDE,
        TEXT_LOBBY_GUIDE_BADGE,
        TEXT_GUIDE_PREV,
        TEXT_GUIDE_NEXT,
        TEXT_GUIDE_CLOSE,
        TEXT_GUIDE_FINISH,
        TEXT_GUIDE_NAV_HINT,
        TEXT_GUIDE_TIP_TAG,
        TEXT_GUIDE_S0_TITLE,
        TEXT_GUIDE_S1_TITLE,
        TEXT_GUIDE_S2_TITLE,
        TEXT_GUIDE_S3_TITLE,
        TEXT_GUIDE_S4_TITLE,
        TEXT_GUIDE_D4_BOKUAN_SUB,
    };
    for (size_t i = 0; i < sizeof p2_ui_ids / sizeof p2_ui_ids[0]; ++i) {
        TEST_ASSERT_TRUE(text_get(p2_ui_ids[i], true)[0] != '\0');
        TEST_ASSERT_TRUE(text_get(p2_ui_ids[i], false)[0] != '\0');
    }

    /* Bokuan 4 permanent upgrades & 6 relic unlocks */
    for (int k = 0; k < (int)META_UPG_COUNT; ++k) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_shop_upgrade_name((MetaUpgradeKind)k));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_shop_upgrade_desc((MetaUpgradeKind)k));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_shop_upgrade_name(META_UPG_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_shop_upgrade_desc(META_UPG_COUNT));

    for (uint32_t s = 0; s < (uint32_t)META_RELIC_UNLOCK_COUNT; ++s) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_shop_relic_name(s));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_shop_relic_desc(s));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_shop_relic_name((uint32_t)META_RELIC_UNLOCK_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_shop_relic_desc((uint32_t)META_RELIC_UNLOCK_COUNT));

    /* 3 Brush styles */
    for (int b = 0; b < (int)BRUSH_STYLE_COUNT; ++b) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_brush_name((BrushStyleId)b));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_brush_desc((BrushStyleId)b));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_brush_stats((BrushStyleId)b));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_brush_recommended((BrushStyleId)b));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_brush_name(BRUSH_STYLE_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_brush_desc(BRUSH_STYLE_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_brush_stats(BRUSH_STYLE_COUNT));

    /* 8 First-time hints */
    for (int h = 0; h < (int)HINT_COUNT; ++h) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_hint_message((HintId)h));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_hint_message(HINT_COUNT));
}

static float fake_measure(void *ctx, const char *text, size_t len)
{
    (void)ctx;
    float w = 0.0f;
    for (size_t i = 0; i < len; i = text_utf8_next(text, i, len)) {
        w += ((unsigned char)text[i] < 0x80u) ? 1.0f : 2.0f;
    }
    return w;
}

static void assert_line(const char *text, const TextLine *line, const char *expected)
{
    char buf[128];
    memcpy(buf, text + line->start, line->len);
    buf[line->len] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_text_layout_wraps_words_and_cjk_with_kinsoku(void)
{
    TextLine lines[TEXT_LAYOUT_MAX_LINES];
    bool truncated = true;

    const char *en = "Rad x1.25 / Force x1.2";
    int32_t n = text_layout_wrap(en, 12.0f, fake_measure, NULL, lines, 4, &truncated);
    TEST_ASSERT_EQUAL_INT32(2, n);
    TEST_ASSERT_FALSE(truncated);
    assert_line(en, &lines[0], "Rad x1.25 /");
    assert_line(en, &lines[1], "Force x1.2");

    /* 「墨と炎。」 must not leave 。 at a line start */
    const char *ja = "\xE5\xA2\xA8\xE3\x81\xA8\xE7\x82\x8E\xE3\x80\x82\xE9\x9B\xB7";
    n = text_layout_wrap(ja, 6.0f, fake_measure, NULL, lines, 4, &truncated);
    TEST_ASSERT_EQUAL_INT32(2, n);
    assert_line(ja, &lines[0], "\xE5\xA2\xA8\xE3\x81\xA8");
    assert_line(ja, &lines[1], "\xE7\x82\x8E\xE3\x80\x82\xE9\x9B\xB7");

    n = text_layout_wrap("aaaa bbbb cccc dddd", 4.0f, fake_measure, NULL, lines, 2, &truncated);
    TEST_ASSERT_EQUAL_INT32(2, n);
    TEST_ASSERT_TRUE(truncated);

    n = text_layout_wrap("abcdefghij", 4.0f, fake_measure, NULL, lines, 4, &truncated);
    TEST_ASSERT_EQUAL_INT32(3, n);
    assert_line("abcdefghij", &lines[2], "ij");
}

static void test_text_layout_fit_prefix_stops_on_codepoint_boundary(void)
{
    const char *ja = "\xE5\xA2\xA8\xE5\xB5\x90"
                     "abc";
    TEST_ASSERT_EQUAL_size_t(3u, text_layout_fit_prefix(ja, strlen(ja), 3.0f, fake_measure, NULL));
    TEST_ASSERT_EQUAL_size_t(7u, text_layout_fit_prefix(ja, strlen(ja), 5.0f, fake_measure, NULL));
    TEST_ASSERT_EQUAL_size_t(0u, text_layout_fit_prefix(ja, strlen(ja), 1.0f, fake_measure, NULL));
}

static void test_utf8_clip_never_splits_a_codepoint(void)
{
    char text[520];
    size_t n = 0;
    while (n + 3 <= 513) {
        text[n++] = (char)0xE5;
        text[n++] = (char)0xA2;
        text[n++] = (char)0xA8;
    }
    text[n] = '\0';
    TEST_ASSERT_EQUAL_size_t(513u, n);
    TEST_ASSERT_EQUAL_size_t(510u, text_utf8_clip(text, n, 511));
    TEST_ASSERT_EQUAL_size_t(507u, text_utf8_clip(text, n, 511 - 3));
    TEST_ASSERT_EQUAL_size_t(5u, text_utf8_clip("abcde", 5, 511));
    TEST_ASSERT_EQUAL_size_t(0u, text_utf8_clip(text, n, 2));
}

static void test_night_parade_scroll_mentions_dragon_orb_exception(void)
{
    const char *ja = text_get(TEXT_RELIC_NIGHT_PARADE_SCROLL_EFFECT, true);
    const char *en = text_get(TEXT_RELIC_NIGHT_PARADE_SCROLL_EFFECT, false);
    TEST_ASSERT_NOT_NULL(strstr(ja, "\xE9\xBE\x8D\xE7\x8E\x89"));
    TEST_ASSERT_NOT_NULL(strstr(en, "Dragon Orb"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_text_table_entries_valid);
    RUN_TEST(test_text_table_content_mappings);
    RUN_TEST(test_text_layout_wraps_words_and_cjk_with_kinsoku);
    RUN_TEST(test_text_layout_fit_prefix_stops_on_codepoint_boundary);
    RUN_TEST(test_utf8_clip_never_splits_a_codepoint);
    RUN_TEST(test_night_parade_scroll_mentions_dragon_orb_exception);
    return UNITY_END();
}
