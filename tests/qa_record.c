#include "audio.h"
#include "bgm_fade.h"
#include "cli.h"
#include "core.h"
#include "fluid.h"
#include "game.h"
#include "paths.h"
#include "raylib.h"
#include "render.h"
#include "roguelike/relics.h"
#include "roguelike/upgrades.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define QA_MAX_FRAMES (INK_TICK_HZ * 60 * 60)
#define QA_SAMPLE_STRIDE 16
#define QA_BGM_SLOTS ((int)BGM_VICTORY + 1)
#define QA_WAV_HEADER_BYTES 44u
#define QA_LOBBY_SEGMENT_FRAMES 180
#define QA_ACT_TRANSITION_MAX_TICKS 160
#define QA_TWO_PI 6.28318530718f
/* Longest non-Thunderburst shake (Cataclysm, 0.34 s) plus the tick it was raised on. */
#define QA_CATACLYSM_SHAKE_WINDOW 0.36f
#define QA_OTHER_SHAKE_WINDOW 0.32f

static const uint64_t k_fnv_offset = UINT64_C(0xcbf29ce484222325);
static const uint64_t k_fnv_prime = UINT64_C(0x100000001b3);

typedef enum {
    QA_SCN_LOBBY_BRUSH = 0,
    QA_SCN_THUNDERBURST_STRESS,
    QA_SCN_ACT1_COMBAT,
    QA_SCN_BOSS_SHOWCASE,
    QA_SCN_COUNT
} QaScenario;

typedef struct {
    const char *name;
    int default_frames;
} QaScenarioDef;

static const QaScenarioDef k_scenarios[QA_SCN_COUNT] = {
    [QA_SCN_LOBBY_BRUSH] = {"lobby_brush", 540},
    [QA_SCN_THUNDERBURST_STRESS] = {"thunderburst_stress", 540},
    [QA_SCN_ACT1_COMBAT] = {"act1_combat", 600},
    [QA_SCN_BOSS_SHOWCASE] = {"boss_showcase", 720},
};

typedef struct {
    QaScenario scenario;
    int frames;
    const char *video_out;
    const char *audio_out;
    const char *report_out;
    bool bgm;
    bool post;
    bool no_render;
    bool list;
} QaOptions;

typedef struct {
    Wave waves[QA_BGM_SLOTS];
    bool loaded[QA_BGM_SLOTS];
    size_t pos[QA_BGM_SLOTS];
    bool loop[QA_BGM_SLOTS];
    int tracks_loaded;
    BgmFade fade;
    bool enabled;
} QaBgm;

typedef struct {
    Wave flow;
    Wave swish;
    bool flow_loaded;
    bool swish_loaded;
} QaBrushLoops;

typedef struct {
    uint64_t frames;
    bool fluid_finite;
    bool audio_finite;

    uint64_t black_frames;
    uint64_t white_frames;
    uint64_t static_frames;
    double mean_luma_sum;
    double draw_ms_sum;
    double draw_ms_max;
    uint32_t max_prim_verts;
    uint32_t max_blend_switches;
    uint64_t prev_hash;
    bool has_prev_hash;

    uint64_t samples;
    int32_t se_peak;
    double se_sq_sum;
    int32_t bgm_peak;
    double bgm_sq_sum;
    int32_t mix_peak;
    double mix_sq_sum;
    uint64_t clipped_samples;
    uint64_t silent_ticks;

    float max_shake_amp;
    double shake_amp_sum;
    uint64_t shake_active_frames;
    float tb_max_shake_amp;
    uint64_t tb_shake_frames;
    int32_t max_callouts_active;
    int32_t max_major_fx_active;
} QaMetrics;

typedef struct {
    QaOptions opt;
    GameState *game;
    AudioState *audio;
    RenderState *render;
    UiView ui;
    RenderTexture2D target;
    bool lobby;
    float non_tb_shake_t;
    FILE *video_fp;
    FILE *audio_fp;
    uint64_t audio_bytes;
    QaBgm bgm;
    QaBrushLoops brush;
    QaMetrics m;
} QaSession;

static void print_usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s --list-scenarios\n"
            "       %s --scenario <name> [--frames N] [--video-out PATH] [--audio-out PATH]\n"
            "              [--report-out PATH] [--no-bgm] [--no-post] [--no-render]\n",
            prog, prog);
}

static int find_scenario(const char *name)
{
    for (int i = 0; i < QA_SCN_COUNT; ++i) {
        if (strcmp(name, k_scenarios[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static bool parse_frames(const char *text, int *out)
{
    char *end = NULL;
    errno = 0;
    const long v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v <= 0 || v > QA_MAX_FRAMES) {
        return false;
    }
    *out = (int)v;
    return true;
}

static const char *option_value(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "qa_record: %s requires a value\n", argv[*i]);
        return NULL;
    }
    ++*i;
    return argv[*i];
}

static const char *option_path(int argc, char **argv, int *i)
{
    const char *v = option_value(argc, argv, i);
    if (v == NULL) {
        return NULL;
    }
    if (v[0] == '\0' || (v[0] == '-' && v[1] == '-')) {
        fprintf(stderr, "qa_record: %s requires a file path (got '%s')\n", argv[*i - 1], v);
        return NULL;
    }
    return v;
}

static bool parse_args(int argc, char **argv, QaOptions *opt)
{
    *opt = (QaOptions){.scenario = QA_SCN_COUNT, .bgm = true, .post = true};
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list-scenarios") == 0) {
            opt->list = true;
        } else if (strcmp(arg, "--no-bgm") == 0) {
            opt->bgm = false;
        } else if (strcmp(arg, "--no-post") == 0) {
            opt->post = false;
        } else if (strcmp(arg, "--no-render") == 0) {
            opt->no_render = true;
        } else if (strcmp(arg, "--scenario") == 0) {
            const char *v = option_value(argc, argv, &i);
            if (v == NULL) {
                return false;
            }
            const int id = find_scenario(v);
            if (id < 0) {
                fprintf(stderr, "qa_record: unknown scenario '%s'\n", v);
                return false;
            }
            opt->scenario = (QaScenario)id;
        } else if (strcmp(arg, "--frames") == 0) {
            const char *v = option_value(argc, argv, &i);
            if (v == NULL) {
                return false;
            }
            if (!parse_frames(v, &opt->frames)) {
                fprintf(stderr, "qa_record: --frames must be an integer in 1..%d\n", QA_MAX_FRAMES);
                return false;
            }
        } else if (strcmp(arg, "--video-out") == 0) {
            if ((opt->video_out = option_path(argc, argv, &i)) == NULL) {
                return false;
            }
        } else if (strcmp(arg, "--audio-out") == 0) {
            if ((opt->audio_out = option_path(argc, argv, &i)) == NULL) {
                return false;
            }
        } else if (strcmp(arg, "--report-out") == 0) {
            if ((opt->report_out = option_path(argc, argv, &i)) == NULL) {
                return false;
            }
        } else {
            fprintf(stderr, "qa_record: unknown option '%s'\n", arg);
            return false;
        }
    }
    if (opt->list) {
        if (argc != 2) {
            fputs("qa_record: --list-scenarios takes no other options\n", stderr);
            return false;
        }
        return true;
    }
    if (opt->scenario == QA_SCN_COUNT) {
        fputs("qa_record: --scenario is required\n", stderr);
        return false;
    }
    if (opt->no_render && opt->video_out != NULL) {
        fputs("qa_record: --video-out cannot be combined with --no-render\n", stderr);
        return false;
    }
    if (opt->frames == 0) {
        opt->frames = k_scenarios[opt->scenario].default_frames;
    }
    return true;
}

