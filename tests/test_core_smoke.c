#include "audio.h"
#include "core.h"
#include "ecs.h"
#include "fluid.h"
#include "game.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_clamp(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, ink_clampf(-1.0f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.5f, ink_clampf(0.5f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 1.0f, ink_clampf(2.0f, 0.0f, 1.0f));
}

static void test_splitmix64(void)
{
    uint64_t seed = 0;
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xe220a8397b1dcdaf), ink_rng_next(&seed));
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0x6e789e6aa1b965f4), ink_rng_next(&seed));

    seed = 0;
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0x1.c4415p-1f, ink_rng_f01(&seed));
}

static void test_game_advances_fluid_and_rng(void)
{
    GameState *game = game_create(UINT64_C(7));
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_UINT64(0, game->ticks);
    TEST_ASSERT_EQUAL_UINT64(0, game->fluid.steps);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(&game->ecs));

    game_step_smoke(game);
    game_step_smoke(game);

    uint64_t expected_rng = 7;
    ink_rng_next(&expected_rng);
    ink_rng_next(&expected_rng);
    TEST_ASSERT_EQUAL_UINT64(2, game->ticks);
    TEST_ASSERT_EQUAL_UINT64(2, game->fluid.steps);
    TEST_ASSERT_EQUAL_UINT64(expected_rng, game->rng);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(&game->ecs));
    TEST_ASSERT_TRUE(fluid_validate_finite(&game->fluid));
    game_destroy(game);
}

static void test_audio_writes_silence(void)
{
    AudioState audio;
    int16_t samples[] = {16, -16, 23, -23};
    const int16_t silence[] = {0, 0, 0, 0};
    audio_init(&audio);
    TEST_ASSERT_EQUAL_UINT64(0, audio.samples_rendered);

    audio_render_silence(&audio, samples, sizeof samples / sizeof samples[0]);
    TEST_ASSERT_EQUAL_INT16_ARRAY(silence, samples, sizeof samples / sizeof samples[0]);
    TEST_ASSERT_EQUAL_UINT64(sizeof samples / sizeof samples[0], audio.samples_rendered);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clamp);
    RUN_TEST(test_splitmix64);
    RUN_TEST(test_game_advances_fluid_and_rng);
    RUN_TEST(test_audio_writes_silence);
    return UNITY_END();
}
