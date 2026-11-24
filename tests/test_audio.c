#include "audio.h"
#include "game.h"
#include "synth_voices.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static int32_t peak_abs(const int16_t *samples, size_t n)
{
    int32_t peak = 0;
    for (size_t i = 0; i < n; ++i) {
        const int32_t v = samples[i] < 0 ? -(int32_t)samples[i] : (int32_t)samples[i];
        if (v > peak) {
            peak = v;
        }
    }
    return peak;
}

static void test_audio_initial_state(void)
{
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(1));
    TEST_ASSERT_EQUAL_INT(BGM_NONE, audio_bgm_track(a));
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    int16_t samples[256];
    memset(samples, 0x55, sizeof samples);
    audio_render(a, samples, 256);
    for (size_t i = 0; i < 256; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, samples[i]);
    }

    audio_consume_events(a, NULL);
    audio_render(a, samples, 0);
    TEST_ASSERT_TRUE(audio_validate_finite(a));
    free(a);
}

static void test_audio_se_events_audible(void)
{
    static const GameEventType se_events[] = {
        EV_KILL,         EV_DEFLECT,    EV_SLAM,           EV_IGNITE,        EV_STORM_ZAP,
        EV_THUNDERBURST, EV_CATACLYSM,  EV_WIND_TELEGRAPH, EV_CYCLONE,       EV_GALE,
        EV_ORB_PICKUP,   EV_SHRINE_HIT, EV_SURGE_START,    EV_BOSS_ENTER,    EV_BOSS_PHASE,
        EV_WAVE_CLEAR,   EV_ACT_CLEAR,  EV_REWARD_PICK,    EV_REWARD_REROLL, EV_BOSS_PART_BREAK,
        EV_SKILL_DENIED, EV_GAME_OVER,  EV_PURIFY,
    };

    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(AUDIO_SAMPLE_RATE, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);

    for (size_t i = 0; i < sizeof se_events / sizeof se_events[0]; ++i) {
        audio_init(a, UINT64_C(42) + i);
        audio_set_volumes(a, 80, 0, 85);
        g->phase = RUN_PHASE_GAME_OVER;
        g->run.chain = 3;
        g->event_count = 1;
        memset(&g->events[0], 0, sizeof g->events[0]);
        g->events[0].type = se_events[i];

        audio_consume_events(a, g);
        audio_render(a, buf, AUDIO_SAMPLE_RATE);

        const int32_t peak = peak_abs(buf, AUDIO_SAMPLE_RATE);
        TEST_ASSERT_TRUE_MESSAGE(peak >= 1000 && peak <= 32767, "SE peak out of range");
        TEST_ASSERT_TRUE(audio_validate_finite(a));
    }

    free(buf);
    free(a);
    game_destroy(g);
}

static void test_audio_lobby_external_pause_and_stamp(void)
{
    AudioState *a = calloc(1, sizeof *a);
    AudioState *b = calloc(1, sizeof *b);
    int16_t *buf_a = calloc(AUDIO_SAMPLE_RATE, sizeof *buf_a);
    int16_t *buf_b = calloc(AUDIO_SAMPLE_RATE, sizeof *buf_b);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(buf_a);
    TEST_ASSERT_NOT_NULL(buf_b);

    /* 1. Lobby mode switches to BGM_LOBBY and renders deterministic finite audio */
    audio_init(a, UINT64_C(77));
    audio_init(b, UINT64_C(77));
    audio_set_lobby_mode(a, true);
    audio_set_lobby_mode(b, true);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, audio_bgm_track(a));
    audio_render(a, buf_a, AUDIO_SAMPLE_RATE);
    audio_render(b, buf_b, AUDIO_SAMPLE_RATE);
    TEST_ASSERT_TRUE(peak_abs(buf_a, AUDIO_SAMPLE_RATE) >= 200);
    TEST_ASSERT_EQUAL_MEMORY(buf_a, buf_b, AUDIO_SAMPLE_RATE * sizeof buf_a[0]);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    /* 2. External BGM suppresses procedural BGM instruments while SE still plays */
    audio_init(a, UINT64_C(88));
    audio_set_volumes(a, 100, 100, 0);
    audio_set_lobby_mode(a, true);
    a->wind_target = 0.0f;
    audio_set_external_bgm(a, true);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, audio_bgm_track(a));
    audio_render(a, buf_a, AUDIO_SAMPLE_RATE / 2);
    TEST_ASSERT_EQUAL_INT32(0, peak_abs(buf_a, AUDIO_SAMPLE_RATE / 2));

    audio_set_volumes(a, 100, 100, 85);
    audio_trigger_ui_stamp(a);
    audio_render(a, buf_a, AUDIO_SAMPLE_RATE / 2);
    TEST_ASSERT_TRUE(peak_abs(buf_a, AUDIO_SAMPLE_RATE / 2) >= 1000);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    /* 3. Pause zeroes brush_target */
    audio_init(a, UINT64_C(99));
    a->brush_target = 0.85f;
    audio_set_paused(a, true);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, a->brush_target);
    TEST_ASSERT_TRUE(a->paused);

    free(buf_b);
    free(buf_a);
    free(b);
    free(a);
}