static void put_u16le(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)(v >> 8);
}

static void put_u32le(unsigned char *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = (unsigned char)((v >> (8 * i)) & 0xffu);
    }
}

static bool write_wav_header(FILE *fp, uint32_t data_bytes)
{
    unsigned char h[QA_WAV_HEADER_BYTES];
    memcpy(h, "RIFF", 4);
    put_u32le(h + 4, 36u + data_bytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    put_u32le(h + 16, 16u);
    put_u16le(h + 20, 1u);
    put_u16le(h + 22, 1u);
    put_u32le(h + 24, (uint32_t)AUDIO_SAMPLE_RATE);
    put_u32le(h + 28, (uint32_t)AUDIO_SAMPLE_RATE * 2u);
    put_u16le(h + 32, 2u);
    put_u16le(h + 34, 16u);
    memcpy(h + 36, "data", 4);
    put_u32le(h + 40, data_bytes);
    return fwrite(h, 1, sizeof h, fp) == sizeof h;
}

static bool finish_wav(FILE *fp, uint64_t data_bytes)
{
    if (data_bytes > (uint64_t)UINT32_MAX - 36u) {
        fputs("qa_record: audio output exceeds the WAV size limit\n", stderr);
        return false;
    }
    if (fflush(fp) != 0 || fseek(fp, 0L, SEEK_SET) != 0 ||
        !write_wav_header(fp, (uint32_t)data_bytes)) {
        fprintf(stderr, "qa_record: failed to finalize WAV header: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static bool close_output(FILE **fp, const char *path)
{
    if (*fp == NULL) {
        return true;
    }
    const bool ok = fclose(*fp) == 0;
    *fp = NULL;
    if (!ok) {
        fprintf(stderr, "qa_record: failed to close '%s': %s\n", path, strerror(errno));
    }
    return ok;
}

static bool load_mono_wave(const char *rel_path, Wave *out)
{
    *out = (Wave){0};
    char full_path[PATH_MAX];
    if (!app_resolve_asset_path(rel_path, full_path, sizeof full_path)) {
        fprintf(stderr, "qa_record: asset '%s' not found\n", rel_path);
        return false;
    }
    Wave w = LoadWave(full_path);
    if (!IsWaveValid(w)) {
        /* An Ogg that opens but decodes zero frames still hands back its allocation. */
        if (w.data != NULL) {
            UnloadWave(w);
        }
        fprintf(stderr, "qa_record: failed to decode '%s'\n", rel_path);
        return false;
    }
    if (w.sampleRate != (unsigned)AUDIO_SAMPLE_RATE || w.sampleSize != 16u || w.channels != 1u) {
        WaveFormat(&w, AUDIO_SAMPLE_RATE, 16, 1);
    }
    if (w.data == NULL || w.frameCount == 0u || w.sampleRate != (unsigned)AUDIO_SAMPLE_RATE ||
        w.sampleSize != 16u || w.channels != 1u) {
        UnloadWave(w);
        fprintf(stderr, "qa_record: failed to convert '%s' to 16-bit mono\n", rel_path);
        return false;
    }
    *out = w;
    return true;
}

static void load_brush_loops(QaBrushLoops *b)
{
    b->flow_loaded = load_mono_wave("audio/sfx_brush_flow.ogg", &b->flow);
    b->swish_loaded = load_mono_wave("audio/sfx_brush_swish.ogg", &b->swish);
}

static void attach_brush_loops(const QaBrushLoops *b, AudioState *audio)
{
    audio_set_brush_samples(audio, b->flow_loaded ? (const int16_t *)b->flow.data : NULL,
                            b->flow_loaded ? (size_t)b->flow.frameCount : 0u,
                            b->swish_loaded ? (const int16_t *)b->swish.data : NULL,
                            b->swish_loaded ? (size_t)b->swish.frameCount : 0u);
}

static void unload_brush_loops(QaBrushLoops *b)
{
    if (b->flow_loaded) {
        UnloadWave(b->flow);
    }
    if (b->swish_loaded) {
        UnloadWave(b->swish);
    }
    *b = (QaBrushLoops){0};
}

static void load_bgm(QaBgm *b)
{
    static const struct {
        BgmTrack track;
        const char *path;
        bool loop;
    } k_tracks[] = {
        {BGM_LOBBY, "audio/bgm_lobby.ogg", true},
        {BGM_BATTLE, "audio/bgm_battle.ogg", true},
        {BGM_BOSS, "audio/bgm_boss.ogg", true},
        {BGM_VICTORY, "audio/bgm_victory.ogg", false},
    };
    for (size_t i = 0; i < sizeof k_tracks / sizeof k_tracks[0]; ++i) {
        const int t = (int)k_tracks[i].track;
        b->loaded[t] = load_mono_wave(k_tracks[i].path, &b->waves[t]);
        b->loop[t] = k_tracks[i].loop;
        b->pos[t] = 0u;
        if (b->loaded[t]) {
            ++b->tracks_loaded;
        }
    }
}

static void unload_bgm(QaBgm *b)
{
    for (int t = 0; t < QA_BGM_SLOTS; ++t) {
        if (b->loaded[t]) {
            UnloadWave(b->waves[t]);
            b->loaded[t] = false;
        }
    }
}

static float bgm_next_sample(QaBgm *b, BgmTrack track)
{
    const int t = (int)track;
    if (t <= (int)BGM_NONE || t >= QA_BGM_SLOTS || !b->loaded[t]) {
        return 0.0f;
    }
    const Wave *w = &b->waves[t];
    const size_t len = (size_t)w->frameCount;
    if (b->pos[t] >= len) {
        return 0.0f;
    }
    const int16_t s = ((const int16_t *)w->data)[b->pos[t]];
    ++b->pos[t];
    if (b->pos[t] >= len && b->loop[t]) {
        b->pos[t] = 0u;
    }
    return (float)s;
}

static void step_external_bgm(QaBgm *b, AudioState *audio)
{
    const BgmTrack desired = audio_bgm_track(audio);
    const bool has_ext = desired > BGM_NONE && desired <= BGM_VICTORY && b->loaded[desired];
    audio_set_external_bgm(audio, has_ext);
    const BgmFadeActions act = bgm_fade_request(&b->fade, has_ext ? desired : BGM_NONE);
    if (act.play != BGM_NONE) {
        b->pos[act.play] = 0u;
    }
    (void)bgm_fade_step(&b->fade, INK_FIXED_DT);
}

static void mix_audio_tick(QaSession *s, int16_t out[AUDIO_SAMPLES_PER_TICK], bool record)
{
    QaBgm *b = &s->bgm;
    if (b->enabled) {
        step_external_bgm(b, s->audio);
    }

    int16_t se[AUDIO_SAMPLES_PER_TICK];
    audio_render(s->audio, se, AUDIO_SAMPLES_PER_TICK);

    const AudioState *a = s->audio;
    const float pause_duck = bgm_fade_step_duck(&b->fade, a->paused, INK_FIXED_DT);
    const float base_vol = ((float)a->master_vol * 0.01f) * ((float)a->bgm_vol * 0.01f) *
                           AUDIO_EXTERNAL_BGM_GAIN * pause_duck;
    const BgmTrack cur = b->fade.cur;
    const BgmTrack prev = b->fade.prev;
    const BgmTrack prev2 = b->fade.prev2;
    const float gain_cur = base_vol * b->fade.gain_cur;
    const float gain_prev = base_vol * b->fade.gain_prev;
    const float gain_prev2 = base_vol * b->fade.gain_prev2;

    int32_t tick_peak = 0;
    for (int i = 0; i < AUDIO_SAMPLES_PER_TICK; ++i) {
        float bgm_sample = 0.0f;
        if (b->enabled) {
            bgm_sample = bgm_next_sample(b, cur) * gain_cur;
            if (prev != cur) {
                bgm_sample += bgm_next_sample(b, prev) * gain_prev;
            }
            if (prev2 != cur && prev2 != prev) {
                bgm_sample += bgm_next_sample(b, prev2) * gain_prev2;
            }
        }
        float mixed = (float)se[i] + bgm_sample;
        if (!isfinite(mixed)) {
            mixed = 0.0f;
            s->m.audio_finite = false;
        }
        const bool clipped = fabsf(mixed) >= 32767.0f;
        out[i] = (int16_t)lroundf(ink_clampf(mixed, -32767.0f, 32767.0f));

        const int32_t out_abs = abs((int32_t)out[i]);
        if (out_abs > tick_peak) {
            tick_peak = out_abs;
        }
        if (!record) {
            continue;
        }
        const int32_t se_abs = abs((int32_t)se[i]);
        if (se_abs > s->m.se_peak) {
            s->m.se_peak = se_abs;
        }
        s->m.se_sq_sum += (double)se[i] * (double)se[i];
        const float bgm_abs = fabsf(bgm_sample);
        if (isfinite(bgm_abs) && bgm_abs > (float)s->m.bgm_peak) {
            s->m.bgm_peak = (int32_t)fminf(bgm_abs, 32767.0f);
        }
        if (isfinite(bgm_sample)) {
            s->m.bgm_sq_sum += (double)bgm_sample * (double)bgm_sample;
        }
        if (out_abs > s->m.mix_peak) {
            s->m.mix_peak = out_abs;
        }
        s->m.mix_sq_sum += (double)out[i] * (double)out[i];
        if (clipped) {
            ++s->m.clipped_samples;
        }
    }
    if (record) {
        s->m.samples += (uint64_t)AUDIO_SAMPLES_PER_TICK;
        if (tick_peak == 0) {
            ++s->m.silent_ticks;
        }
    }
}

static bool write_audio_tick(QaSession *s, const int16_t pcm[AUDIO_SAMPLES_PER_TICK])
{
    if (s->audio_fp == NULL) {
        return true;
    }
    unsigned char buf[AUDIO_SAMPLES_PER_TICK * 2];
    for (int i = 0; i < AUDIO_SAMPLES_PER_TICK; ++i) {
        put_u16le(buf + 2 * i, (uint16_t)pcm[i]);
    }
    if (fwrite(buf, 1, sizeof buf, s->audio_fp) != sizeof buf) {
        fprintf(stderr, "qa_record: audio write failed: %s\n", strerror(errno));
        return false;
    }
    s->audio_bytes += sizeof buf;
    return true;
}

static uint64_t fnv1a(uint64_t h, const unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)p[i];
        h *= k_fnv_prime;
    }
    return h;
}

static bool capture_video_frame(QaSession *s)
{
    render_draw_to_texture(s->render, s->game, &s->ui, s->target);
    const double draw_ms = s->render->last_metrics.draw_ms;

    Image img = LoadImageFromTexture(s->target.texture);
    if (img.data != NULL && img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    if (img.data == NULL || img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 ||
        img.width != INK_SCREEN_W || img.height != INK_SCREEN_H) {
        fputs("qa_record: failed to read back the offscreen frame as RGBA8\n", stderr);
        UnloadImage(img);
        return false;
    }

    const size_t row_bytes = (size_t)img.width * 4u;
    const unsigned char *pixels = img.data;
    uint64_t hash = k_fnv_offset;
    double luma_sum = 0.0;
    double luma_min = 255.0;
    double luma_max = 0.0;
    uint32_t luma_n = 0;
    bool write_ok = true;

    /* GL readback is bottom-up; rows go out in reverse instead of flipping a 3.6 MB copy. */
    for (int y = img.height - 1; y >= 0; --y) {
        const unsigned char *row = pixels + (size_t)y * row_bytes;
        if (s->video_fp != NULL && fwrite(row, 1, row_bytes, s->video_fp) != row_bytes) {
            write_ok = false;
            break;
        }
        if ((img.height - 1 - y) % QA_SAMPLE_STRIDE != 0) {
            continue;
        }
        for (int x = 0; x < img.width; x += QA_SAMPLE_STRIDE) {
            const unsigned char *p = row + (size_t)x * 4u;
            const double luma = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            luma_sum += luma;
            luma_min = luma < luma_min ? luma : luma_min;
            luma_max = luma > luma_max ? luma : luma_max;
            ++luma_n;
            hash = fnv1a(hash, p, 4u);
        }
    }
    UnloadImage(img);
    if (!write_ok) {
        fprintf(stderr, "qa_record: video write failed: %s\n", strerror(errno));
        return false;
    }

    QaMetrics *m = &s->m;
    const double mean = luma_n > 0u ? luma_sum / (double)luma_n : 0.0;
    m->mean_luma_sum += mean;
    if (mean < 4.0 && luma_max < 16.0) {
        ++m->black_frames;
    }
    if (mean > 251.0 && luma_min > 240.0) {
        ++m->white_frames;
    }
    if (m->has_prev_hash && hash == m->prev_hash) {
        ++m->static_frames;
    }
    m->prev_hash = hash;
    m->has_prev_hash = true;
    m->draw_ms_sum += draw_ms;
    m->draw_ms_max = draw_ms > m->draw_ms_max ? draw_ms : m->draw_ms_max;
    if (s->render->last_metrics.prim_verts > m->max_prim_verts) {
        m->max_prim_verts = s->render->last_metrics.prim_verts;
    }
    if (s->render->last_metrics.blend_switches > m->max_blend_switches) {
        m->max_blend_switches = s->render->last_metrics.blend_switches;
    }
    return true;
}

static void sample_vfx(QaSession *s)
{
    const RenderState *r = s->render;
    QaMetrics *m = &s->m;
    if (r->shake_t > 0.0f && r->shake_amp > 0.01f) {
        ++m->shake_active_frames;
        m->shake_amp_sum += (double)r->shake_amp;
        m->max_shake_amp = r->shake_amp > m->max_shake_amp ? r->shake_amp : m->max_shake_amp;
        if (s->non_tb_shake_t <= 0.0f) {
            ++m->tb_shake_frames;
            m->tb_max_shake_amp = fmaxf(m->tb_max_shake_amp, r->shake_amp);
        }
    }
    int32_t callouts = 0;
    for (int i = 0; i < RENDER_MAX_CALLOUTS; ++i) {
        callouts += r->callouts[i].active ? 1 : 0;
    }
    m->max_callouts_active = callouts > m->max_callouts_active ? callouts : m->max_callouts_active;
    m->max_major_fx_active =
        r->major_fx_active > m->max_major_fx_active ? r->major_fx_active : m->max_major_fx_active;
}

/* Every renderer shake source other than Thunderburst opens a window during which the shared
 * shake amplitude is not attributed to Thunderburst. */
static void track_non_thunderburst_shake(QaSession *s)
{
    s->non_tb_shake_t = fmaxf(0.0f, s->non_tb_shake_t - INK_FIXED_DT);
    const GameState *g = s->game;
    const int32_t count = g->event_count < GAME_MAX_EVENTS ? g->event_count : GAME_MAX_EVENTS;
    for (int32_t i = 0; i < count; ++i) {
        const GameEvent *ev = &g->events[i];
        if (ev->type == EV_CATACLYSM) {
            s->non_tb_shake_t = fmaxf(s->non_tb_shake_t, QA_CATACLYSM_SHAKE_WINDOW);
        } else if ((ev->type == EV_SHRINE_HIT && (ev->flags & EVF_SHRINE_BLOCKED) == 0) ||
                   ev->type == EV_BOSS_PHASE || ev->type == EV_BOSS_ENTER ||
                   ev->type == EV_BOSS_PART_BREAK || ev->type == EV_SLAM) {
            s->non_tb_shake_t = fmaxf(s->non_tb_shake_t, QA_OTHER_SHAKE_WINDOW);
        }
    }
}

/* Unrecorded ticks still render and drop their audio so the sample clock stays locked to the
 * game tick across skipped transitions. */
static bool qa_tick(QaSession *s, const BrushInput *in, const GameCommand *cmd, bool record)
{
    if (s->lobby) {
        game_step_sandbox(s->game, in);
        audio_consume_lobby_events(s->audio, s->game);
    } else {
        game_step(s->game, in, cmd);
        audio_consume_events(s->audio, s->game);
    }
    const bool render = !s->opt.no_render;
    if (render) {
        track_non_thunderburst_shake(s);
        render_consume_events(s->render, s->game);
        render_update_vfx(s->render, s->game, INK_FIXED_DT);
    }

    int16_t pcm[AUDIO_SAMPLES_PER_TICK];
    mix_audio_tick(s, pcm, record);
    if (!record) {
        return true;
    }

    ++s->m.frames;
    s->m.fluid_finite = s->m.fluid_finite && fluid_validate_finite(&s->game->fluid);
    s->m.audio_finite = s->m.audio_finite && audio_validate_finite(s->audio);
    if (render) {
        sample_vfx(s);
        if (!capture_video_frame(s)) {
            return false;
        }
    }
    return write_audio_tick(s, pcm);
}

static void reset_render_vfx(QaSession *s, uint64_t seed)
{
    if (!s->opt.no_render) {
        render_reset_vfx(s->render, seed);
    }
}

static void prepare_audio(QaSession *s, uint64_t seed, bool lobby)
{
    audio_init(s->audio, seed);
    attach_brush_loops(&s->brush, s->audio);
    audio_set_lobby_mode(s->audio, lobby);
    if (!s->bgm.enabled) {
        audio_set_volumes(s->audio, 80, 0, 85);
        audio_set_external_bgm(s->audio, true);
    }
    s->lobby = lobby;
}

static void hold_shrines(GameState *g, int32_t floor_hp)
{
    const int32_t n = g->shrine_count < GAME_MAX_SHRINES ? g->shrine_count : GAME_MAX_SHRINES;
    for (int32_t i = 0; i < n; ++i) {
        ShrineState *sh = &g->shrines[i];
        const int32_t target = floor_hp < sh->max_hp ? floor_hp : sh->max_hp;
        if (sh->hp < target) {
            sh->hp = target;
        }
    }
}

static bool run_lobby_brush(QaSession *s)
{
    static const BrushStyleId k_styles[] = {BRUSH_STYLE_HEAVY, BRUSH_STYLE_EMBER,
                                            BRUSH_STYLE_QUILL};
    const float cx = 720.0f;
    const float cy = 430.0f;
    const float rx = 280.0f;
    const float ry = 120.0f;

    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x10bb7001));
    game_init_sandbox(s->game, &cfg);
    reset_render_vfx(s, cfg.seed);
    prepare_audio(s, UINT64_C(101), true);
    s->ui.scene = UI_SCENE_TITLE_LOBBY;
    s->ui.selected_brush = BRUSH_STYLE_HEAVY;
    s->ui.lobby_canvas_hover = true;

    float px = cx;
    float py = cy;
    for (int f = 0; f < s->opt.frames; ++f) {
        const int seg = f / QA_LOBBY_SEGMENT_FRAMES;
        const int local = f % QA_LOBBY_SEGMENT_FRAMES;
        if (local == 0) {
            const BrushStyleId style = k_styles[seg % 3];
            s->ui.selected_brush = style;
            game_sandbox_set_brush(s->game, style);
        }

        BrushInput in = {.mx = px, .my = py, .pmx = px, .pmy = py};
        if (local >= 15 && local <= 115) {
            const float th = QA_TWO_PI * (float)(local - 15) / 100.0f;
            in.mx = cx + rx * sinf(th);
            in.my = cy + ry * sinf(2.0f * th);
            if (local == 15) {
                in.pmx = in.mx;
                in.pmy = in.my;
            }
            in.paint = true;
        } else if (local == 125) {
            in = (BrushInput){
                .mx = 520.0f, .my = 410.0f, .pmx = 520.0f, .pmy = 410.0f, .cyclone = true};
        } else if (local == 135) {
            in = (BrushInput){.mx = 760.0f,
                              .my = 410.0f,
                              .pmx = 760.0f,
                              .pmy = 410.0f,
                              .cyclone = true,
                              .cyclone_ccw = true};
        } else if (local == 145) {
            in = (BrushInput){
                .mx = 760.0f, .my = 340.0f, .pmx = 560.0f, .pmy = 460.0f, .gale = true};
        }
        s->ui.lobby_stroke_active = in.paint;
        if (!qa_tick(s, &in, NULL, true)) {
            return false;
        }
        px = in.mx;
        py = in.my;
    }
    return true;
}

static bool grant_stress_build(GameState *g)
{
    static const struct {
        UpgradeId id;
        int levels;
    } k_upgrades[] = {
        {UPG_EMBER_TIP, 2},         {UPG_STORM_BRUSH, 3},  {UPG_LASTING_SURGE, 3},
        {UPG_THUNDERBURST_FUSE, 3}, {UPG_CHARGED_SEAL, 3}, {UPG_FLAME_GALE, 2},
    };
    static const RelicId k_relics[] = {RELIC_TWIN_BRUSH, RELIC_ROSARY_SWIRL};

    for (size_t i = 0; i < sizeof k_upgrades / sizeof k_upgrades[0]; ++i) {
        for (int lv = 0; lv < k_upgrades[i].levels; ++lv) {
            if (!roguelike_grant_upgrade(g, k_upgrades[i].id)) {
                fprintf(stderr, "qa_record: failed to grant upgrade %d level %d\n",
                        (int)k_upgrades[i].id, lv + 1);
                return false;
            }
        }
    }
    for (size_t i = 0; i < sizeof k_relics / sizeof k_relics[0]; ++i) {
        if (!roguelike_grant_relic(g, k_relics[i])) {
            fprintf(stderr, "qa_record: failed to grant relic %d\n", (int)k_relics[i]);
            return false;
        }
    }
    return true;
}

static BrushInput stress_sweep_input(int frame, float pmx, float pmy)
{
    const float t = (float)frame / (float)INK_TICK_HZ;
    return (BrushInput){
        .mx = 640.0f + 420.0f * sinf(t * 2.1f),
        .my = 360.0f + 210.0f * sinf(t * 4.2f),
        .pmx = pmx,
        .pmy = pmy,
        .paint = true,
        .cyclone = frame % 120 == 0,
        .gale = frame % 90 == 45,
    };
}

static bool run_thunderburst_stress(QaSession *s)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_EMBER, UINT64_C(0x7b0557e5));
    game_init(s->game, &cfg);
    reset_render_vfx(s, cfg.seed);
    prepare_audio(s, UINT64_C(102), false);
    s->ui.scene = UI_SCENE_RUN;
    s->ui.selected_brush = cfg.brush_style;
    game_debug_start_wave(s->game, 1);
    if (!grant_stress_build(s->game)) {
        return false;
    }

    GameState *g = s->game;
    const BrushInput start = stress_sweep_input(0, 0.0f, 0.0f);
    float px = start.mx;
    float py = start.my;
    for (int f = 0; f < s->opt.frames; ++f) {
        if (g->phase != RUN_PHASE_PLAYING) {
            game_debug_start_wave(g, 1);
        }
        g->player.surge_t = g->player.surge_t < 6.0f ? 6.0f : g->player.surge_t;
        g->player.ink = g->player.ink_max;
        hold_shrines(g, 4);

        const BrushInput in = stress_sweep_input(f, px, py);
        const GameCommand cmd = {.type = CMD_NONE, .arg = 0};
        if (!qa_tick(s, &in, &cmd, true)) {
            return false;
        }
        px = in.mx;
        py = in.my;
    }
    return true;
}

