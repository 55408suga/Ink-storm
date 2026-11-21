#include "text_table.h"
#include "unity.h"

#include <stddef.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_text_table_entries_valid(void)
{
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_text_table_entries_valid);
    return UNITY_END();
}
