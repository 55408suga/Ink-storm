#include "audio.h"
#include "game.h"
#include "synth_voices.h"
#include "unity.h"

#include <math.h>
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
    enum { N = AUDIO_SAMPLES_PER_TICK * 6, VARIANTS = 5 };
    AudioState *a = calloc(1, sizeof *a);
    int16_t *bufs = calloc((size_t)N * VARIANTS, sizeof *bufs);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(bufs);
    render_brush(a, BRUSH_STYLE_HEAVY, false, false, bufs, N);
    render_brush(a, BRUSH_STYLE_EMBER, false, false, bufs + N, N);
    render_brush(a, BRUSH_STYLE_QUILL, false, false, bufs + 2 * N, N);
    render_brush(a, BRUSH_STYLE_HEAVY, true, false, bufs + 3 * N, N);
    render_brush(a, BRUSH_STYLE_HEAVY, false, true, bufs + 4 * N, N);
    for (int i = 0; i < VARIANTS; ++i) {
        TEST_ASSERT_TRUE(peak_abs(bufs + i * N, N) > 200);
        for (int j = i + 1; j < VARIANTS; ++j) {
            TEST_ASSERT_TRUE(memcmp(bufs + i * N, bufs + j * N, N * sizeof *bufs) != 0);
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

static void one_event(GameState *g, GameEventType type)
{
    g->phase = RUN_PHASE_PLAYING;
    g->event_count = 1;
    memset(&g->events[0], 0, sizeof g->events[0]);
    g->events[0].type = type;
}

static void test_audio_batched_same_kind_voices_match_per_tick(void)
{
    enum { TOTAL = AUDIO_SAMPLES_PER_TICK * 3 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    AudioState *b = calloc(1, sizeof *b);
    int16_t *out_a = calloc(TOTAL, sizeof *out_a);
    int16_t *out_b = calloc(TOTAL, sizeof *out_b);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(out_a);
    TEST_ASSERT_NOT_NULL(out_b);

    audio_init(a, UINT64_C(4));
    for (int t = 0; t < 3; ++t) {
        one_event(g, EV_SKILL_DENIED);
        audio_consume_events(a, g);
        audio_render(a, out_a + (size_t)t * AUDIO_SAMPLES_PER_TICK, AUDIO_SAMPLES_PER_TICK);
    }

    audio_init(b, UINT64_C(4));
    for (int t = 0; t < 3; ++t) {
        one_event(g, EV_SKILL_DENIED);
        audio_consume_events(b, g);
    }
    audio_render(b, out_b, TOTAL);
    TEST_ASSERT_EQUAL_MEMORY(out_a, out_b, TOTAL * sizeof *out_a);

    free(out_b);
    free(out_a);
    free(b);
    free(a);
    game_destroy(g);
}

static void test_audio_scene_change_drops_unplayed_combat_voice_but_keeps_ui(void)
{
    enum { N = AUDIO_SAMPLE_RATE / 4 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(N, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);

    audio_init(a, UINT64_C(5));
    audio_set_volumes(a, 100, 0, 100);
    one_event(g, EV_SKILL_DENIED);
    audio_consume_events(a, g);
    audio_set_lobby_mode(a, true);
    audio_render(a, buf, N);
    TEST_ASSERT_EQUAL_INT32(0, peak_abs(buf, N));

    audio_init(a, UINT64_C(5));
    audio_set_volumes(a, 100, 0, 100);
    audio_trigger_ui_cue(a, AUDIO_UI_DENY);
    audio_set_lobby_mode(a, true);
    audio_render(a, buf, N);
    TEST_ASSERT_TRUE(peak_abs(buf, N) >= 1000);

    free(buf);
    free(a);
    game_destroy(g);
}

static void render_skill(AudioState *a, GameState *g, GameEventType type, bool dry, float surge,
                         int16_t *buf, size_t n)
{
    audio_init(a, UINT64_C(6));
    audio_set_volumes(a, 100, 0, 100);
    g->player.dry = dry;
    g->player.surge_t = surge;
    one_event(g, type);
    audio_consume_events(a, g);
    audio_render(a, buf, n);
}

static void test_audio_skill_voices_reflect_dry_and_surge(void)
{
    enum { N = AUDIO_SAMPLES_PER_TICK * 8 };
    static const GameEventType k_skills[2] = {EV_CYCLONE, EV_GALE};
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *bufs = calloc((size_t)N * 3, sizeof *bufs);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(bufs);
    for (int k = 0; k < 2; ++k) {
        render_skill(a, g, k_skills[k], false, 0.0f, bufs, N);
        render_skill(a, g, k_skills[k], true, 0.0f, bufs + N, N);
        render_skill(a, g, k_skills[k], false, 2.0f, bufs + 2 * N, N);
        TEST_ASSERT_TRUE(memcmp(bufs, bufs + N, N * sizeof *bufs) != 0);
        TEST_ASSERT_TRUE(memcmp(bufs, bufs + 2 * N, N * sizeof *bufs) != 0);
    }
    free(bufs);
    free(a);
    game_destroy(g);
}

static double rms_of(const int16_t *samples, size_t n)
{
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += (double)samples[i] * (double)samples[i];
    }
    return n > 0 ? sqrt(sum / (double)n) : 0.0;
}

typedef struct {
    GameState *g;
    AudioState *a;
    int16_t tick[AUDIO_SAMPLES_PER_TICK];
} BrushRig;

static BrushRig *brush_rig_create(void)
{
    BrushRig *rig = calloc(1, sizeof *rig);
    TEST_ASSERT_NOT_NULL(rig);
    rig->g = game_create(NULL);
    rig->a = calloc(1, sizeof *rig->a);
    TEST_ASSERT_NOT_NULL(rig->g);
    TEST_ASSERT_NOT_NULL(rig->a);
    audio_init(rig->a, UINT64_C(11));
    audio_set_volumes(rig->a, 100, 0, 100);
    audio_set_lobby_mode(rig->a, true);
    rig->g->config.brush_style = BRUSH_STYLE_HEAVY;
    rig->g->event_count = 0;
    return rig;
}

static void brush_rig_destroy(BrushRig *rig)
{
    TEST_ASSERT_TRUE(audio_validate_finite(rig->a));
    game_destroy(rig->g);
    free(rig->a);
    free(rig);
}

static void brush_rig_queue(BrushRig *rig, bool painting, float stroke_speed)
{
    rig->g->player.painting = painting;
    rig->g->player.stroke_speed = stroke_speed;
    audio_consume_lobby_events(rig->a, rig->g);
}

static double brush_rig_tick(BrushRig *rig, bool painting, float stroke_speed)
{
    brush_rig_queue(rig, painting, stroke_speed);
    audio_render(rig->a, rig->tick, AUDIO_SAMPLES_PER_TICK);
    return rms_of(rig->tick, AUDIO_SAMPLES_PER_TICK);
}

static double brush_rig_steady_rms(BrushRig *rig, float stroke_speed)
{
    for (int t = 0; t < 12; ++t) {
        (void)brush_rig_tick(rig, true, stroke_speed);
    }
    double steady = 0.0;
    for (int t = 0; t < 6; ++t) {
        steady += brush_rig_tick(rig, true, stroke_speed) / 6.0;
    }
    TEST_ASSERT_TRUE(steady > 200.0);
    return steady;
}

static void test_audio_brush_rides_through_a_still_tick_while_held(void)
{
    BrushRig *rig = brush_rig_create();
    const double steady = brush_rig_steady_rms(rig, 600.0f);

    TEST_ASSERT_TRUE(brush_rig_tick(rig, true, 0.0f) > 0.8 * steady);
    TEST_ASSERT_TRUE(brush_rig_tick(rig, true, 600.0f) > 0.8 * steady);
    brush_rig_destroy(rig);
}

static void test_audio_brush_releases_from_the_first_sample_after_a_lift(void)
{
    BrushRig *rig = brush_rig_create();
    (void)brush_rig_steady_rms(rig, 600.0f);
    const float held_env = rig->a->brush_env;

    brush_rig_queue(rig, false, 0.0f);
    audio_render(rig->a, rig->tick, 1u);
    TEST_ASSERT_TRUE(rig->a->brush_env < held_env);
    audio_render(rig->a, rig->tick, AUDIO_SAMPLES_PER_TICK - 1u);
    TEST_ASSERT_TRUE(rig->a->brush_env < 0.65f * held_env);

    for (int t = 1; t < 6; ++t) {
        (void)brush_rig_tick(rig, false, 0.0f);
    }
    TEST_ASSERT_TRUE(rig->a->brush_env < 0.05f * held_env);

    for (int t = 0; t < 120; ++t) {
        (void)brush_rig_tick(rig, false, 0.0f);
    }
    TEST_ASSERT_EQUAL_INT32(0, peak_abs(rig->tick, AUDIO_SAMPLES_PER_TICK));
    brush_rig_destroy(rig);
}

static void test_audio_fast_stroke_from_silence_reaches_full_level_by_its_second_tick(void)
{
    BrushRig *rig = brush_rig_create();
    for (int t = 0; t < 4; ++t) {
        (void)brush_rig_tick(rig, false, 0.0f);
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, rig->a->brush_env);
    (void)brush_rig_tick(rig, true, 900.0f);
    const double second_tick = brush_rig_tick(rig, true, 900.0f);
    const double steady = brush_rig_steady_rms(rig, 900.0f);
    TEST_ASSERT_TRUE(second_tick > 0.9 * steady);
    brush_rig_destroy(rig);
}

static void fill_test_loop(int16_t *pcm, size_t n, uint64_t seed)
{
    uint64_t s = seed;
    float lp = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        s = s * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        const float white = (float)(s >> 40) * (2.0f / 16777216.0f) - 1.0f;
        lp += 0.2f * (white - lp);
        pcm[i] = (int16_t)lroundf(lp * 9000.0f);
    }
}

typedef struct {
    const int16_t *flow;
    size_t flow_len;
    const int16_t *swish;
    size_t swish_len;
} TestLoops;

static void start_brush_audio(AudioState *a, const TestLoops *loops)
{
    audio_init(a, UINT64_C(21));
    audio_set_volumes(a, 100, 0, 100);
    if (loops != NULL) {
        audio_set_brush_samples(a, loops->flow, loops->flow_len, loops->swish, loops->swish_len);
    }
}

static void paint_held_stroke(AudioState *a, int32_t style, bool dry, bool surge, int16_t *buf,
                              size_t n)
{
    a->brush_target = 1.0f;
    a->brush_env = 1.0f;
    a->brush_active = true;
    a->brush_speed = 0.8f;
    a->brush_style = style;
    a->brush_dry = dry;
    a->brush_surge = surge;
    audio_render(a, buf, n);
}

static void render_brush_with(AudioState *a, const TestLoops *loops, int32_t style, bool dry,
                              bool surge, int16_t *buf, size_t n)
{
    start_brush_audio(a, loops);
    paint_held_stroke(a, style, dry, surge, buf, n);
}

static void test_audio_brush_loops_shape_the_stroke_and_fall_back_cleanly(void)
{
    enum {
        N = AUDIO_SAMPLES_PER_TICK * 6,
        VARIANTS = 5,
        FLOW = AUDIO_SAMPLE_RATE * 3 / 2,
        PAD = AUDIO_SAMPLE_RATE,
        SWISH = AUDIO_SAMPLE_RATE / 3
    };
    AudioState *a = calloc(1, sizeof *a);
    int16_t *flow = calloc(FLOW + PAD, sizeof *flow);
    int16_t *swish = calloc(SWISH, sizeof *swish);
    int16_t *silent = calloc(SWISH, sizeof *silent);
    int16_t *bufs = calloc((size_t)N * VARIANTS, sizeof *bufs);
    int16_t *other = calloc(N, sizeof *other);
    int16_t *procedural = calloc(N, sizeof *procedural);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(flow);
    TEST_ASSERT_NOT_NULL(swish);
    TEST_ASSERT_NOT_NULL(silent);
    TEST_ASSERT_NOT_NULL(bufs);
    TEST_ASSERT_NOT_NULL(other);
    TEST_ASSERT_NOT_NULL(procedural);
    fill_test_loop(flow, FLOW, UINT64_C(0x51));
    fill_test_loop(swish, SWISH, UINT64_C(0x52));
    const TestLoops padded = {flow, FLOW + PAD, swish, SWISH};

    render_brush_with(a, &padded, BRUSH_STYLE_HEAVY, false, false, bufs, N);
    render_brush_with(a, &padded, BRUSH_STYLE_EMBER, false, false, bufs + N, N);
    render_brush_with(a, &padded, BRUSH_STYLE_QUILL, false, false, bufs + 2 * N, N);
    render_brush_with(a, &padded, BRUSH_STYLE_HEAVY, true, false, bufs + 3 * N, N);
    render_brush_with(a, &padded, BRUSH_STYLE_HEAVY, false, true, bufs + 4 * N, N);
    TEST_ASSERT_TRUE(audio_validate_finite(a));
    for (int i = 0; i < VARIANTS; ++i) {
        TEST_ASSERT_TRUE(peak_abs(bufs + i * N, N) > 200);
        for (int j = i + 1; j < VARIANTS; ++j) {
            TEST_ASSERT_TRUE(memcmp(bufs + i * N, bufs + j * N, N * sizeof *bufs) != 0);
        }
    }

    render_brush_with(a, &padded, BRUSH_STYLE_HEAVY, false, false, other, N);
    TEST_ASSERT_EQUAL_MEMORY(bufs, other, N * sizeof *other);
    const TestLoops trimmed = {flow, FLOW, swish, SWISH};
    render_brush_with(a, &trimmed, BRUSH_STYLE_HEAVY, false, false, other, N);
    TEST_ASSERT_EQUAL_MEMORY(bufs, other, N * sizeof *other);

    render_brush_with(a, NULL, BRUSH_STYLE_HEAVY, false, false, procedural, N);
    TEST_ASSERT_TRUE(peak_abs(procedural, N) > 200);
    TEST_ASSERT_TRUE(memcmp(bufs, procedural, N * sizeof *procedural) != 0);
    const TestLoops all_silent = {silent, SWISH, silent, SWISH};
    render_brush_with(a, &all_silent, BRUSH_STYLE_HEAVY, false, false, other, N);
    TEST_ASSERT_EQUAL_MEMORY(procedural, other, N * sizeof *other);
    start_brush_audio(a, &trimmed);
    TEST_ASSERT_NOT_NULL(a->brush_flow_pcm);
    audio_set_brush_samples(a, NULL, 0u, NULL, 0u);
    paint_held_stroke(a, BRUSH_STYLE_HEAVY, false, false, other, N);
    TEST_ASSERT_NULL(a->brush_flow_pcm);
    TEST_ASSERT_NULL(a->brush_swish_pcm);
    TEST_ASSERT_EQUAL_MEMORY(procedural, other, N * sizeof *other);

    free(procedural);
    free(other);
    free(bufs);
    free(silent);
    free(swish);
    free(flow);
    free(a);
}

static void test_audio_skill_voices_blend_attached_loops(void)
{
    enum { N = AUDIO_SAMPLE_RATE, FLOW = AUDIO_SAMPLE_RATE * 3 / 2, SWISH = AUDIO_SAMPLE_RATE / 3 };
    static const GameEventType k_skills[2] = {EV_CYCLONE, EV_GALE};
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *flow = calloc(FLOW, sizeof *flow);
    int16_t *swish = calloc(SWISH, sizeof *swish);
    int16_t *loaded = calloc(N, sizeof *loaded);
    int16_t *procedural = calloc(N, sizeof *procedural);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(flow);
    TEST_ASSERT_NOT_NULL(swish);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_NOT_NULL(procedural);
    fill_test_loop(flow, FLOW, UINT64_C(0x61));
    fill_test_loop(swish, SWISH, UINT64_C(0x62));
    for (int k = 0; k < 2; ++k) {
        render_skill(a, g, k_skills[k], false, 0.0f, procedural, N);
        audio_init(a, UINT64_C(6));
        audio_set_volumes(a, 100, 0, 100);
        audio_set_brush_samples(a, flow, FLOW, swish, SWISH);
        one_event(g, k_skills[k]);
        audio_consume_events(a, g);
        audio_render(a, loaded, N);
        const int32_t peak = peak_abs(loaded, N);
        TEST_ASSERT_TRUE(peak >= 1000 && peak <= 32767);
        TEST_ASSERT_TRUE(memcmp(loaded, procedural, N * sizeof *loaded) != 0);
        TEST_ASSERT_TRUE(audio_validate_finite(a));
    }
    free(procedural);
    free(loaded);
    free(swish);
    free(flow);
    free(a);
    game_destroy(g);
}

static void fill_square_pcm(int16_t *pcm, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        pcm[i] = (i / 2u) % 2u == 0u ? INT16_MIN : INT16_MAX;
    }
}

static void test_audio_pcm_read_stays_between_its_central_samples(void)
{
    static const int16_t k_edge[4] = {INT16_MIN, INT16_MAX, INT16_MAX, INT16_MIN};
    TEST_ASSERT_TRUE(fabsf(audio_pcm_read(k_edge, 4u, 1.5f, 0.0f)) <= 1.0f);

    enum { N = 4096 };
    int16_t *pcm = calloc(N, sizeof *pcm);
    TEST_ASSERT_NOT_NULL(pcm);
    fill_test_loop(pcm, N, UINT64_C(0x71));
    for (int i = 1; i < N - 3; ++i) {
        pcm[i] = (int16_t)(i % 7 == 0 ? INT16_MAX : (i % 11 == 0 ? INT16_MIN : pcm[i]));
    }
    for (int k = 0; k < 4 * (N - 4); ++k) {
        const float pos = 1.0f + (float)k * 0.25f + 0.125f;
        const int32_t i = (int32_t)floorf(pos);
        const float lo = fminf((float)pcm[i], (float)pcm[i + 1]) / 32768.0f;
        const float hi = fmaxf((float)pcm[i], (float)pcm[i + 1]) / 32768.0f;
        const float v = audio_pcm_read(pcm, N, pos, 0.0f);
        TEST_ASSERT_TRUE(v >= lo - 1e-6f && v <= hi + 1e-6f);
    }
    free(pcm);
}

static void test_audio_grain_setup_bounds_huge_and_invalid_windows(void)
{
    AudioGrainLoop g;
    audio_grain_setup(&g, (size_t)UINT32_MAX, 0.0f, 1.0f, 1024.0f);
    TEST_ASSERT_EQUAL_UINT32(0u, g.start);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, g.span);
    TEST_ASSERT_EQUAL_FLOAT(1024.0f, g.grain_len);

    audio_grain_setup(&g, 4096u, 0.25f, 0.75f, 512.0f);
    TEST_ASSERT_EQUAL_UINT32(1024u, g.start);
    TEST_ASSERT_EQUAL_UINT32(2048u, g.span);

    static const float k_bad[] = {NAN, INFINITY, -INFINITY};
    for (size_t b = 0; b < sizeof k_bad / sizeof k_bad[0]; ++b) {
        audio_grain_setup(&g, 4096u, k_bad[b], 0.9f, 512.0f);
        TEST_ASSERT_EQUAL_UINT32(0u, g.span);
        audio_grain_setup(&g, 4096u, 0.1f, k_bad[b], 512.0f);
        TEST_ASSERT_EQUAL_UINT32(0u, g.span);
        audio_grain_setup(&g, 4096u, 0.1f, 0.9f, k_bad[b]);
        TEST_ASSERT_EQUAL_UINT32(0u, g.span);
    }
    audio_grain_setup(&g, 4096u, 0.1f, 0.9f, 4.0f);
    TEST_ASSERT_EQUAL_UINT32(0u, g.span);
}