static bool run_autoplay_frames(QaSession *s, int frames, int32_t shrine_floor)
{
    for (int f = 0; f < frames; ++f) {
        hold_shrines(s->game, shrine_floor);
        const BrushInput in = game_autoplay_input(s->game);
        const GameCommand cmd = game_autoplay_command(s->game);
        if (!qa_tick(s, &in, &cmd, true)) {
            return false;
        }
    }
    return true;
}

static void prepare_run(QaSession *s, uint64_t game_seed, uint64_t audio_seed)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, game_seed);
    game_init(s->game, &cfg);
    reset_render_vfx(s, cfg.seed);
    prepare_audio(s, audio_seed, false);
    s->ui.scene = UI_SCENE_RUN;
    s->ui.selected_brush = cfg.brush_style;
}

static bool run_act1_combat(QaSession *s)
{
    prepare_run(s, UINT64_C(0xac71c0b1), UINT64_C(103));
    game_debug_start_wave(s->game, 0);
    return run_autoplay_frames(s, s->opt.frames, 3);
}

static bool run_boss_showcase(QaSession *s)
{
    prepare_run(s, UINT64_C(0xb055c0de), UINT64_C(104));
    const int third = s->opt.frames / 3;
    const int seg_frames[GAME_MAIN_ACT_COUNT] = {third, third, s->opt.frames - 2 * third};
    for (int32_t act = 0; act < GAME_MAIN_ACT_COUNT; ++act) {
        if (seg_frames[act] <= 0) {
            continue;
        }
        game_debug_start_act(s->game, act);
        for (int t = 0;
             t < QA_ACT_TRANSITION_MAX_TICKS && s->game->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
            (void)qa_tick(s, NULL, NULL, false);
        }
        game_debug_start_wave(s->game, GAME_BOSS_WAVE_INDEX);
        if (!run_autoplay_frames(s, seg_frames[act], 4)) {
            return false;
        }
    }
    return true;
}

