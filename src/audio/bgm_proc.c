#include "synth_voices.h"

#include "core.h"

#include <math.h>
#include <string.h>

static const float k_bgm_two_pi = 6.28318530718f;
static const float k_xfade_step = 1.0f / (0.5f * (float)AUDIO_SAMPLE_RATE);

static const int k_lobby_melody[8] = {0, 3, 2, 1, 0, 2, 4, 3};
static const int k_battle_melody[8] = {0, 2, 3, 2, 4, 3, 2, 0};
static const int k_boss_melody[8] = {0, 4, 3, 5, 2, 4, 5, 7};
static const int k_victory_notes[5] = {0, 2, 4, 5, 7};

static bool track_is_external(const AudioState *audio, BgmTrack track)
{
    if (audio->external_bgm_mask != 0u) {
        const int t = (int)track;
        return t > 0 && t < 32 && (audio->external_bgm_mask & (UINT32_C(1) << (unsigned)t)) != 0u;
    }
    return audio->external_bgm;
}

static bool track_has_proc_bgm(const AudioState *audio, BgmTrack track)
{
    return track != BGM_NONE && !track_is_external(audio, track);
}

void audio_bgm_apply_track(AudioState *audio, BgmTrack next)
{
    if (next == audio->active_bgm) {
        return;
    }
    const bool fading = audio->xfade_t < 1.0f;
    const float u = fading ? ink_clampf(audio->xfade_t, 0.0f, 1.0f) : 1.0f;
    const float cur_gain = u;
    const float prev_gain =
        fading ? ink_clampf(audio->bgm_prev_start_gain * (1.0f - u), 0.0f, 1.0f) : 0.0f;

    if (next != BGM_NONE && next == audio->prev_bgm && fading) {
        const AudioBgmSynth tmp = audio->bgm_cur;
        audio->bgm_cur = audio->bgm_prev;
        audio->bgm_prev = tmp;
        audio->prev_bgm = audio->active_bgm;
        audio->active_bgm = next;
        if (prev_gain >= 1.0f - 1e-6f) {
            audio->xfade_t = 1.0f;
            audio->bgm_prev_start_gain = 0.0f;
        } else {
            audio->xfade_t = prev_gain;
            audio->bgm_prev_start_gain = ink_clampf(cur_gain / (1.0f - prev_gain), 0.0f, 1.0f);
        }
        return;
    }

    const float cur_loud = track_has_proc_bgm(audio, audio->active_bgm) ? cur_gain : 0.0f;
    const float prev_loud = track_has_proc_bgm(audio, audio->prev_bgm) ? prev_gain : 0.0f;
    if (cur_loud >= prev_loud) {
        audio->bgm_prev = audio->bgm_cur;
        audio->prev_bgm = audio->active_bgm;
        audio->bgm_prev_start_gain = cur_gain;
    } else {
        audio->bgm_prev_start_gain = prev_gain;
    }
    memset(&audio->bgm_cur, 0, sizeof audio->bgm_cur);
    audio->active_bgm = next;
    audio->xfade_t = 0.0f;
}

static void wrap_deck_phases(AudioBgmSynth *deck)
{
    if (deck->drone_ph > 1000.0f) {
        deck->drone_ph -= 1000.0f;
    }
    if (deck->drum_ph > 1000.0f) {
        deck->drum_ph -= 1000.0f;
    }
    if (deck->koto_ph > 1000.0f) {
        deck->koto_ph -= 1000.0f;
    }
}

