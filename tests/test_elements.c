#include "fluid.h"
#include "game.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_elements_skeleton_finite(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);

    game_debug_start_wave(g, 0);
    for (int i = 0; i < 60; ++i) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));

    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_elements_skeleton_finite);
    return UNITY_END();
}
