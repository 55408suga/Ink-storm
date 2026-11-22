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
    out->ready = true;
    out->current_ext_track = BGM_NONE;
    out->prev_ext_track = BGM_NONE;
    out->ext_xfade = 1.0f;

    load_bgm_slot(out, BGM_LOBBY, "audio/bgm_lobby.ogg", true);
    load_bgm_slot(out, BGM_BATTLE, "audio/bgm_battle.ogg", true);
    load_bgm_slot(out, BGM_BOSS, "audio/bgm_boss.ogg", true);
    load_bgm_slot(out, BGM_VICTORY, "audio/bgm_victory.ogg", false);
}

static void stop_external_bgm(AudioOut *out)
{
    if (out->current_ext_track != BGM_NONE && out->bgm_loaded[out->current_ext_track]) {
        StopMusicStream(out->bgm[out->current_ext_track]);
    }
    if (out->prev_ext_track != BGM_NONE && out->prev_ext_track != out->current_ext_track &&
        out->bgm_loaded[out->prev_ext_track]) {
        StopMusicStream(out->bgm[out->prev_ext_track]);
    }
    out->current_ext_track = BGM_NONE;
    out->prev_ext_track = BGM_NONE;
    out->ext_xfade = 1.0f;
}

static void pump_external_bgm(AudioOut *out, AudioState *audio)
{
    const BgmTrack desired = audio_bgm_track(audio);
    const bool has_ext =
        (int)desired > 0 && (int)desired < AUDIO_BGM_TRACK_SLOTS && out->bgm_loaded[desired];

    if (!has_ext) {
        audio_set_external_bgm(audio, false);
        if (out->current_ext_track != BGM_NONE || out->prev_ext_track != BGM_NONE) {
            stop_external_bgm(out);
        }
        return;
    }

    audio_set_external_bgm(audio, true);
    if (desired != out->current_ext_track) {
        if (out->prev_ext_track != BGM_NONE && out->prev_ext_track != desired &&
            out->prev_ext_track != out->current_ext_track && out->bgm_loaded[out->prev_ext_track]) {
            StopMusicStream(out->bgm[out->prev_ext_track]);
        }
        out->prev_ext_track = out->current_ext_track;
        out->current_ext_track = desired;
        out->ext_xfade = out->prev_ext_track == BGM_NONE ? 1.0f : 0.0f;
        StopMusicStream(out->bgm[desired]);
        PlayMusicStream(out->bgm[desired]);
    }

    const float dt = ink_clampf(GetFrameTime(), 0.0f, 0.1f);
    if (out->ext_xfade < 1.0f) {
        out->ext_xfade = fminf(1.0f, out->ext_xfade + dt * 2.0f);
    }

    const float pause_duck = audio->paused ? 0.35f : 1.0f;
    const float base_vol =
        ((float)audio->master_vol * 0.01f) * ((float)audio->bgm_vol * 0.01f) * pause_duck;

    UpdateMusicStream(out->bgm[out->current_ext_track]);
    SetMusicVolume(out->bgm[out->current_ext_track], base_vol * out->ext_xfade);

    if (out->prev_ext_track != BGM_NONE && out->prev_ext_track != out->current_ext_track &&
        out->bgm_loaded[out->prev_ext_track]) {
        if (out->ext_xfade < 1.0f) {
            UpdateMusicStream(out->bgm[out->prev_ext_track]);
            SetMusicVolume(out->bgm[out->prev_ext_track], base_vol * (1.0f - out->ext_xfade));
        } else {
            StopMusicStream(out->bgm[out->prev_ext_track]);
            out->prev_ext_track = BGM_NONE;
        }
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