static float sample_track(const AudioState *audio, AudioBgmSynth *deck, BgmTrack track,
                          float wind_sig)
{
    const float sr = (float)AUDIO_SAMPLE_RATE;

    if (track_is_external(audio, track)) {
        /* External OGG music plays via AudioOut; keep only subtle ambient wind here. */
        return wind_sig * 0.35f;
    }

    float s = 0.0f;
    switch (track) {
    case BGM_NONE:
        return wind_sig * 0.85f;
    case BGM_LOBBY: {
        const uint64_t step_samples = (uint64_t)AUDIO_SAMPLE_RATE; /* calm 60 BPM pulse */
        const uint64_t pos_in_step = deck->sample % step_samples;
        const uint64_t step_idx = deck->sample / step_samples;
        if (pos_in_step == 0 && (step_idx % 2u) == 0u) {
            const int note = k_lobby_melody[(step_idx / 2u) % 8u];
            deck->koto_freq = k_yo_scale[note] * 0.5f;
            deck->koto_env = 0.26f;
            deck->koto_ph = 0.0f;
        }
        deck->drone_ph += 146.83f / sr;
        s = wind_sig * 0.55f + sinf(deck->drone_ph * k_bgm_two_pi) * 0.05f;
        if (deck->koto_env > 0.001f) {
            deck->koto_ph += deck->koto_freq / sr;
            s += (sinf(deck->koto_ph * k_bgm_two_pi) * 0.80f +
                  sinf(deck->koto_ph * k_bgm_two_pi * 2.0f) * 0.20f * deck->koto_env) *
                 deck->koto_env * 0.22f;
            deck->koto_env *= 0.99990f;
        }
        break;
    }
    case BGM_BATTLE: {
        const uint64_t step_samples = (uint64_t)(AUDIO_SAMPLE_RATE / 2); /* 120 BPM 8th-ish */
        const uint64_t pos_in_step = deck->sample % step_samples;
        const uint64_t step_idx = deck->sample / step_samples;
        if (pos_in_step == 0) {
            if ((step_idx % 4u) == 0u) {
                deck->drum_env = 0.42f;
                deck->drum_ph = 0.0f;
            }
            if ((step_idx % 2u) == 0u) {
                const int note = k_battle_melody[(step_idx / 2u) % 8u];
                deck->koto_freq = k_yo_scale[note] * 0.5f;
                deck->koto_env = 0.34f;
                deck->koto_ph = 0.0f;
            }
        }
        s = wind_sig * 0.45f;
        if (deck->drum_env > 0.001f) {
            deck->drum_ph += (78.0f + 45.0f * deck->drum_env) / sr;
            s += sinf(deck->drum_ph * k_bgm_two_pi) * deck->drum_env * 0.24f;
            deck->drum_env *= 0.99945f;
        }
        if (deck->koto_env > 0.001f) {
            deck->koto_ph += deck->koto_freq / sr;
            s += (sinf(deck->koto_ph * k_bgm_two_pi) * 0.75f +
                  sinf(deck->koto_ph * k_bgm_two_pi * 2.0f) * 0.25f * deck->koto_env) *
                 deck->koto_env * 0.22f;
            deck->koto_env *= 0.99982f;
        }
        break;
    }
    case BGM_BOSS: {
        const uint64_t step_samples = (uint64_t)(AUDIO_SAMPLE_RATE / 3); /* faster pulse */
        const uint64_t pos_in_step = deck->sample % step_samples;
        const uint64_t step_idx = deck->sample / step_samples;
        if (pos_in_step == 0) {
            if ((step_idx % 2u) == 0u) {
                deck->drum_env = (step_idx % 4u == 0u) ? 0.62f : 0.38f;
                deck->drum_ph = 0.0f;
            }
            const int note = k_boss_melody[step_idx % 8u];
            deck->koto_freq = k_yo_scale[note];
            deck->koto_env = 0.38f;
            deck->koto_ph = 0.0f;
        }
        deck->drone_ph += 146.83f / sr; /* D3 drone */
        s = wind_sig * 0.55f + sinf(deck->drone_ph * k_bgm_two_pi) * 0.09f;
        if (deck->drum_env > 0.001f) {
            deck->drum_ph += (68.0f + 85.0f * deck->drum_env) / sr;
            s += sinf(deck->drum_ph * k_bgm_two_pi) * deck->drum_env * 0.28f;
            deck->drum_env *= 0.99930f;
        }
        if (deck->koto_env > 0.001f) {
            deck->koto_ph += deck->koto_freq / sr;
            s += sinf(deck->koto_ph * k_bgm_two_pi) * deck->koto_env * 0.20f;
            deck->koto_env *= 0.99972f;
        }
        break;
    }
    case BGM_VICTORY: {
        deck->victory_t += 1.0f / sr;
        const int target_step = (int)(deck->victory_t / 0.28f);
        if (target_step >= 0 && target_step < 5 && target_step >= deck->victory_step) {
            deck->koto_freq = k_yo_scale[k_victory_notes[target_step]];
            deck->koto_env = 0.48f;
            deck->koto_ph = 0.0f;
            deck->victory_step = target_step + 1;
        }
        s = wind_sig * 0.35f;
        if (deck->koto_env > 0.001f) {
            deck->koto_ph += deck->koto_freq / sr;
            s += (sinf(deck->koto_ph * k_bgm_two_pi) * 0.72f +
                  sinf(deck->koto_ph * k_bgm_two_pi * 2.0f) * 0.28f * deck->koto_env) *
                 deck->koto_env * 0.30f;
            deck->koto_env *= 0.99986f;
        }
        break;
    }
    default:
        return 0.0f;
    }

    deck->sample++;
    wrap_deck_phases(deck);
    return s;
}

float audio_bgm_step(AudioState *audio, float nz)
{
    if (!audio->has_consumed) {
        return 0.0f;
    }

    audio->wind_env += (audio->wind_target - audio->wind_env) * 0.0016f;
    const float cutoff = ink_clampf(0.032f + audio->wind_env * 0.12f, 0.02f, 0.24f);
    audio->wind_lp += cutoff * (nz - audio->wind_lp);
    audio->wind_bp += (cutoff * 0.45f) * (audio->wind_lp - audio->wind_bp);
    const float wind_sig = (audio->wind_lp - audio->wind_bp) * audio->wind_env * 0.28f;

    float out = 0.0f;
    if (audio->xfade_t < 1.0f) {
        const float cur = sample_track(audio, &audio->bgm_cur, audio->active_bgm, wind_sig);
        const float prev = audio->prev_bgm == BGM_NONE
                               ? wind_sig * 0.35f
                               : sample_track(audio, &audio->bgm_prev, audio->prev_bgm, wind_sig);
        out = prev * (audio->bgm_prev_start_gain * (1.0f - audio->xfade_t)) + cur * audio->xfade_t;
        audio->xfade_t += k_xfade_step;
        if (audio->xfade_t >= 1.0f) {
            audio->xfade_t = 1.0f;
            audio->bgm_prev_start_gain = 0.0f;
        }
    } else {
        out = sample_track(audio, &audio->bgm_cur, audio->active_bgm, wind_sig);
    }

    return out;
}
