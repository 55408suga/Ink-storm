#include "audio_out.h"

#include "core.h"
#include "paths.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { AUDIO_CB_CHUNK_FRAMES = 512 };

/* raylib's AudioCallback carries no user pointer, so the callback reaches the state it renders
 * through this module-level bridge. */
static pthread_mutex_t g_audio_cb_mtx = PTHREAD_MUTEX_INITIALIZER;
static AudioState *g_audio_cb_state = NULL;

void audio_out_configure_env(void)
{
    /* WSLg's PulseAudio server grants a ~75 ms period unless the client asks for less; libpulse
     * reads this when the device connects, and a value the user already set is kept. Must run
     * during single-threaded startup before libraries can spawn threads that inspect environ. */
    if (getenv("PULSE_SERVER") != NULL) {
        (void)setenv("PULSE_LATENCY_MSEC", "20", 0);
    }
}

void audio_out_lock(void) { (void)pthread_mutex_lock(&g_audio_cb_mtx); }

void audio_out_unlock(void) { (void)pthread_mutex_unlock(&g_audio_cb_mtx); }

static void audio_stream_callback(void *buffer_data, unsigned int frames)
{
    if (buffer_data == NULL || frames == 0u) {
        return;
    }
    /* raylib's mixer hands in a byte buffer whose alignment and declared type are uint8_t, so
     * render into typed PCM storage and copy the bytes in bounded chunks. */
    unsigned char *dst = (unsigned char *)buffer_data;
    int16_t scratch[AUDIO_CB_CHUNK_FRAMES];
    size_t remaining = (size_t)frames;

    audio_out_lock();
    while (remaining > 0u) {
        const size_t chunk =
            remaining < (size_t)AUDIO_CB_CHUNK_FRAMES ? remaining : (size_t)AUDIO_CB_CHUNK_FRAMES;
        if (g_audio_cb_state != NULL) {
            audio_render(g_audio_cb_state, scratch, chunk);
            memcpy(dst, scratch, chunk * sizeof(int16_t));
        } else {
            memset(dst, 0, chunk * sizeof(int16_t));
        }
        dst += chunk * sizeof(int16_t);
        remaining -= chunk;
    }
    audio_out_unlock();
}

static void load_bgm_slot(AudioOut *out, BgmTrack track, const char *rel_path, bool looping)
{
    if ((int)track <= 0 || (int)track >= AUDIO_BGM_TRACK_SLOTS) {
        return;
    }
    char full_path[PATH_MAX];
    if (!app_resolve_asset_path(rel_path, full_path, sizeof full_path)) {
        TraceLog(LOG_WARNING, "INKSTORM: external BGM '%s' missing; procedural fallback active",
                 rel_path);
        return;
    }
    Music m = LoadMusicStream(full_path);
    if (!IsMusicValid(m)) {
        TraceLog(LOG_WARNING,
                 "INKSTORM: failed to load external BGM '%s'; procedural fallback active",
                 rel_path);
        return;
    }
    m.looping = looping;
    out->bgm[track] = m;
    out->bgm_loaded[track] = true;
    TraceLog(LOG_INFO, "INKSTORM: loaded external BGM '%s'", rel_path);
}

static bool load_brush_loop(const char *rel_path, Wave *out_wave)
{
    char full_path[PATH_MAX];
    if (!app_resolve_asset_path(rel_path, full_path, sizeof full_path)) {
        TraceLog(LOG_WARNING, "INKSTORM: brush loop '%s' missing; procedural fallback active",
                 rel_path);
        return false;
    }
    Wave w = LoadWave(full_path);
    if (!IsWaveValid(w)) {
        /* An Ogg that opens but decodes zero frames still hands back its allocation. */
        if (w.data != NULL) {
            UnloadWave(w);
        }
        TraceLog(LOG_WARNING,
                 "INKSTORM: failed to load brush loop '%s'; procedural fallback active", rel_path);
        return false;
    }
    if (w.sampleRate != (unsigned)AUDIO_SAMPLE_RATE || w.sampleSize != 16u || w.channels != 1u) {
        WaveFormat(&w, AUDIO_SAMPLE_RATE, 16, 1);
    }
    if (w.data == NULL || w.frameCount == 0u || w.sampleRate != (unsigned)AUDIO_SAMPLE_RATE ||
        w.sampleSize != 16u || w.channels != 1u) {
        UnloadWave(w);
        TraceLog(LOG_WARNING,
                 "INKSTORM: brush loop '%s' could not be converted; procedural fallback active",
                 rel_path);
        return false;
    }
    *out_wave = w;
    TraceLog(LOG_INFO, "INKSTORM: loaded brush loop '%s' (%u frames)", rel_path, w.frameCount);
    return true;
}

static void attach_brush_loops(AudioOut *out, AudioState *audio)
{
    out->attached_audio = audio;
    if (audio == NULL) {
        return;
    }
    audio_set_brush_samples(audio,
                            out->brush_flow_loaded ? (const int16_t *)out->brush_flow.data : NULL,
                            out->brush_flow_loaded ? (size_t)out->brush_flow.frameCount : 0u,
                            out->brush_swish_loaded ? (const int16_t *)out->brush_swish.data : NULL,
                            out->brush_swish_loaded ? (size_t)out->brush_swish.frameCount : 0u);
}

static bool track_loaded(const AudioOut *out, BgmTrack t)
{
    return (int)t > 0 && (int)t < AUDIO_BGM_TRACK_SLOTS && out->bgm_loaded[t];
}