static bool run_scenario(QaSession *s)
{
    s->non_tb_shake_t = 0.0f;
    switch (s->opt.scenario) {
    case QA_SCN_LOBBY_BRUSH:
        return run_lobby_brush(s);
    case QA_SCN_THUNDERBURST_STRESS:
        return run_thunderburst_stress(s);
    case QA_SCN_ACT1_COMBAT:
        return run_act1_combat(s);
    case QA_SCN_BOSS_SHOWCASE:
        return run_boss_showcase(s);
    case QA_SCN_COUNT:
        break;
    }
    return false;
}

static const char *json_bool(bool v) { return v ? "true" : "false"; }

static double finite_or_zero(double v) { return isfinite(v) ? v : 0.0; }

static bool write_report(const QaSession *s, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "qa_record: cannot open report '%s': %s\n", path, strerror(errno));
        return false;
    }
    const QaMetrics *m = &s->m;
    const GameState *g = s->game;
    const double frames = m->frames > 0u ? (double)m->frames : 1.0;
    const double samples = m->samples > 0u ? (double)m->samples : 1.0;
    const double shake_frames = m->shake_active_frames > 0u ? (double)m->shake_active_frames : 1.0;
    const int32_t shrine_hp = g->shrine_count > 0 ? g->shrines[0].hp : 0;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"scenario\": \"%s\",\n", k_scenarios[s->opt.scenario].name);
    fprintf(fp, "  \"frames\": %" PRIu64 ",\n", m->frames);
    fprintf(fp, "  \"fps\": %d,\n", INK_TICK_HZ);
    fprintf(fp, "  \"width\": %d,\n", INK_SCREEN_W);
    fprintf(fp, "  \"height\": %d,\n", INK_SCREEN_H);
    const bool video = !s->opt.no_render;
    const bool fluid_gpu = video && s->render->post_enabled && s->render->fluid_shader.id != 0 &&
                           s->render->fluid_paper_loc >= 0 && s->render->paper_tex.id != 0 &&
                           s->render->fluid_field_tex[0].id != 0 &&
                           s->render->fluid_field_tex[1].id != 0;
    fprintf(fp, "  \"bgm_enabled\": %s,\n", json_bool(s->bgm.enabled));
    fprintf(fp, "  \"video_enabled\": %s,\n", json_bool(video));
    fprintf(fp, "  \"post_enabled\": %s,\n", json_bool(video && s->render->post_enabled));
    fprintf(fp, "  \"fluid_gpu\": %s,\n", json_bool(fluid_gpu));
    fprintf(fp, "  \"sprites_enabled\": %s,\n", json_bool(video && s->render->sprites.enabled));
    fprintf(fp, "  \"sprite_groups_ok\": %" PRId32 ",\n", video ? s->render->sprites.groups_ok : 0);
    fprintf(fp, "  \"sprite_load_ms\": %.3f,\n",
            video ? finite_or_zero(s->render->sprites.load_ms) : 0.0);
    fprintf(fp, "  \"determinism\": {\n");
    fprintf(fp, "    \"final_hash\": \"%016" PRIx64 "\",\n", game_hash(g));
    fprintf(fp, "    \"fluid_finite\": %s,\n", json_bool(m->fluid_finite));
    fprintf(fp, "    \"audio_finite\": %s,\n", json_bool(m->audio_finite));
    fprintf(fp, "    \"dropped_events\": %" PRIu32 "\n", g->dropped_events);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"video\": {\n");
    fprintf(fp, "    \"black_frames\": %" PRIu64 ",\n", m->black_frames);
    fprintf(fp, "    \"white_frames\": %" PRIu64 ",\n", m->white_frames);
    fprintf(fp, "    \"static_frames\": %" PRIu64 ",\n", m->static_frames);
    fprintf(fp, "    \"mean_luma\": %.3f,\n", finite_or_zero(m->mean_luma_sum / frames));
    fprintf(fp, "    \"avg_draw_ms\": %.3f,\n", finite_or_zero(m->draw_ms_sum / frames));
    fprintf(fp, "    \"max_draw_ms\": %.3f,\n", finite_or_zero(m->draw_ms_max));
    fprintf(fp, "    \"max_prim_verts\": %" PRIu32 ",\n", m->max_prim_verts);
    fprintf(fp, "    \"max_blend_switches\": %" PRIu32 "\n", m->max_blend_switches);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"audio\": {\n");
    fprintf(fp, "    \"samples\": %" PRIu64 ",\n", m->samples);
    fprintf(fp, "    \"se_peak\": %" PRId32 ",\n", m->se_peak);
    fprintf(fp, "    \"se_rms\": %.6f,\n", finite_or_zero(sqrt(m->se_sq_sum / samples) / 32767.0));
    fprintf(fp, "    \"bgm_peak\": %" PRId32 ",\n", m->bgm_peak);
    fprintf(fp, "    \"bgm_rms\": %.6f,\n",
            finite_or_zero(sqrt(m->bgm_sq_sum / samples) / 32767.0));
    fprintf(fp, "    \"mix_peak\": %" PRId32 ",\n", m->mix_peak);
    fprintf(fp, "    \"mix_rms\": %.6f,\n",
            finite_or_zero(sqrt(m->mix_sq_sum / samples) / 32767.0));
    fprintf(fp, "    \"clipped_samples\": %" PRIu64 ",\n", m->clipped_samples);
    fprintf(fp, "    \"clip_ratio\": %.6f,\n", (double)m->clipped_samples / samples);
    fprintf(fp, "    \"silent_ticks\": %" PRIu64 ",\n", m->silent_ticks);
    fprintf(fp, "    \"bgm_tracks_loaded\": %d\n", s->bgm.tracks_loaded);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"vfx\": {\n");
    fprintf(fp, "    \"max_shake_amp\": %.4f,\n", finite_or_zero((double)m->max_shake_amp));
    fprintf(fp, "    \"avg_shake_amp\": %.4f,\n", finite_or_zero(m->shake_amp_sum / shake_frames));
    fprintf(fp, "    \"shake_active_ratio\": %.4f,\n", (double)m->shake_active_frames / frames);
    fprintf(fp, "    \"thunderburst_max_shake_amp\": %.4f,\n",
            finite_or_zero((double)m->tb_max_shake_amp));
    fprintf(fp, "    \"thunderburst_shake_frames\": %" PRIu64 ",\n", m->tb_shake_frames);
    fprintf(fp, "    \"max_callouts_active\": %" PRId32 ",\n", m->max_callouts_active);
    fprintf(fp, "    \"max_major_fx_active\": %" PRId32 "\n", m->max_major_fx_active);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"game\": {\n");
    fprintf(fp, "    \"kills\": %" PRId32 ",\n", g->run.kills);
    fprintf(fp, "    \"max_chain\": %" PRId32 ",\n", g->run.max_chain);
    fprintf(fp, "    \"score\": %" PRId32 ",\n", g->run.score);
    fprintf(fp, "    \"ignitions\": %" PRId32 ",\n", g->run.ignitions);
    fprintf(fp, "    \"storm_zaps\": %" PRId32 ",\n", g->run.storm_zaps);
    fprintf(fp, "    \"thunderbursts\": %" PRId32 ",\n", g->run.thunderbursts);
    fprintf(fp, "    \"cataclysms\": %" PRId32 ",\n", g->run.cataclysms);
    fprintf(fp, "    \"shrine_hp\": %" PRId32 "\n", shrine_hp);
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    const bool write_ok = !ferror(fp);
    const bool close_ok = fclose(fp) == 0;
    if (!write_ok || !close_ok) {
        fprintf(stderr, "qa_record: failed to write report '%s'\n", path);
        return false;
    }
    return true;
}