static void test_audio_grain_step_survives_non_finite_and_out_of_range_state(void)
{
    enum { N = 8192, STEPS = 6000 };
    int16_t *pcm = calloc(N, sizeof *pcm);
    TEST_ASSERT_NOT_NULL(pcm);
    fill_square_pcm(pcm, N);

    typedef struct {
        float rate;
        float ph;
        float base;
        float grain_len;
    } GrainCase;
    static const GrainCase k_cases[] = {
        {NAN, 0.0f, 0.0f, 0.0f}, {INFINITY, 0.0f, 0.0f, 0.0f}, {-3.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, NAN, 0.0f, 0.0f}, {1.0f, -0.4f, 0.0f, 0.0f},    {1.0f, 1e30f, 0.0f, 0.0f},
        {1.0f, 0.0f, NAN, 0.0f}, {1.0f, 0.0f, -500.0f, 0.0f},  {1.0f, 0.0f, 1e30f, 0.0f},
        {1.0f, 0.0f, 0.0f, NAN}, {1.0f, 0.0f, 0.0f, 1e30f},
    };
    for (size_t c = 0; c < sizeof k_cases / sizeof k_cases[0]; ++c) {
        const GrainCase gc = k_cases[c];
        AudioGrainLoop g;
        audio_grain_setup(&g, N, 0.1f, 0.9f, 1024.0f);
        g.ph[0] = gc.ph;
        g.base[1] = gc.base;
        if (gc.grain_len != 0.0f) {
            g.grain_len = gc.grain_len;
        }
        uint64_t rng = UINT64_C(9);
        for (int i = 0; i < STEPS; ++i) {
            const float v = audio_grain_step(&g, pcm, N, gc.rate, &rng);
            /* The two half-offset Hann windows sum to 1 only up to float rounding. */
            TEST_ASSERT_TRUE(isfinite(v) && fabsf(v) <= 1.001f);
        }
        if (gc.grain_len == 0.0f) {
            TEST_ASSERT_TRUE(audio_grain_finite(&g));
            TEST_ASSERT_TRUE(g.ph[0] >= 0.0f && g.ph[0] < 1.0f);
            TEST_ASSERT_TRUE(g.base[1] >= 0.0f && g.base[1] <= (float)g.span);
        }
    }

    AudioGrainLoop past_end;
    audio_grain_setup(&past_end, N, 0.1f, 0.9f, 1024.0f);
    uint64_t rng = UINT64_C(9);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, audio_grain_step(&past_end, pcm, N / 2u, 1.0f, &rng));
    past_end.start = UINT32_MAX - 32u;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, audio_grain_step(&past_end, pcm, N, 1.0f, &rng));
    free(pcm);
}

