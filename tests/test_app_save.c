#include "meta.h"
#include "save.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_save_load_missing_returns_default(void)
{
    MetaState expected;
    meta_init_default(&expected);

    MetaState actual;
    memset(&actual, 0xaa, sizeof actual);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_MISSING, save_load(NULL, &actual));
    TEST_ASSERT_EQUAL_UINT32(expected.version, actual.version);
    TEST_ASSERT_EQUAL_INT32(expected.ink_crystals, actual.ink_crystals);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.upgrade_lv, actual.upgrade_lv, META_UPG_COUNT);
    TEST_ASSERT_EQUAL_HEX64(expected.unlocked_relic_mask, actual.unlocked_relic_mask);
    TEST_ASSERT_EQUAL_INT32(expected.best_score, actual.best_score);
    TEST_ASSERT_EQUAL_INT32(expected.best_act, actual.best_act);
    TEST_ASSERT_EQUAL_INT32(expected.runs_played, actual.runs_played);
    TEST_ASSERT_EQUAL_INT32(expected.clears, actual.clears);
    TEST_ASSERT_EQUAL_HEX32(expected.seen_hints_mask, actual.seen_hints_mask);
    TEST_ASSERT_EQUAL_UINT8(expected.settings.master_vol, actual.settings.master_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.settings.bgm_vol, actual.settings.bgm_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.settings.se_vol, actual.settings.se_vol);
    TEST_ASSERT_EQUAL_UINT8(expected.settings.shake_scale, actual.settings.shake_scale);
    TEST_ASSERT_EQUAL(expected.settings.post_shader_on, actual.settings.post_shader_on);
    TEST_ASSERT_EQUAL(expected.settings.hints_on, actual.settings.hints_on);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_save_load_missing_returns_default);
    return UNITY_END();
}