static void test_audio_heavy_load(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);

    audio_init(a, UINT64_C(99));
    g->phase = RUN_PHASE_PLAYING;
    g->run.wave_index = GAME_BOSS_WAVE_INDEX;
    g->run.chain = 5;

    int16_t buf[AUDIO_SAMPLES_PER_TICK];
    for (int tick = 0; tick < 120; ++tick) {
        g->event_count = GAME_MAX_EVENTS;
        for (int i = 0; i < GAME_MAX_EVENTS; ++i) {
            memset(&g->events[i], 0, sizeof g->events[i]);
            g->events[i].type = (GameEventType)(1 + ((tick + i) % (EV_TYPE_COUNT - 1)));
        }
        audio_consume_events(a, g);
        audio_render(a, buf, AUDIO_SAMPLES_PER_TICK);
        for (size_t s = 0; s < AUDIO_SAMPLES_PER_TICK; ++s) {
            TEST_ASSERT_TRUE(buf[s] >= -32767 && buf[s] <= 32767);
        }
        TEST_ASSERT_TRUE(audio_validate_finite(a));
    }

    free(a);
    game_destroy(g);
}

static void test_audio_bgm_tracks(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);

    audio_init(a, UINT64_C(7));
    g->event_count = 0;

    typedef struct {
        RunPhase phase;
        int32_t wave;
        BgmTrack expected;
    } BgmCase;

    const BgmCase cases[] = {
        {RUN_PHASE_ACT_TRANSITION, 0, BGM_BATTLE},
        {RUN_PHASE_PLAYING, 0, BGM_BATTLE},
        {RUN_PHASE_WAVE_AFTERMATH, 2, BGM_BATTLE},
        {RUN_PHASE_REWARD, 2, BGM_BATTLE},
        {RUN_PHASE_PLAYING, GAME_BOSS_WAVE_INDEX, BGM_BOSS},
        {RUN_PHASE_WAVE_AFTERMATH, GAME_BOSS_WAVE_INDEX, BGM_BOSS},
        {RUN_PHASE_VICTORY, GAME_BOSS_WAVE_INDEX, BGM_VICTORY},
        {RUN_PHASE_REWARD, GAME_BOSS_WAVE_INDEX, BGM_BATTLE}, /* Endless continuation */
        {RUN_PHASE_ACT_TRANSITION, 0, BGM_BATTLE},
        {RUN_PHASE_SHRINE_BREAK, 1, BGM_NONE},
        {RUN_PHASE_GAME_OVER, 1, BGM_NONE},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        g->phase = cases[i].phase;
        g->run.wave_index = cases[i].wave;
        if (i == 7) {
            g->run.endless_loop = 1;
        }
        audio_consume_events(a, g);
        TEST_ASSERT_EQUAL_INT(cases[i].expected, audio_bgm_track(a));
    }

    /* Verify rendering across BGM_VICTORY -> BGM_BATTLE endless transition stays finite */
    audio_init(a, UINT64_C(8));
    int16_t buf[AUDIO_SAMPLES_PER_TICK * 4];
    g->phase = RUN_PHASE_VICTORY;
    g->run.endless_loop = 0;
    audio_consume_events(a, g);
    audio_render(a, buf, AUDIO_SAMPLES_PER_TICK);
    TEST_ASSERT_EQUAL_INT(BGM_VICTORY, audio_bgm_track(a));
    TEST_ASSERT_EQUAL_INT(BGM_VICTORY, a->active_bgm);

    g->phase = RUN_PHASE_REWARD;
    g->run.endless_loop = 1;
    audio_consume_events(a, g);
    audio_render(a, buf, AUDIO_SAMPLES_PER_TICK * 4);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, audio_bgm_track(a));
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, a->active_bgm);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    free(a);
    game_destroy(g);
}