typedef enum { QA_OUT_VIDEO = 0, QA_OUT_AUDIO, QA_OUT_REPORT, QA_OUT_COUNT } QaOutput;

static const char *const k_output_flags[QA_OUT_COUNT] = {"--video-out", "--audio-out",
                                                         "--report-out"};

static void output_paths(const QaOptions *opt, const char *paths[QA_OUT_COUNT])
{
    paths[QA_OUT_VIDEO] = opt->video_out;
    paths[QA_OUT_AUDIO] = opt->audio_out;
    paths[QA_OUT_REPORT] = opt->report_out;
}

static bool same_inode(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

/* Outputs are checked by inode as well as by name, so two spellings or a link of one file can
 * never be truncated by the other before recording starts. */
static bool outputs_distinct(const char *paths[QA_OUT_COUNT], const struct stat st[QA_OUT_COUNT],
                             const bool have_st[QA_OUT_COUNT])
{
    for (int i = 0; i < QA_OUT_COUNT; ++i) {
        for (int j = i + 1; j < QA_OUT_COUNT; ++j) {
            if (paths[i] == NULL || paths[j] == NULL) {
                continue;
            }
            if (strcmp(paths[i], paths[j]) == 0 ||
                (have_st[i] && have_st[j] && same_inode(&st[i], &st[j]))) {
                fprintf(stderr, "qa_record: %s and %s must name different files\n",
                        k_output_flags[i], k_output_flags[j]);
                return false;
            }
        }
    }
    return true;
}

static bool validate_outputs_before_open(const QaOptions *opt)
{
    const char *paths[QA_OUT_COUNT];
    output_paths(opt, paths);
    struct stat st[QA_OUT_COUNT];
    bool have_st[QA_OUT_COUNT];
    for (int i = 0; i < QA_OUT_COUNT; ++i) {
        have_st[i] = paths[i] != NULL && stat(paths[i], &st[i]) == 0;
    }
    if (!outputs_distinct(paths, st, have_st)) {
        return false;
    }
    /* fopen would block forever on a FIFO with no reader, so reject it before opening. */
    if (have_st[QA_OUT_AUDIO] && !S_ISREG(st[QA_OUT_AUDIO].st_mode)) {
        fprintf(stderr, "qa_record: --audio-out '%s' must be a regular file\n", opt->audio_out);
        return false;
    }
    if (have_st[QA_OUT_REPORT] && !S_ISREG(st[QA_OUT_REPORT].st_mode)) {
        fprintf(stderr, "qa_record: --report-out '%s' must be a regular file\n", opt->report_out);
        return false;
    }
    return true;
}

static bool validate_outputs_after_open(const QaSession *s)
{
    const char *paths[QA_OUT_COUNT];
    output_paths(&s->opt, paths);
    FILE *const fps[QA_OUT_COUNT] = {s->video_fp, s->audio_fp, NULL};
    struct stat st[QA_OUT_COUNT];
    bool have_st[QA_OUT_COUNT];
    for (int i = 0; i < QA_OUT_COUNT; ++i) {
        have_st[i] = fps[i] != NULL ? fstat(fileno(fps[i]), &st[i]) == 0
                                    : paths[i] != NULL && stat(paths[i], &st[i]) == 0;
    }
    if (!outputs_distinct(paths, st, have_st)) {
        return false;
    }
    if (s->audio_fp != NULL && (!have_st[QA_OUT_AUDIO] || !S_ISREG(st[QA_OUT_AUDIO].st_mode) ||
                                fseek(s->audio_fp, 0L, SEEK_CUR) != 0)) {
        fprintf(stderr, "qa_record: --audio-out '%s' must be a seekable regular file\n",
                s->opt.audio_out);
        return false;
    }
    return true;
}

static bool open_outputs(QaSession *s)
{
    if (!validate_outputs_before_open(&s->opt)) {
        return false;
    }
    if (s->opt.audio_out != NULL) {
        s->audio_fp = fopen(s->opt.audio_out, "wb");
        if (s->audio_fp == NULL) {
            fprintf(stderr, "qa_record: cannot open audio output '%s': %s\n", s->opt.audio_out,
                    strerror(errno));
            return false;
        }
    }
    if (s->opt.video_out != NULL) {
        s->video_fp = fopen(s->opt.video_out, "wb");
        if (s->video_fp == NULL) {
            fprintf(stderr, "qa_record: cannot open video output '%s': %s\n", s->opt.video_out,
                    strerror(errno));
            return false;
        }
    }
    if (!validate_outputs_after_open(s)) {
        return false;
    }
    if (s->audio_fp != NULL && !write_wav_header(s->audio_fp, 0u)) {
        fprintf(stderr, "qa_record: cannot write audio output '%s': %s\n", s->opt.audio_out,
                strerror(errno));
        return false;
    }
    return true;
}

static bool init_graphics(QaSession *s)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM - QA Recorder");
    if (!IsWindowReady()) {
        fputs("qa_record: failed to create the hidden window\n", stderr);
        return false;
    }

    char font_path[PATH_MAX];
    char title_font_path[PATH_MAX];
    const bool has_font =
        app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path);
    const bool has_title_font =
        app_resolve_asset_path("fonts/title_brush.ttf", title_font_path, sizeof title_font_path);
    char sprite_dir[PATH_MAX];
    const bool has_sprites = app_resolve_asset_path("sprites", sprite_dir, sizeof sprite_dir);
    const uint32_t fault_mask = cli_parse_debug_faults(getenv("INKSTORM_DEBUG_FAULTS"), stderr);
    const RenderOptions render_opts = {
        .use_post = s->opt.post,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
        .title_font_path = has_title_font ? title_font_path : NULL,
        .sprite_dir = has_sprites ? sprite_dir : NULL,
    };
    render_init(s->render, &render_opts);

    s->target = LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H);
    if (s->target.id == 0 || s->target.texture.id == 0 || !render_texture_usable(s->target) ||
        s->target.id == s->render->scene.id || s->target.id == s->render->bg_cache.id) {
        fputs("qa_record: failed to create a dedicated offscreen render texture\n", stderr);
        /* Never unload an id the renderer still owns. */
        if (s->target.id != 0 && s->target.id != s->render->scene.id &&
            s->target.id != s->render->bg_cache.id) {
            UnloadRenderTexture(s->target);
        }
        s->target = (RenderTexture2D){0};
        return false;
    }
    return true;
}

