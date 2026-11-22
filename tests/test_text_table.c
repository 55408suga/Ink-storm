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
    /* All 10 enemies & 3 boss epithets */
    for (int id = 0; id < (int)ENEMY_DEF_COUNT; ++id) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_name((EnemyDefId)id));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_intro((EnemyDefId)id));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_name(ENEMY_DEF_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_intro(ENEMY_DEF_COUNT));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_TENGU));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_NUE));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_RYU));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_ARCHER));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_DEF_COUNT));

    /* All 24 upgrades x Lv1..3 */
    for (int id = 0; id < (int)UPG_COUNT; ++id) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_upgrade_name((UpgradeId)id));
        for (int lv = 1; lv <= 3; ++lv) {
            TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_upgrade_effect((UpgradeId)id, lv));
        }
        TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect((UpgradeId)id, 0));
        TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect((UpgradeId)id, 4));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_name(UPG_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_upgrade_effect(UPG_COUNT, 1));

    /* All 5 axes */
    for (int ax = 0; ax < (int)AXIS_COUNT; ++ax) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_axis_name((UpgradeAxis)ax));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_axis_name(AXIS_COUNT));

    /* All 3 acts (title & short name) */
    for (int act = 0; act < 3; ++act) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_title(act));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_short(act));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(3));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_short(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_short(3));

    /* All 9 initial relics (name & effect; tradeoffs on the 4 with drawbacks) */
    for (int rid = 0; rid < (int)RELIC_COUNT; ++rid) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_name((RelicId)rid));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_effect((RelicId)rid));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_name(RELIC_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_effect(RELIC_COUNT));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_VERMILION_INKSTONE));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_TENGU_FAN));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_TWIN_BRUSH));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_relic_tradeoff(RELIC_PACT_OF_FIRE));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_THUNDER_DRUM));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_relic_tradeoff(RELIC_COUNT));

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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_text_table_entries_valid);
    RUN_TEST(test_text_table_content_mappings);
    return UNITY_END();
}
