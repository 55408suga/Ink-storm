#include "audio.h"
#include "game.h"
#include "synth_voices.h"
#include "unity.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int count_active_kind(const AudioState *a, AudioVoiceKind kind)
{
    int n = 0;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        n += a->voices[i].active && a->voices[i].kind == kind ? 1 : 0;
    }
    return n;
}

static const AudioVoice *newest_voice(const AudioState *a)
{
    const AudioVoice *best = NULL;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        const AudioVoice *v = &a->voices[i];
        if (v->active && (best == NULL || v->birth_seq > best->birth_seq)) {
            best = v;
        }
    }
    return best;
}

static void render_silence(AudioState *a, size_t n)
{
    int16_t buf[1024];
    while (n > 0) {
        const size_t chunk = n < 1024u ? n : 1024u;
        audio_render(a, buf, chunk);
        n -= chunk;
    }
}

/* Spawns one voice now and reports whether a slot was actually taken. */
static bool spawn_now(AudioState *a, AudioVoiceKind kind, float freq)
{
    const uint64_t seq = a->voice_seq;
    audio_voice_spawn(a, kind, a->samples_rendered, freq, 0.0f);
    return a->voice_seq != seq;
}

static void test_audio_thunderburst_retriggers_no_faster_than_024s_and_reactions_cap_at_two(void)
{
    enum { GAP = AUDIO_SAMPLE_RATE * 24 / 100 };
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(31));

    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_FALSE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    }
    render_silence(a, GAP - 1);
    TEST_ASSERT_FALSE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    render_silence(a, 1);
    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));

    for (int i = 0; i < 12; ++i) {
        render_silence(a, GAP);
        TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
        TEST_ASSERT_TRUE(count_active_kind(a, AUDIO_VOICE_THUNDERBURST) <= 2);
    }
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    /* Clearing the retrigger gaps isolates the per-kind cap. */
    audio_init(a, UINT64_C(31));
    for (int i = 0; i < 5; ++i) {
        a->thunderburst_next_sample = 0;
        a->storm_zap_next_sample = 0;
        TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
        TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_STORM_ZAP, 1320.0f));
    }
    TEST_ASSERT_EQUAL_INT(2, count_active_kind(a, AUDIO_VOICE_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT(2, count_active_kind(a, AUDIO_VOICE_STORM_ZAP));
    free(a);
}

static void test_audio_thunderburst_chain_softens_then_recovers_after_075s_quiet(void)
{
    enum { GAP = AUDIO_SAMPLE_RATE * 24 / 100, CHAIN = AUDIO_SAMPLE_RATE * 75 / 100 };
    static const float k_chain_gain[] = {1.00f, 0.87f, 0.74f, 0.61f, 0.48f, 0.48f, 0.48f};
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(32));

    for (size_t i = 0; i < sizeof k_chain_gain / sizeof k_chain_gain[0]; ++i) {
        if (i > 0) {
            render_silence(a, GAP);
        }
        TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
        const AudioVoice *v = newest_voice(a);
        TEST_ASSERT_NOT_NULL(v);
        TEST_ASSERT_EQUAL_INT(AUDIO_VOICE_THUNDERBURST, v->kind);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, k_chain_gain[i], v->param);
    }

    render_silence(a, CHAIN - 1);
    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.48f, newest_voice(a)->param);

    render_silence(a, CHAIN);
    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, newest_voice(a)->param);
    free(a);
}

static void test_audio_thunderburst_never_steals_skill_or_milestone_voices(void)
{
    static const AudioVoiceKind k_held[] = {
        AUDIO_VOICE_WAVE_CLEAR,    AUDIO_VOICE_SHRINE_GONG, AUDIO_VOICE_CATACLYSM,
        AUDIO_VOICE_CYCLONE_SWIRL, AUDIO_VOICE_GALE_SLASH,  AUDIO_VOICE_SURGE_KOTO,
    };
    enum { HELD = sizeof k_held / sizeof k_held[0] };
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(33));
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_spawn(a, k_held[i % HELD], 0, 150.0f, 0.0f);
    }
    AudioVoiceKind before[AUDIO_MAX_VOICES];
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        TEST_ASSERT_TRUE(a->voices[i].active);
        before[i] = a->voices[i].kind;
    }

    TEST_ASSERT_FALSE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    TEST_ASSERT_EQUAL_UINT64(0, a->thunderburst_next_sample);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        TEST_ASSERT_TRUE(a->voices[i].active);
        TEST_ASSERT_EQUAL_INT(before[i], a->voices[i].kind);
    }
    free(a);
}