static void queue_one_stroke_and_one_voice(AudioState *a, GameState *g)
{
    audio_init(a, UINT64_C(5));
    g->player.painting = true;
    g->player.stroke_speed = 600.0f;
    g->event_count = 0;
    audio_consume_events(a, g);
    one_event(g, EV_CYCLONE);
    audio_consume_events(a, g);
    TEST_ASSERT_TRUE(a->state_q_count > 0);
    TEST_ASSERT_TRUE(a->pending_count > 0);
    TEST_ASSERT_TRUE(audio_validate_finite(a));
}

static void test_audio_validator_rejects_non_finite_queued_ticks_and_voices(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    for (int field = 0; field < 5; ++field) {
        queue_one_stroke_and_one_voice(a, g);
        AudioTickState *tick = &a->state_q[a->state_q_head];
        AudioPendingVoice *voice = &a->pending[a->pending_head];
        float *const fields[5] = {&tick->brush_target, &tick->brush_speed, &tick->wind_target,
                                  &voice->freq, &voice->param};
        *fields[field] = NAN;
        TEST_ASSERT_FALSE(audio_validate_finite(a));
    }

    queue_one_stroke_and_one_voice(a, g);
    const int32_t free_tick = (a->state_q_head + a->state_q_count) % AUDIO_STATE_QUEUE_CAP;
    const int32_t free_voice = (a->pending_head + a->pending_count) % AUDIO_PENDING_VOICE_CAP;
    a->state_q[free_tick].brush_speed = NAN;
    a->pending[free_voice].freq = NAN;
    TEST_ASSERT_TRUE(audio_validate_finite(a));
    free(a);
    game_destroy(g);
}

