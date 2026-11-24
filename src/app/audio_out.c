#include "audio_out.h"

#include "core.h"
#include "paths.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

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

void audio_out_init(AudioOut *out)
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
    PlayAudioStream(out->stream);
    /* The small SE stream size is global state; music streams decoded with it underrun and
     * stutter every ~23 ms, so restore a roomy default before any OGG is opened. */
    SetAudioStreamBufferSizeDefault(AUDIO_MUSIC_STREAM_FRAMES);
    out->ready = true;
    bgm_fade_reset(&out->fade);

    load_bgm_slot(out, BGM_LOBBY, "audio/bgm_lobby.ogg", true);
    load_bgm_slot(out, BGM_BATTLE, "audio/bgm_battle.ogg", true);
    load_bgm_slot(out, BGM_BOSS, "audio/bgm_boss.ogg", true);
    load_bgm_slot(out, BGM_VICTORY, "audio/bgm_victory.ogg", false);
}

static bool track_loaded(const AudioOut *out, BgmTrack t)
{
    return (int)t > 0 && (int)t < AUDIO_BGM_TRACK_SLOTS && out->bgm_loaded[t];
}

static float external_base_volume(const AudioState *audio)
{
    const float pause_duck = audio->paused ? 0.35f : 1.0f;
    return ((float)audio->master_vol * 0.01f) * ((float)audio->bgm_vol * 0.01f) * pause_duck;
}

static void pump_external_bgm(AudioOut *out, AudioState *audio)
{
    const BgmTrack desired = audio_bgm_track(audio);
    const bool has_ext = track_loaded(out, desired);
    audio_set_external_bgm(audio, has_ext);

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

    const float base_vol = external_base_volume(audio);
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

    for (int i = 0; i < 2; ++i) {
        if (!IsAudioStreamProcessed(out->stream)) {
            break;
        }
        audio_render(audio, out->pcm, AUDIO_STREAM_FRAMES);
        UpdateAudioStream(out->stream, out->pcm, AUDIO_STREAM_FRAMES);
    }
}

void audio_out_close(AudioOut *out)
{
    if (out == NULL || !out->ready) {
        return;
    }
    for (int i = 1; i < AUDIO_BGM_TRACK_SLOTS; ++i) {
        if (out->bgm_loaded[i]) {
            StopMusicStream(out->bgm[i]);
            UnloadMusicStream(out->bgm[i]);
            out->bgm_loaded[i] = false;
        }
    }
    StopAudioStream(out->stream);
    UnloadAudioStream(out->stream);
    CloseAudioDevice();
    memset(out, 0, sizeof *out);
}
