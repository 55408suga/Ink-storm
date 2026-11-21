#include "audio.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_audio_skeleton_silence_and_null_game(void)
{
    AudioState audio;
    audio_init(&audio, UINT64_C(1));
    TEST_ASSERT_EQUAL_INT(BGM_NONE, audio_bgm_track(&audio));
    TEST_ASSERT_TRUE(audio_validate_finite(&audio));

    int16_t samples[64];
    memset(samples, 0x55, sizeof samples);
    audio_render(&audio, samples, sizeof samples / sizeof samples[0]);
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, samples[i]);
    }

    audio_consume_events(&audio, NULL);
    TEST_ASSERT_TRUE(audio_validate_finite(&audio));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_audio_skeleton_silence_and_null_game);
    return UNITY_END();
}