void audio_out_init(AudioOut *out, AudioState *audio)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "INKSTORM: audio device unavailable; running muted");
        return;
    }

    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_FRAMES);
    out->stream = LoadAudioStream(AUDIO_SAMPLE_RATE, 16, 1);
    if (!IsAudioStreamValid(out->stream)) {
        CloseAudioDevice();
        TraceLog(LOG_WARNING, "INKSTORM: audio stream unavailable; running muted");
        return;
    }
    /* The SE stream size is global state, and music streams decoded on the game thread with it
     * underrun and stutter, so restore a roomy default before any OGG is opened. */
    SetAudioStreamBufferSizeDefault(AUDIO_MUSIC_STREAM_FRAMES);
    out->ready = true;
    bgm_fade_reset(&out->fade);

    load_bgm_slot(out, BGM_LOBBY, "audio/bgm_lobby.ogg", true);
    load_bgm_slot(out, BGM_BATTLE, "audio/bgm_battle.ogg", true);
    load_bgm_slot(out, BGM_BOSS, "audio/bgm_boss.ogg", true);
    load_bgm_slot(out, BGM_VICTORY, "audio/bgm_victory.ogg", false);

    out->brush_flow_loaded = load_brush_loop("audio/sfx_brush_flow.ogg", &out->brush_flow);
    out->brush_swish_loaded = load_brush_loop("audio/sfx_brush_swish.ogg", &out->brush_swish);

    uint32_t ext_bgm_mask = 0u;
    for (int i = 1; i < AUDIO_BGM_TRACK_SLOTS; ++i) {
        if (out->bgm_loaded[i]) {
            ext_bgm_mask |= (UINT32_C(1) << (unsigned)i);
        }
    }

    audio_out_lock();
    attach_brush_loops(out, audio);
    audio_set_realtime_mode(audio, true);
    if (audio != NULL) {
        /* Suppress procedural BGM on init and across scene changes before the pump runs. */
        audio_set_external_bgm_mask(audio, ext_bgm_mask);
        audio_set_external_bgm(audio, track_loaded(out, audio_bgm_track(audio)));
    }
    g_audio_cb_state = audio;
    audio_out_unlock();

    SetAudioStreamCallback(out->stream, audio_stream_callback);
    PlayAudioStream(out->stream);
}

static float external_base_volume(const AudioState *audio)
{
    const float pause_duck = audio->paused ? 0.35f : 1.0f;
    return ((float)audio->master_vol * 0.01f) * ((float)audio->bgm_vol * 0.01f) * pause_duck;
}

static void pump_external_bgm(AudioOut *out, AudioState *audio)
{
    audio_out_lock();
    const BgmTrack desired = audio_bgm_track(audio);
    const bool has_ext = track_loaded(out, desired);
    audio_set_external_bgm(audio, has_ext);
    const float base_vol = external_base_volume(audio);
    audio_out_unlock();

    const BgmFadeActions act = bgm_fade_request(&out->fade, has_ext ? desired : BGM_NONE);
    if (track_loaded(out, act.stop)) {
        StopMusicStream(out->bgm[act.stop]);
    }
    if (track_loaded(out, act.play)) {
        StopMusicStream(out->bgm[act.play]);
        SetMusicVolume(out->bgm[act.play], 0.0f);
        PlayMusicStream(out->bgm[act.play]);
    }

    const float dt = ink_clampf(GetFrameTime(), 0.0f, 0.1f);
    const BgmTrack finished = bgm_fade_step(&out->fade, dt);
    if (track_loaded(out, finished)) {
        StopMusicStream(out->bgm[finished]);
    }

    if (track_loaded(out, out->fade.cur)) {
        UpdateMusicStream(out->bgm[out->fade.cur]);
        SetMusicVolume(out->bgm[out->fade.cur], base_vol * out->fade.gain_cur);
    }
    if (out->fade.prev != out->fade.cur && track_loaded(out, out->fade.prev)) {
        UpdateMusicStream(out->bgm[out->fade.prev]);
        SetMusicVolume(out->bgm[out->fade.prev], base_vol * out->fade.gain_prev);
    }
}

void audio_out_pump(AudioOut *out, AudioState *audio)
{
    if (out == NULL || !out->ready || audio == NULL) {
        return;
    }
    pump_external_bgm(out, audio);
}

void audio_out_close(AudioOut *out)
{
    if (out == NULL || !out->ready) {
        return;
    }
    audio_out_lock();
    g_audio_cb_state = NULL;
    audio_out_unlock();
    SetAudioStreamCallback(out->stream, NULL);
    StopAudioStream(out->stream);
    UnloadAudioStream(out->stream);

    if (out->attached_audio != NULL) {
        audio_set_brush_samples(out->attached_audio, NULL, 0u, NULL, 0u);
        audio_set_external_bgm_mask(out->attached_audio, 0u);
        audio_set_realtime_mode(out->attached_audio, false);
        out->attached_audio = NULL;
    }
    if (out->brush_flow_loaded) {
        UnloadWave(out->brush_flow);
        out->brush_flow_loaded = false;
    }
    if (out->brush_swish_loaded) {
        UnloadWave(out->brush_swish);
        out->brush_swish_loaded = false;
    }
    for (int i = 1; i < AUDIO_BGM_TRACK_SLOTS; ++i) {
        if (out->bgm_loaded[i]) {
            StopMusicStream(out->bgm[i]);
            UnloadMusicStream(out->bgm[i]);
            out->bgm_loaded[i] = false;
        }
    }
    CloseAudioDevice();
    memset(out, 0, sizeof *out);
}