static void test_audio_volumes(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    AudioState *b = calloc(1, sizeof *b);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    g->phase = RUN_PHASE_PLAYING;
    g->event_count = 1;
    memset(&g->events[0], 0, sizeof g->events[0]);
    g->events[0].type = EV_KILL;

    int16_t buf_a[AUDIO_SAMPLES_PER_TICK * 4];
    int16_t buf_b[AUDIO_SAMPLES_PER_TICK * 4];

    /* master = 0 -> silence */
    audio_init(a, UINT64_C(11));
    audio_set_volumes(a, 0, 100, 100);
    audio_consume_events(a, g);
    audio_render(a, buf_a, sizeof buf_a / sizeof buf_a[0]);
    TEST_ASSERT_EQUAL_INT32(0, peak_abs(buf_a, sizeof buf_a / sizeof buf_a[0]));

    /* se = 0 with bgm = 0 and SE-only -> silence */
    audio_init(a, UINT64_C(11));
    audio_set_volumes(a, 100, 0, 0);
    audio_consume_events(a, g);
    audio_render(a, buf_a, sizeof buf_a / sizeof buf_a[0]);
    TEST_ASSERT_EQUAL_INT32(0, peak_abs(buf_a, sizeof buf_a / sizeof buf_a[0]));

    /* 200 clamped to 100 */
    audio_init(a, UINT64_C(11));
    audio_init(b, UINT64_C(11));
    audio_set_volumes(a, 200, 200, 200);
    audio_set_volumes(b, 100, 100, 100);
    audio_consume_events(a, g);
    audio_consume_events(b, g);
    audio_render(a, buf_a, sizeof buf_a / sizeof buf_a[0]);
    audio_render(b, buf_b, sizeof buf_b / sizeof buf_b[0]);
    TEST_ASSERT_EQUAL_MEMORY(buf_a, buf_b, sizeof buf_a);

    free(b);
    free(a);
    game_destroy(g);
}

static void setup_tick_event(GameState *g, int tick)
{
    g->phase = RUN_PHASE_PLAYING;
    g->run.wave_index = 0;
    g->run.chain = tick + 1;
    g->event_count = 1;
    memset(&g->events[0], 0, sizeof g->events[0]);
    if (tick == 0) {
        g->events[0].type = EV_CYCLONE;
    } else if (tick == 1) {
        g->events[0].type = EV_KILL;
    } else {
        g->events[0].type = EV_DEFLECT;
    }
}

static void test_audio_determinism(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    AudioState *b = calloc(1, sizeof *b);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    enum { TOTAL = AUDIO_SAMPLES_PER_TICK * 3 };
    int16_t out_a[TOTAL];
    int16_t out_b[TOTAL];

    /* Per-tick consume + render */
    audio_init(a, UINT64_C(12345));
    for (int t = 0; t < 3; ++t) {
        setup_tick_event(g, t);
        audio_consume_events(a, g);
        audio_render(a, out_a + (size_t)t * AUDIO_SAMPLES_PER_TICK, AUDIO_SAMPLES_PER_TICK);
    }

    /* Batched 0, 2, 1 ticks */
    audio_init(b, UINT64_C(12345));
    audio_render(b, out_b, 0);
    setup_tick_event(g, 0);
    audio_consume_events(b, g);
    setup_tick_event(g, 1);
    audio_consume_events(b, g);
    audio_render(b, out_b, AUDIO_SAMPLES_PER_TICK * 2);
    setup_tick_event(g, 2);
    audio_consume_events(b, g);
    audio_render(b, out_b + AUDIO_SAMPLES_PER_TICK * 2, AUDIO_SAMPLES_PER_TICK);

    TEST_ASSERT_EQUAL_MEMORY(out_a, out_b, sizeof out_a);

    free(b);
    free(a);
    game_destroy(g);
}