static void init_ui(UiView *ui)
{
    *ui = (UiView){0};
    meta_init_default(&ui->meta_view);
    ui->meta_view.settings.hints_on = false;
    ui->meta_view.settings.shake_scale = 100;
    ui->meta_view.settings.reduce_flash = false;
    ui->meta_view.settings.cursor_scale = 100;
    ui->scene = UI_SCENE_RUN;
    ui->hover_choice = UI_HOVER_NONE;
    ui->shake_scale = 100;
    ui->cursor_scale = 100;
}

static bool finish_outputs(QaSession *s, bool ok)
{
    if (ok && s->audio_fp != NULL) {
        ok = finish_wav(s->audio_fp, s->audio_bytes);
    }
    if (ok && s->video_fp != NULL && fflush(s->video_fp) != 0) {
        fprintf(stderr, "qa_record: video flush failed: %s\n", strerror(errno));
        ok = false;
    }
    const bool video_closed = close_output(&s->video_fp, s->opt.video_out);
    const bool audio_closed = close_output(&s->audio_fp, s->opt.audio_out);
    return ok && video_closed && audio_closed;
}

static void print_summary(const QaSession *s)
{
    fprintf(stderr,
            "qa_record: %s frames=%" PRIu64 " hash=%016" PRIx64 " video=%s static=%" PRIu64
            " se_peak=%" PRId32 " bgm_peak=%" PRId32 " mix_peak=%" PRId32 " thunderbursts=%" PRId32
            " tb_shake_max=%.2f\n",
            k_scenarios[s->opt.scenario].name, s->m.frames, game_hash(s->game),
            s->opt.no_render ? "off" : "on", s->m.static_frames, s->m.se_peak, s->m.bgm_peak,
            s->m.mix_peak, s->game->run.thunderbursts, (double)s->m.tb_max_shake_amp);
}

