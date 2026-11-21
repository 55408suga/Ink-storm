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
    for (int id = 0; id < (int)ENEMY_DEF_COUNT; ++id) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_name((EnemyDefId)id));
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_enemy_intro((EnemyDefId)id));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_name(ENEMY_DEF_COUNT));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_enemy_intro(ENEMY_DEF_COUNT));
    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_boss_epithet(ENEMY_BOSS_TENGU));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_ARCHER));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_boss_epithet(ENEMY_DEF_COUNT));

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

    for (int ax = 0; ax < (int)AXIS_COUNT; ++ax) {
        TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_axis_name((UpgradeAxis)ax));
    }
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_axis_name(AXIS_COUNT));

    TEST_ASSERT_NOT_EQUAL(TEXT_NONE, text_act_title(0));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(-1));
    TEST_ASSERT_EQUAL_INT(TEXT_NONE, text_act_title(1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_text_table_entries_valid);
    RUN_TEST(test_text_table_content_mappings);
    return UNITY_END();
}