static void test_audio_ticks_keep_subtick_offsets_after_renderer_runs_ahead(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(AUDIO_SAMPLE_RATE, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);
    audio_init(a, UINT64_C(5));
    g->phase = RUN_PHASE_PLAYING;
    g->event_count = 0;

    const size_t ahead = AUDIO_SAMPLES_PER_TICK * 10u + 100u;
    audio_render(a, buf, ahead);
    for (int t = 0; t < 3; ++t) {
        audio_consume_events(a, g);
    }
    TEST_ASSERT_EQUAL_INT32(3, a->state_q_count);
    for (int t = 0; t < 3; ++t) {
        const AudioTickState *st = &a->state_q[(a->state_q_head + t) % AUDIO_STATE_QUEUE_CAP];
        TEST_ASSERT_EQUAL_UINT64(ahead + (uint64_t)t * AUDIO_SAMPLES_PER_TICK, st->at_sample);
    }
    free(buf);
    free(a);
    game_destroy(g);
}

static void test_audio_lobby_entry_drops_stale_combat_state(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t buf[AUDIO_SAMPLES_PER_TICK * 4];
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(6));

    g->phase = RUN_PHASE_PLAYING;
    g->player.painting = true;
    g->player.stroke_speed = 900.0f;
    g->event_count = 1;
    memset(&g->events[0], 0, sizeof g->events[0]);
    g->events[0].type = EV_SHRINE_HIT;
    for (int t = 0; t < 4; ++t) {
        audio_consume_events(a, g);
    }
    audio_render(a, buf, 10);
    TEST_ASSERT_TRUE(a->state_q_count > 0);

    audio_set_lobby_mode(a, true);
    TEST_ASSERT_EQUAL_INT32(0, a->state_q_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, a->brush_target);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        TEST_ASSERT_TRUE(!a->voices[i].active || a->voices[i].releasing);
    }
    audio_render(a, buf, sizeof buf / sizeof buf[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, a->brush_target);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, a->active_bgm);

    g->event_count = 0;
    g->config.brush_style = BRUSH_STYLE_QUILL;
    audio_consume_lobby_events(a, g);
    audio_render(a, buf, AUDIO_SAMPLES_PER_TICK * 2);
    TEST_ASSERT_TRUE(a->lobby_mode);
    TEST_ASSERT_TRUE(a->brush_target > 0.5f);
    TEST_ASSERT_EQUAL_INT32(BRUSH_STYLE_QUILL, a->brush_style);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, a->active_bgm);
    free(a);
    game_destroy(g);
}

static void test_audio_spam_never_steals_milestone_voices(void)
{
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(9));
    audio_voice_spawn(a, AUDIO_VOICE_WAVE_CLEAR, 0, 392.0f, 0.0f);
    audio_voice_spawn(a, AUDIO_VOICE_SHRINE_GONG, 0, 152.0f, 0.0f);
    for (int i = 0; i < 60; ++i) {
        audio_voice_spawn(a, AUDIO_VOICE_STORM_ZAP, 0, 1320.0f, 0.0f);
        audio_voice_spawn(a, AUDIO_VOICE_KILL_TAIKO, 0, 135.0f, 0.0f);
        audio_voice_spawn(a, AUDIO_VOICE_CYCLONE_SWIRL, 0, 145.0f, 0.0f);
    }
    int wave_clear = 0;
    int gong = 0;
    int zaps = 0;
    int kills = 0;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!a->voices[i].active) {
            continue;
        }
        wave_clear += a->voices[i].kind == AUDIO_VOICE_WAVE_CLEAR ? 1 : 0;
        gong += a->voices[i].kind == AUDIO_VOICE_SHRINE_GONG ? 1 : 0;
        zaps += a->voices[i].kind == AUDIO_VOICE_STORM_ZAP ? 1 : 0;
        kills += a->voices[i].kind == AUDIO_VOICE_KILL_TAIKO ? 1 : 0;
    }
    TEST_ASSERT_EQUAL_INT(1, wave_clear);
    TEST_ASSERT_EQUAL_INT(1, gong);
    TEST_ASSERT_TRUE(zaps <= 3);
    TEST_ASSERT_TRUE(kills <= 4);
    free(a);
}

