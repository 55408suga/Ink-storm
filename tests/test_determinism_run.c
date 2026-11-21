#include "game.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_determinism_run_skeleton(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x1234));
    GameState *a = game_create(&cfg);
    GameState *b = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    for (int tick = 0; tick < 300; ++tick) {
        const BrushInput in_a = game_autoplay_input(a);
        const BrushInput in_b = game_autoplay_input(b);
        const GameCommand cmd_a = game_autoplay_command(a);
        const GameCommand cmd_b = game_autoplay_command(b);
        game_step(a, &in_a, &cmd_a);
        game_step(b, &in_b, &cmd_b);

        uint64_t sec_a[GAME_HASH_SECTION_COUNT];
        uint64_t sec_b[GAME_HASH_SECTION_COUNT];
        game_hash_sections(a, sec_a);
        game_hash_sections(b, sec_b);
        for (int s = 0; s < GAME_HASH_SECTION_COUNT; ++s) {
            TEST_ASSERT_EQUAL_HEX64(sec_a[s], sec_b[s]);
        }
    }

    game_destroy(b);
    game_destroy(a);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_determinism_run_skeleton);
    return UNITY_END();
}