static void render_after_quick_pause(AudioState *a, GameState *g, bool painted, bool pause,
                                     int16_t *buf, size_t n)
{
    enum { QUEUED_TICKS = 4 };
    audio_init(a, UINT64_C(31));
    audio_set_volumes(a, 100, 0, 100);
    audio_set_lobby_mode(a, true);
    g->config.brush_style = BRUSH_STYLE_HEAVY;
    g->event_count = 0;
    g->player.painting = painted;
    g->player.stroke_speed = painted ? 600.0f : 0.0f;
    for (int t = 0; t < QUEUED_TICKS; ++t) {
        audio_consume_lobby_events(a, g);
    }
    if (pause) {
        audio_set_paused(a, true);
        audio_set_paused(a, false);
    }
    g->player.painting = false;
    g->player.stroke_speed = 0.0f;
    audio_consume_lobby_events(a, g);
    audio_render(a, buf, n);
}

static void test_audio_quick_pause_and_resume_never_replays_a_queued_stroke(void)
{
    enum { N = AUDIO_SAMPLES_PER_TICK * 12 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(N, sizeof *buf);
    int16_t *idle = calloc(N, sizeof *idle);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_NOT_NULL(idle);

    render_after_quick_pause(a, g, true, false, buf, N);
    TEST_ASSERT_TRUE(peak_abs(buf, N) > 200);

    render_after_quick_pause(a, g, false, true, idle, N);
    render_after_quick_pause(a, g, true, true, buf, N);
    TEST_ASSERT_FALSE(a->brush_active);
    TEST_ASSERT_EQUAL_MEMORY(idle, buf, N * sizeof *buf);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    free(idle);
    free(buf);
    free(a);
    game_destroy(g);
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
    RUN_TEST(test_audio_batched_same_kind_voices_match_per_tick);
    RUN_TEST(test_audio_scene_change_drops_unplayed_combat_voice_but_keeps_ui);
    RUN_TEST(test_audio_skill_voices_reflect_dry_and_surge);
    RUN_TEST(test_audio_brush_rides_through_a_still_tick_while_held);
    RUN_TEST(test_audio_brush_releases_from_the_first_sample_after_a_lift);
    RUN_TEST(test_audio_fast_stroke_from_silence_reaches_full_level_by_its_second_tick);
    RUN_TEST(test_audio_brush_loops_shape_the_stroke_and_fall_back_cleanly);
    RUN_TEST(test_audio_skill_voices_blend_attached_loops);
    RUN_TEST(test_audio_pcm_read_stays_between_its_central_samples);
    RUN_TEST(test_audio_grain_setup_bounds_huge_and_invalid_windows);
    RUN_TEST(test_audio_grain_step_survives_non_finite_and_out_of_range_state);
    RUN_TEST(test_audio_validator_rejects_non_finite_queued_ticks_and_voices);
    RUN_TEST(test_audio_quick_pause_and_resume_never_replays_a_queued_stroke);
    return UNITY_END();
}