static void test_audio_storm_zap_hops_in_one_tick_share_one_voice_until_012s(void)
{
    enum { GAP = AUDIO_SAMPLE_RATE * 12 / 100, HOPS = 4 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t buf[AUDIO_SAMPLES_PER_TICK];
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(34));

    g->phase = RUN_PHASE_PLAYING;
    g->event_count = HOPS;
    for (int i = 0; i < HOPS; ++i) {
        memset(&g->events[i], 0, sizeof g->events[i]);
        g->events[i].type = EV_STORM_ZAP;
    }
    const uint64_t seq = a->voice_seq;
    audio_consume_events(a, g);
    audio_render(a, buf, AUDIO_SAMPLES_PER_TICK);
    TEST_ASSERT_EQUAL_UINT64(seq + 1u, a->voice_seq);
    TEST_ASSERT_EQUAL_INT(1, count_active_kind(a, AUDIO_VOICE_STORM_ZAP));

    render_silence(a, GAP - AUDIO_SAMPLES_PER_TICK - 1);
    TEST_ASSERT_FALSE(spawn_now(a, AUDIO_VOICE_STORM_ZAP, 1320.0f));
    render_silence(a, 1);
    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_STORM_ZAP, 1320.0f));
    TEST_ASSERT_TRUE(audio_validate_finite(a));
    free(a);
    game_destroy(g);
}

static int32_t render_peak(AudioState *a, int16_t *buf, size_t n)
{
    audio_render(a, buf, n);
    return peak_abs(buf, n);
}

static void test_audio_chained_thunderburst_renders_quieter_than_a_fresh_one(void)
{
    enum { GAP = AUDIO_SAMPLE_RATE * 24 / 100, CHAIN_STEPS = 4 };
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(GAP, sizeof *buf);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);
    audio_init(a, UINT64_C(36));
    audio_set_volumes(a, 100, 0, 100);

    TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
    const int32_t fresh_peak = render_peak(a, buf, GAP);
    int32_t chained_peak = 0;
    for (int i = 0; i < CHAIN_STEPS; ++i) {
        TEST_ASSERT_TRUE(spawn_now(a, AUDIO_VOICE_THUNDERBURST, 62.0f));
        chained_peak = render_peak(a, buf, GAP);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.48f, newest_voice(a)->param);
    TEST_ASSERT_TRUE(fresh_peak >= 1000);
    TEST_ASSERT_TRUE_MESSAGE(chained_peak * 4 < fresh_peak * 3,
                             "chained Thunderburst should render audibly quieter");
    free(buf);
    free(a);
}