static void render_brush(AudioState *a, int32_t style, bool dry, bool surge, int16_t *buf, size_t n)
{
    audio_init(a, UINT64_C(21));
    audio_set_volumes(a, 100, 0, 100);
    a->brush_target = 0.9f;
    a->brush_env = 0.9f;
    a->brush_style = style;
    a->brush_dry = dry;
    a->brush_surge = surge;
    audio_render(a, buf, n);
}

static void test_audio_brush_scrape_differs_per_brush_and_state(void)
{
    enum { N = AUDIO_SAMPLES_PER_TICK * 6 };
    AudioState *a = calloc(1, sizeof *a);
    int16_t (*bufs)[N] = calloc(5, sizeof *bufs);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(bufs);
    render_brush(a, BRUSH_STYLE_HEAVY, false, false, bufs[0], N);
    render_brush(a, BRUSH_STYLE_EMBER, false, false, bufs[1], N);
    render_brush(a, BRUSH_STYLE_QUILL, false, false, bufs[2], N);
    render_brush(a, BRUSH_STYLE_HEAVY, true, false, bufs[3], N);
    render_brush(a, BRUSH_STYLE_HEAVY, false, true, bufs[4], N);
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(peak_abs(bufs[i], N) > 200);
        for (int j = i + 1; j < 5; ++j) {
            TEST_ASSERT_TRUE(memcmp(bufs[i], bufs[j], sizeof bufs[i]) != 0);
        }
    }
    free(bufs);
    free(a);
}

static void test_audio_pause_ducks_procedural_bgm(void)
{
    enum { N = AUDIO_SAMPLE_RATE };
    AudioState *a = calloc(1, sizeof *a);
    int16_t *loud = calloc(N, sizeof *loud);
    int16_t *ducked = calloc(N, sizeof *ducked);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(loud);
    TEST_ASSERT_NOT_NULL(ducked);
    audio_init(a, UINT64_C(31));
    audio_set_volumes(a, 100, 100, 0);
    audio_set_lobby_mode(a, true);
    audio_render(a, loud, N);
    audio_render(a, loud, N);

    audio_init(a, UINT64_C(31));
    audio_set_volumes(a, 100, 100, 0);
    audio_set_lobby_mode(a, true);
    audio_set_paused(a, true);
    audio_render(a, ducked, N);
    audio_render(a, ducked, N);
    TEST_ASSERT_TRUE(peak_abs(ducked, N) * 2 < peak_abs(loud, N));
    TEST_ASSERT_TRUE(peak_abs(ducked, N) > 0);
    free(ducked);
    free(loud);
    free(a);
}

static void test_audio_ui_cues_are_audible(void)
{
    static const AudioUiCue cues[] = {AUDIO_UI_CLICK, AUDIO_UI_BACK, AUDIO_UI_DENY,
                                      AUDIO_UI_PREVIEW};
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(AUDIO_SAMPLE_RATE / 4, sizeof *buf);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);
    for (size_t i = 0; i < sizeof cues / sizeof cues[0]; ++i) {
        audio_init(a, UINT64_C(3));
        audio_set_volumes(a, 100, 0, 100);
        audio_trigger_ui_cue(a, cues[i]);
        audio_render(a, buf, AUDIO_SAMPLE_RATE / 4);
        TEST_ASSERT_TRUE(peak_abs(buf, AUDIO_SAMPLE_RATE / 4) >= 1000);
    }
    free(buf);
    free(a);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_audio_initial_state);
    RUN_TEST(test_audio_se_events_audible);
    RUN_TEST(test_audio_lobby_external_pause_and_stamp);
    RUN_TEST(test_audio_heavy_load);
    RUN_TEST(test_audio_bgm_tracks);
    RUN_TEST(test_audio_volumes);
    RUN_TEST(test_audio_determinism);
    RUN_TEST(test_audio_ticks_keep_subtick_offsets_after_renderer_runs_ahead);
    RUN_TEST(test_audio_lobby_entry_drops_stale_combat_state);
    RUN_TEST(test_audio_spam_never_steals_milestone_voices);
    RUN_TEST(test_audio_brush_scrape_differs_per_brush_and_state);
    RUN_TEST(test_audio_pause_ducks_procedural_bgm);
    RUN_TEST(test_audio_ui_cues_are_audible);
    return UNITY_END();
}