static int run_session(QaSession *s)
{
    bool ok = open_outputs(s);
    bool window_open = false;
    if (ok && !s->opt.no_render) {
        ok = init_graphics(s);
        window_open = IsWindowReady();
    }
    if (ok) {
        load_brush_loops(&s->brush);
        if (s->bgm.enabled) {
            load_bgm(&s->bgm);
        }
        ok = run_scenario(s);
    }
    if (s->audio != NULL) {
        audio_set_brush_samples(s->audio, NULL, 0u, NULL, 0u);
    }
    unload_bgm(&s->bgm);
    unload_brush_loops(&s->brush);

    ok = finish_outputs(s, ok);
    if (ok) {
        print_summary(s);
        if (s->opt.report_out != NULL) {
            ok = write_report(s, s->opt.report_out);
        }
    }

    if (s->target.id != 0) {
        UnloadRenderTexture(s->target);
        s->target = (RenderTexture2D){0};
    }
    if (window_open) {
        render_close(s->render);
        CloseWindow();
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    const char *prog = argc > 0 ? argv[0] : "qa_record";
    QaOptions opt;
    if (!parse_args(argc, argv, &opt)) {
        print_usage(stderr, prog);
        return EXIT_FAILURE;
    }
    if (opt.list) {
        for (int i = 0; i < QA_SCN_COUNT; ++i) {
            puts(k_scenarios[i].name);
        }
        return EXIT_SUCCESS;
    }

    /* A reader that exits early (ffmpeg on a FIFO) must surface as a write error, not SIGPIPE. */
    signal(SIGPIPE, SIG_IGN);
    SetTraceLogLevel(LOG_WARNING);

    QaSession session = {.opt = opt};
    QaSession *s = &session;
    init_ui(&s->ui);
    s->bgm.enabled = opt.bgm;
    bgm_fade_reset(&s->bgm.fade);
    s->m.fluid_finite = true;
    s->m.audio_finite = true;

    s->game = game_create(NULL);
    s->audio = calloc(1, sizeof *s->audio);
    s->render = calloc(1, sizeof *s->render);
    int status = EXIT_FAILURE;
    if (s->game == NULL || s->audio == NULL || s->render == NULL) {
        fputs("qa_record: failed to allocate state\n", stderr);
    } else {
        status = run_session(s);
    }

    free(s->render);
    free(s->audio);
    game_destroy(s->game);
    return status;
}