static void test_audio_batched_storm_zap_flood_still_queues_a_wave_clear(void)
{
    enum { TICKS = 12 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    audio_init(a, UINT64_C(37));

    g->phase = RUN_PHASE_PLAYING;
    for (int t = 0; t < TICKS; ++t) {
        g->event_count = GAME_MAX_EVENTS;
        for (int i = 0; i < GAME_MAX_EVENTS; ++i) {
            memset(&g->events[i], 0, sizeof g->events[i]);
            g->events[i].type = EV_STORM_ZAP;
        }
        if (t == TICKS - 1) {
            g->events[GAME_MAX_EVENTS - 1].type = EV_WAVE_CLEAR;
        }
        audio_consume_events(a, g);
    }

    bool wave_clear_queued = false;
    for (int32_t i = 0; i < a->pending_count; ++i) {
        const AudioPendingVoice *p = &a->pending[(a->pending_head + i) % AUDIO_PENDING_VOICE_CAP];
        wave_clear_queued = wave_clear_queued || p->kind == AUDIO_VOICE_WAVE_CLEAR;
    }
    TEST_ASSERT_TRUE(wave_clear_queued);
    TEST_ASSERT_TRUE(a->pending_count <= TICKS);

    render_silence(a, (size_t)AUDIO_SAMPLES_PER_TICK * TICKS);
    TEST_ASSERT_EQUAL_INT(1, count_active_kind(a, AUDIO_VOICE_WAVE_CLEAR));
    free(a);
    game_destroy(g);
}

static void setup_reaction_storm_tick(GameState *g, int tick)
{
    g->phase = RUN_PHASE_PLAYING;
    g->run.wave_index = 0;
    g->event_count = 0;
    static const GameEventType seq[] = {EV_THUNDERBURST, EV_STORM_ZAP, EV_STORM_ZAP,
                                        EV_THUNDERBURST, EV_STORM_ZAP};
    const int n = 1 + tick % 5;
    for (int i = 0; i < n; ++i) {
        memset(&g->events[i], 0, sizeof g->events[i]);
        g->events[i].type = seq[(tick + i) % 5];
        g->event_count++;
    }
}

static void test_audio_reaction_limits_match_per_tick_and_batched_and_reset_in_lobby(void)
{
    enum { TICKS = 72, BATCH = 6, TOTAL = AUDIO_SAMPLES_PER_TICK * TICKS };
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

    audio_init(a, UINT64_C(35));
    audio_set_volumes(a, 100, 0, 100);
    for (int t = 0; t < TICKS; ++t) {
        setup_reaction_storm_tick(g, t);
        audio_consume_events(a, g);
        audio_render(a, out_a + (size_t)t * AUDIO_SAMPLES_PER_TICK, AUDIO_SAMPLES_PER_TICK);
    }

    audio_init(b, UINT64_C(35));
    audio_set_volumes(b, 100, 0, 100);
    for (int t = 0; t < TICKS; t += BATCH) {
        for (int k = 0; k < BATCH; ++k) {
            setup_reaction_storm_tick(g, t + k);
            audio_consume_events(b, g);
        }
        audio_render(b, out_b + (size_t)t * AUDIO_SAMPLES_PER_TICK,
                     (size_t)AUDIO_SAMPLES_PER_TICK * BATCH);
    }
    TEST_ASSERT_TRUE(peak_abs(out_a, TOTAL) >= 1000);
    TEST_ASSERT_EQUAL_MEMORY(out_a, out_b, TOTAL * sizeof *out_a);
    TEST_ASSERT_EQUAL_UINT64(a->voice_seq, b->voice_seq);

    (void)spawn_now(b, AUDIO_VOICE_THUNDERBURST, 62.0f);
    (void)spawn_now(b, AUDIO_VOICE_STORM_ZAP, 1320.0f);
    TEST_ASSERT_TRUE(b->thunderburst_next_sample > b->samples_rendered);
    TEST_ASSERT_TRUE(b->storm_zap_next_sample > b->samples_rendered);
    audio_set_lobby_mode(b, true);
    TEST_ASSERT_EQUAL_UINT64(0, b->thunderburst_next_sample);
    TEST_ASSERT_EQUAL_UINT64(0, b->thunderburst_chain_until);
    TEST_ASSERT_EQUAL_UINT64(0, b->storm_zap_next_sample);
    TEST_ASSERT_EQUAL_UINT8(0, b->thunderburst_chain);
    TEST_ASSERT_TRUE(spawn_now(b, AUDIO_VOICE_THUNDERBURST, 62.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, newest_voice(b)->param);
    TEST_ASSERT_TRUE(spawn_now(b, AUDIO_VOICE_STORM_ZAP, 1320.0f));
    TEST_ASSERT_TRUE(audio_validate_finite(b));

    free(out_b);
    free(out_a);
    free(b);
    free(a);
    game_destroy(g);
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

enum { ONSET_WINDOW = AUDIO_SAMPLE_RATE / 200, SWISH_LEAD_IN = AUDIO_SAMPLE_RATE * 45 / 1000 };

static double loudest_window_rms(const int16_t *buf, size_t from, size_t to)
{
    double loudest = 0.0;
    for (size_t at = from; at + ONSET_WINDOW <= to; at += ONSET_WINDOW / 5u) {
        const double r = rms_of(buf + at, ONSET_WINDOW);
        loudest = r > loudest ? r : loudest;
    }
    return loudest;
}

static void render_cast(AudioState *a, GameState *g, const TestLoops *loops, GameEventType type,
                        int32_t style, int16_t *buf, size_t n)
{
    start_brush_audio(a, loops);
    g->config.brush_style = (BrushStyleId)style;
    g->player.dry = false;
    g->player.surge_t = 0.0f;
    one_event(g, type);
    audio_consume_events(a, g);
    audio_render(a, buf, n);
}

static void test_audio_gale_and_cyclone_reach_full_voice_within_20ms_past_a_swish_lead_in(void)
{
    enum {
        N = AUDIO_SAMPLE_RATE / 2,
        EARLY = AUDIO_SAMPLE_RATE / 50,
        PRE_ROLL = AUDIO_SAMPLE_RATE * 3 / 1000,
        FLOW = AUDIO_SAMPLE_RATE * 3 / 2,
        SWISH = AUDIO_SAMPLE_RATE / 3
    };
    static const GameEventType k_skills[2] = {EV_GALE, EV_CYCLONE};
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *flow = calloc(FLOW, sizeof *flow);
    int16_t *swish = calloc(SWISH, sizeof *swish);
    int16_t *buf = calloc(N, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(flow);
    TEST_ASSERT_NOT_NULL(swish);
    TEST_ASSERT_NOT_NULL(buf);
    fill_test_loop(flow, FLOW, UINT64_C(0x71));
    fill_test_loop(swish, SWISH, UINT64_C(0x72));
    /* At the plain loop level the swish hides under the pink band, so a late swish would pass. */
    for (size_t i = 0; i < SWISH; ++i) {
        swish[i] = (int16_t)(i < SWISH_LEAD_IN ? swish[i] / 40 : swish[i] * 3);
    }
    const TestLoops loops = {flow, FLOW, swish, SWISH};

    start_brush_audio(a, &loops);
    TEST_ASSERT_NOT_NULL(a->brush_swish_pcm);
    const size_t attack = SWISH_LEAD_IN - (size_t)(a->brush_swish_pcm - swish);
    TEST_ASSERT_TRUE(a->brush_swish_onset + PRE_ROLL >= attack);
    TEST_ASSERT_TRUE(a->brush_swish_onset < attack);
    audio_set_brush_samples(a, NULL, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL_size_t(0u, a->brush_swish_onset);

    for (int k = 0; k < 2; ++k) {
        for (int32_t style = 0; style < BRUSH_STYLE_COUNT; ++style) {
            for (int with_loops = 0; with_loops < 2; ++with_loops) {
                render_cast(a, g, with_loops ? &loops : NULL, k_skills[k], style, buf, N);
                const double early = loudest_window_rms(buf, 0u, EARLY);
                const double loudest = loudest_window_rms(buf, 0u, N);
                TEST_ASSERT_TRUE(loudest > 0.0);
                TEST_ASSERT_TRUE_MESSAGE(early >= 0.9 * loudest, "skill voice slow to speak");
                const int32_t peak = peak_abs(buf, N);
                /* At full 100/100 gain the voice stays clear of hard saturation; at default 80/90
                 * game gain (~0.72x) this leaves ample mixer headroom for external BGM. */
                TEST_ASSERT_TRUE(peak >= 1000 && peak <= 29000);
                TEST_ASSERT_TRUE(audio_validate_finite(a));
            }
        }
    }
    free(buf);
    free(swish);
    free(flow);
    free(a);
    game_destroy(g);
}

static void test_audio_swish_onset_ignores_early_spike_and_late_loud_tail(void)
{
    enum {
        SWISH = AUDIO_SAMPLE_RATE / 3,
        LATE_FROM = AUDIO_SAMPLE_RATE * 220 / 1000,
        PRE_ROLL = AUDIO_SAMPLE_RATE * 3 / 1000,
        MIN_REMAINING = 1024
    };
    AudioState *a = calloc(1, sizeof *a);
    int16_t *swish = calloc(SWISH, sizeof *swish);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(swish);

    /* Case 1: isolated full-scale click inside quiet lead-in must not anchor onset at 0. */
    fill_test_loop(swish, SWISH, UINT64_C(0x91));
    for (size_t i = 0; i < SWISH; ++i) {
        swish[i] = (int16_t)(i < SWISH_LEAD_IN ? swish[i] / 40 : swish[i] * 3);
    }
    swish[10] = INT16_MAX;
    audio_init(a, UINT64_C(1));
    audio_set_brush_samples(a, NULL, 0u, swish, SWISH);
    TEST_ASSERT_NOT_NULL(a->brush_swish_pcm);
    const size_t attack1 = SWISH_LEAD_IN - (size_t)(a->brush_swish_pcm - swish);
    TEST_ASSERT_TRUE(a->brush_swish_onset + PRE_ROLL >= attack1);
    TEST_ASSERT_TRUE(a->brush_swish_onset < attack1);

    /* Case 2: louder late tail must not pull onset past the useful opening body. */
    fill_test_loop(swish, SWISH, UINT64_C(0x92));
    for (size_t i = 0; i < SWISH; ++i) {
        if (i < SWISH_LEAD_IN) {
            swish[i] = (int16_t)(swish[i] / 50);
        } else if (i < LATE_FROM) {
            swish[i] = (int16_t)(swish[i] / 2);
        } else {
            const int32_t boosted = (int32_t)swish[i] * 6;
            swish[i] =
                (int16_t)(boosted > INT16_MAX ? INT16_MAX
                                              : (boosted < -INT16_MAX ? -INT16_MAX : boosted));
        }
    }
    audio_init(a, UINT64_C(2));
    audio_set_brush_samples(a, NULL, 0u, swish, SWISH);
    TEST_ASSERT_NOT_NULL(a->brush_swish_pcm);
    const size_t attack2 = SWISH_LEAD_IN - (size_t)(a->brush_swish_pcm - swish);
    TEST_ASSERT_TRUE(a->brush_swish_onset + PRE_ROLL >= attack2);
    TEST_ASSERT_TRUE(a->brush_swish_onset < attack2 + 128u);
    TEST_ASSERT_TRUE(a->brush_swish_len - a->brush_swish_onset >= (size_t)MIN_REMAINING);

    /* Case 3: isolated full-scale click at index 0 over a quiet (+-256) body. */
    enum { QUIET_N = 4096, QUIET_LEAD = 1500 };
    int16_t *quiet = calloc(QUIET_N, sizeof *quiet);
    TEST_ASSERT_NOT_NULL(quiet);
    quiet[0] = INT16_MAX;
    for (size_t i = QUIET_LEAD; i < QUIET_N; ++i) {
        quiet[i] = (i & 1u) == 0u ? (int16_t)256 : (int16_t)-256;
    }
    audio_init(a, UINT64_C(3));
    audio_set_brush_samples(a, NULL, 0u, quiet, QUIET_N);
    TEST_ASSERT_NOT_NULL(a->brush_swish_pcm);
    TEST_ASSERT_TRUE(a->brush_swish_onset + PRE_ROLL >= (size_t)QUIET_LEAD);
    TEST_ASSERT_TRUE(a->brush_swish_onset < (size_t)QUIET_LEAD);

    free(quiet);
    free(swish);
    free(a);
}

static void test_audio_realtime_mode_caps_queued_lead_at_two_ticks_offline_keeps_full_batch(void)
{
    enum { TICKS = 10, AHEAD = 100 };
    const uint64_t realtime_cap = 2u * (uint64_t)AUDIO_SAMPLES_PER_TICK;
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t buf[AHEAD];
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    for (int realtime = 0; realtime < 2; ++realtime) {
        audio_init(a, UINT64_C(8));
        audio_set_realtime_mode(a, realtime != 0);
        audio_render(a, buf, AHEAD);
        for (int t = 0; t < TICKS; ++t) {
            one_event(g, EV_KILL);
            audio_consume_events(a, g);
        }
        const uint64_t now = a->samples_rendered;
        TEST_ASSERT_EQUAL_INT32(TICKS, a->state_q_count);
        TEST_ASSERT_EQUAL_INT32(TICKS - 1, a->pending_count);
        uint64_t prev_at = 0u;
        for (int t = 0; t < TICKS; ++t) {
            const uint64_t at = a->state_q[(a->state_q_head + t) % AUDIO_STATE_QUEUE_CAP].at_sample;
            if (realtime) {
                TEST_ASSERT_TRUE(at >= now && at <= now + realtime_cap);
                if (t > 0) {
                    TEST_ASSERT_TRUE(at > prev_at);
                }
            } else {
                TEST_ASSERT_EQUAL_UINT64(now + (uint64_t)t * AUDIO_SAMPLES_PER_TICK, at);
            }
            prev_at = at;
        }
        for (int i = 0; i < a->pending_count; ++i) {
            const uint64_t start =
                a->pending[(a->pending_head + i) % AUDIO_PENDING_VOICE_CAP].start_sample;
            const uint64_t tick_at =
                a->state_q[(a->state_q_head + i + 1) % AUDIO_STATE_QUEUE_CAP].at_sample;
            if (realtime) {
                TEST_ASSERT_EQUAL_UINT64(tick_at, start);
            } else {
                TEST_ASSERT_EQUAL_UINT64(now + (uint64_t)(i + 1) * AUDIO_SAMPLES_PER_TICK, start);
            }
        }
        TEST_ASSERT_TRUE(audio_validate_finite(a));
    }
    free(a);
    game_destroy(g);
}

static void test_audio_realtime_catchup_preserves_brush_onset_at_unaligned_clock(void)
{
    enum { AHEAD = 100, BURST = 4, TAIL = AUDIO_SAMPLES_PER_TICK * 4 };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(TAIL, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);

    audio_init(a, UINT64_C(9));
    audio_set_volumes(a, 100, 0, 100);
    audio_set_external_bgm(a, true);
    audio_set_realtime_mode(a, true);
    audio_render(a, buf, AHEAD);

    for (int t = 0; t < BURST; ++t) {
        g->player.painting = (t == 0);
        g->player.stroke_speed = (t == 0) ? 700.0f : 0.0f;
        g->event_count = 0;
        if (t == 0) {
            one_event(g, EV_KILL);
        } else if (t == 1) {
            one_event(g, EV_GALE);
        }
        audio_consume_events(a, g);
    }

    TEST_ASSERT_EQUAL_INT32(BURST, a->state_q_count);
    const uint64_t t0 = a->state_q[a->state_q_head].at_sample;
    const uint64_t t1 = a->state_q[(a->state_q_head + 1) % AUDIO_STATE_QUEUE_CAP].at_sample;
    TEST_ASSERT_EQUAL_UINT64((uint64_t)AHEAD, t0);
    TEST_ASSERT_TRUE(t1 > t0);

    /* Render only up to tick 1: tick 0's brush onset must remain active and build envelope. */
    const size_t first_span = (size_t)(t1 - t0);
    audio_render(a, buf, first_span);
    TEST_ASSERT_TRUE(a->brush_active);
    TEST_ASSERT_TRUE(a->brush_env > 0.05f);

    audio_render(a, buf, TAIL);
    TEST_ASSERT_FALSE(a->brush_active);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    /* Recompression after a partial drain must never postpone an already scheduled cue. */
    audio_init(a, UINT64_C(12));
    audio_set_realtime_mode(a, true);
    audio_render(a, buf, AHEAD);
    for (int t = 0; t < 15; ++t) {
        g->player.painting = false;
        g->event_count = 0;
        if (t == 14) {
            one_event(g, EV_GALE);
        }
        audio_consume_events(a, g);
    }
    TEST_ASSERT_EQUAL_INT32(1, a->pending_count);
    const uint64_t gale_before = a->pending[a->pending_head].start_sample;
    TEST_ASSERT_EQUAL_UINT64((uint64_t)AHEAD + 2u * (uint64_t)AUDIO_SAMPLES_PER_TICK, gale_before);

    audio_render(a, buf, 512u);
    g->event_count = 0;
    audio_consume_events(a, g);
    TEST_ASSERT_EQUAL_INT32(1, a->pending_count);
    TEST_ASSERT_TRUE(a->pending[a->pending_head].start_sample <= gale_before);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    free(buf);
    free(a);
    game_destroy(g);
}

static void test_audio_realtime_stale_brush_releases_when_producer_stalls(void)
{
    enum {
        WARMUP = AUDIO_SAMPLES_PER_TICK * 3,
        STALL = AUDIO_SAMPLE_RATE / 2,
        TAIL_WIN = AUDIO_SAMPLE_RATE / 20
    };
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    int16_t *buf = calloc(STALL, sizeof *buf);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(buf);

    for (int realtime = 0; realtime < 2; ++realtime) {
        audio_init(a, UINT64_C(10));
        audio_set_volumes(a, 100, 0, 100);
        audio_set_external_bgm(a, true);
        audio_set_realtime_mode(a, realtime != 0);
        g->player.painting = true;
        g->player.stroke_speed = 700.0f;
        g->event_count = 0;
        audio_consume_events(a, g);

        audio_render(a, buf, WARMUP);
        TEST_ASSERT_TRUE(a->brush_active);
        TEST_ASSERT_TRUE(rms_of(buf, WARMUP) > 100.0);

        audio_render(a, buf, STALL);
        const double tail_rms = rms_of(buf + (STALL - TAIL_WIN), TAIL_WIN);
        if (realtime) {
            TEST_ASSERT_FALSE(a->brush_active);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, a->brush_target);
            TEST_ASSERT_TRUE(tail_rms < 25.0);
        } else {
            TEST_ASSERT_TRUE(a->brush_active);
            TEST_ASSERT_TRUE(tail_rms > 100.0);
        }
        TEST_ASSERT_TRUE(audio_validate_finite(a));
    }

    free(buf);
    free(a);
    game_destroy(g);
}

typedef struct {
    pthread_mutex_t mtx;
    AudioState *state;
    bool stop;
    size_t active_callbacks;
    size_t silent_callbacks;
    size_t active_nonzero_bytes;
    size_t silent_nonzero_bytes;
} AudioCallbackTestShared;

static void *audio_callback_test_worker(void *arg)
{
    static const size_t k_chunks[4] = {330u, 550u, 735u, 1024u};
    AudioCallbackTestShared *sh = (AudioCallbackTestShared *)arg;
    unsigned char raw[1u + 1024u * sizeof(int16_t)];
    int16_t scratch[512];
    size_t step = 0u;
    const struct timespec pause_ns = {0, 150000L};

    for (;;) {
        const size_t frames = k_chunks[step & 3u];
        const size_t total_bytes = frames * sizeof(int16_t);
        step++;
        unsigned char *dst = raw + 1u; /* Intentionally 1-byte misaligned like raw byte storage. */
        size_t remaining = frames;

        (void)pthread_mutex_lock(&sh->mtx);
        if (sh->stop) {
            (void)pthread_mutex_unlock(&sh->mtx);
            break;
        }
        const bool active = sh->state != NULL;
        while (remaining > 0u) {
            const size_t chunk = remaining < 512u ? remaining : 512u;
            if (active) {
                audio_render(sh->state, scratch, chunk);
                memcpy(dst, scratch, chunk * sizeof(int16_t));
            } else {
                memset(dst, 0, chunk * sizeof(int16_t));
            }
            dst += chunk * sizeof(int16_t);
            remaining -= chunk;
        }
        size_t nonzero = 0u;
        for (size_t b = 0u; b < total_bytes; ++b) {
            if (raw[1u + b] != 0u) {
                nonzero++;
            }
        }
        if (active) {
            sh->active_callbacks++;
            sh->active_nonzero_bytes += nonzero;
        } else {
            sh->silent_callbacks++;
            sh->silent_nonzero_bytes += nonzero;
        }
        (void)pthread_mutex_unlock(&sh->mtx);
        (void)nanosleep(&pause_ns, NULL);
    }
    return NULL;
}

static void test_audio_concurrent_realtime_callback_and_teardown(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);

    audio_init(a, UINT64_C(11));
    audio_set_realtime_mode(a, true);
    /* Verify per-track mask suppresses procedural lobby BGM even when external_bgm starts false. */
    audio_set_external_bgm(a, false);
    audio_set_external_bgm_mask(a, UINT32_C(1) << (unsigned)BGM_LOBBY);
    audio_set_lobby_mode(a, true);
    int16_t lobby_masked[512];
    int16_t lobby_ext[512];
    AudioState *ref = calloc(1, sizeof *ref);
    TEST_ASSERT_NOT_NULL(ref);
    audio_init(ref, UINT64_C(11));
    audio_set_realtime_mode(ref, true);
    audio_set_external_bgm(ref, true);
    audio_set_lobby_mode(ref, true);
    audio_render(a, lobby_masked, 512u);
    audio_render(ref, lobby_ext, 512u);
    TEST_ASSERT_EQUAL_MEMORY(lobby_ext, lobby_masked, sizeof lobby_ext);
    free(ref);

    audio_set_lobby_mode(a, false);

    AudioCallbackTestShared sh;
    memset(&sh, 0, sizeof sh);
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_init(&sh.mtx, NULL));
    sh.state = a;

    pthread_t worker;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&worker, NULL, audio_callback_test_worker, &sh));

    const struct timespec tick_ns = {0, 250000L};
    for (int i = 0; i < 40; ++i) {
        g->player.painting = (i % 4) < 2;
        g->player.stroke_speed = g->player.painting ? 650.0f : 0.0f;
        g->event_count = 0;
        if (i % 5 == 0) {
            one_event(g, EV_GALE);
        } else if (i % 7 == 0) {
            one_event(g, EV_CYCLONE);
        } else if (i % 3 == 0) {
            one_event(g, EV_KILL);
        }

        (void)pthread_mutex_lock(&sh.mtx);
        audio_consume_events(a, g);
        if (i == 15) {
            audio_set_paused(a, true);
        } else if (i == 20) {
            audio_set_paused(a, false);
        } else if (i == 25) {
            audio_set_volumes(a, 90, 80, 95);
            audio_set_external_bgm(a, true);
        }
        (void)pthread_mutex_unlock(&sh.mtx);
        (void)nanosleep(&tick_ns, NULL);
    }

    /* Detach state while callback thread is still active, mirroring audio_out_close(). */
    (void)pthread_mutex_lock(&sh.mtx);
    sh.state = NULL;
    (void)pthread_mutex_unlock(&sh.mtx);
    const struct timespec wait_ns = {0, 500000L};
    for (int tries = 0; tries < 400; ++tries) {
        (void)pthread_mutex_lock(&sh.mtx);
        const bool seen = sh.silent_callbacks > 0u;
        (void)pthread_mutex_unlock(&sh.mtx);
        if (seen) {
            break;
        }
        (void)nanosleep(&wait_ns, NULL);
    }

    (void)pthread_mutex_lock(&sh.mtx);
    sh.stop = true;
    (void)pthread_mutex_unlock(&sh.mtx);
    TEST_ASSERT_EQUAL_INT(0, pthread_join(worker, NULL));
    (void)pthread_mutex_destroy(&sh.mtx);

    TEST_ASSERT_TRUE(sh.active_callbacks > 0u);
    TEST_ASSERT_TRUE(sh.silent_callbacks > 0u);
    TEST_ASSERT_TRUE(sh.active_nonzero_bytes > 0u);
    TEST_ASSERT_EQUAL_size_t(0u, sh.silent_nonzero_bytes);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

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
    RUN_TEST(test_audio_thunderburst_retriggers_no_faster_than_024s_and_reactions_cap_at_two);
    RUN_TEST(test_audio_thunderburst_chain_softens_then_recovers_after_075s_quiet);
    RUN_TEST(test_audio_thunderburst_never_steals_skill_or_milestone_voices);
    RUN_TEST(test_audio_storm_zap_hops_in_one_tick_share_one_voice_until_012s);
    RUN_TEST(test_audio_reaction_limits_match_per_tick_and_batched_and_reset_in_lobby);
    RUN_TEST(test_audio_chained_thunderburst_renders_quieter_than_a_fresh_one);
    RUN_TEST(test_audio_batched_storm_zap_flood_still_queues_a_wave_clear);
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
    RUN_TEST(test_audio_gale_and_cyclone_reach_full_voice_within_20ms_past_a_swish_lead_in);
    RUN_TEST(test_audio_swish_onset_ignores_early_spike_and_late_loud_tail);
    RUN_TEST(test_audio_realtime_mode_caps_queued_lead_at_two_ticks_offline_keeps_full_batch);
    RUN_TEST(test_audio_realtime_catchup_preserves_brush_onset_at_unaligned_clock);
    RUN_TEST(test_audio_realtime_stale_brush_releases_when_producer_stalls);
    RUN_TEST(test_audio_concurrent_realtime_callback_and_teardown);
    RUN_TEST(test_audio_pcm_read_stays_between_its_central_samples);
    RUN_TEST(test_audio_grain_setup_bounds_huge_and_invalid_windows);
    RUN_TEST(test_audio_grain_step_survives_non_finite_and_out_of_range_state);
    RUN_TEST(test_audio_validator_rejects_non_finite_queued_ticks_and_voices);
    RUN_TEST(test_audio_quick_pause_and_resume_never_replays_a_queued_stroke);
    return UNITY_END();
}
