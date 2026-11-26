#include "synth_voices.h"

#include "core.h"

#include <math.h>

static const float k_bgm_two_pi = 6.28318530718f;
static const float k_xfade_step = 1.0f / (0.5f * (float)AUDIO_SAMPLE_RATE);

static const int k_lobby_melody[8] = {0, 3, 2, 1, 0, 2, 4, 3};
static const int k_battle_melody[8] = {0, 2, 3, 2, 4, 3, 2, 0};
static const int k_boss_melody[8] = {0, 4, 3, 5, 2, 4, 5, 7};
static const int k_victory_notes[5] = {0, 2, 4, 5, 7};

void audio_bgm_apply_track(AudioState *audio, BgmTrack next)
{
    if (next == audio->active_bgm) {
        return;
    }
    audio->prev_bgm = audio->active_bgm;
    audio->active_bgm = next;
    audio->xfade_t = 0.0f;
    if (next == BGM_VICTORY) {
        audio->bgm_victory_t = 0.0f;
        audio->bgm_victory_step = 0;
    }
}

static float sample_track(AudioState *audio, BgmTrack track, float wind_sig)
{
    const float sr = (float)AUDIO_SAMPLE_RATE;

    const bool has_masked_ext = (int)track > 0 && (int)track < 32 &&
                                (audio->external_bgm_mask & (UINT32_C(1) << (unsigned)track)) != 0u;
    if (audio->external_bgm || has_masked_ext) {
        /* External OGG music plays via AudioOut; keep only subtle ambient wind here. */
        return wind_sig * 0.35f;
    }

    switch (track) {
    case BGM_NONE:
        return wind_sig * 0.85f;
    case BGM_LOBBY: {
        const uint64_t step_samples = (uint64_t)AUDIO_SAMPLE_RATE; /* calm 60 BPM pulse */
        const uint64_t pos_in_step = audio->bgm_sample % step_samples;
        const uint64_t step_idx = audio->bgm_sample / step_samples;
        if (pos_in_step == 0 && (step_idx % 2u) == 0u) {
            const int note = k_lobby_melody[(step_idx / 2u) % 8u];
            audio->bgm_koto_freq = k_yo_scale[note] * 0.5f;
            audio->bgm_koto_env = 0.26f;
            audio->bgm_koto_ph = 0.0f;
        }
        audio->bgm_drone_ph += 146.83f / sr;
        if (audio->bgm_drone_ph > 1000.0f) {
            audio->bgm_drone_ph -= 1000.0f;
        }
        float s = wind_sig * 0.55f + sinf(audio->bgm_drone_ph * k_bgm_two_pi) * 0.05f;
        if (audio->bgm_koto_env > 0.001f) {
            audio->bgm_koto_ph += audio->bgm_koto_freq / sr;
            s += (sinf(audio->bgm_koto_ph * k_bgm_two_pi) * 0.80f +
                  sinf(audio->bgm_koto_ph * k_bgm_two_pi * 2.0f) * 0.20f * audio->bgm_koto_env) *
                 audio->bgm_koto_env * 0.22f;
            audio->bgm_koto_env *= 0.99990f;
        }
        return s;
    }
    case BGM_BATTLE: {
        const uint64_t step_samples = (uint64_t)(AUDIO_SAMPLE_RATE / 2); /* 120 BPM 8th-ish */
        const uint64_t pos_in_step = audio->bgm_sample % step_samples;
        const uint64_t step_idx = audio->bgm_sample / step_samples;
        if (pos_in_step == 0) {
            if ((step_idx % 4u) == 0u) {
                audio->bgm_drum_env = 0.42f;
                audio->bgm_drum_ph = 0.0f;
            }
            if ((step_idx % 2u) == 0u) {
                const int note = k_battle_melody[(step_idx / 2u) % 8u];
                audio->bgm_koto_freq = k_yo_scale[note] * 0.5f;
                audio->bgm_koto_env = 0.34f;
                audio->bgm_koto_ph = 0.0f;
            }
        }
        float s = wind_sig * 0.45f;
        if (audio->bgm_drum_env > 0.001f) {
            audio->bgm_drum_ph += (78.0f + 45.0f * audio->bgm_drum_env) / sr;
            s += sinf(audio->bgm_drum_ph * k_bgm_two_pi) * audio->bgm_drum_env * 0.24f;
            audio->bgm_drum_env *= 0.99945f;
        }
        if (audio->bgm_koto_env > 0.001f) {
            audio->bgm_koto_ph += audio->bgm_koto_freq / sr;
            s += (sinf(audio->bgm_koto_ph * k_bgm_two_pi) * 0.75f +
                  sinf(audio->bgm_koto_ph * k_bgm_two_pi * 2.0f) * 0.25f * audio->bgm_koto_env) *
                 audio->bgm_koto_env * 0.22f;
            audio->bgm_koto_env *= 0.99982f;
        }
        return s;
    }
    case BGM_BOSS: {
        const uint64_t step_samples = (uint64_t)(AUDIO_SAMPLE_RATE / 3); /* faster pulse */
        const uint64_t pos_in_step = audio->bgm_sample % step_samples;
        const uint64_t step_idx = audio->bgm_sample / step_samples;
        if (pos_in_step == 0) {
            if ((step_idx % 2u) == 0u) {
                audio->bgm_drum_env = (step_idx % 4u == 0u) ? 0.62f : 0.38f;
                audio->bgm_drum_ph = 0.0f;
            }
            const int note = k_boss_melody[step_idx % 8u];
            audio->bgm_koto_freq = k_yo_scale[note];
            audio->bgm_koto_env = 0.38f;
            audio->bgm_koto_ph = 0.0f;
        }
        audio->bgm_drone_ph += 146.83f / sr; /* D3 drone */
        if (audio->bgm_drone_ph > 1000.0f) {
            audio->bgm_drone_ph -= 1000.0f;
        }
        float s = wind_sig * 0.55f + sinf(audio->bgm_drone_ph * k_bgm_two_pi) * 0.09f;
        if (audio->bgm_drum_env > 0.001f) {
            audio->bgm_drum_ph += (68.0f + 85.0f * audio->bgm_drum_env) / sr;
            s += sinf(audio->bgm_drum_ph * k_bgm_two_pi) * audio->bgm_drum_env * 0.28f;
            audio->bgm_drum_env *= 0.99930f;
        }
        if (audio->bgm_koto_env > 0.001f) {
            audio->bgm_koto_ph += audio->bgm_koto_freq / sr;
            s += sinf(audio->bgm_koto_ph * k_bgm_two_pi) * audio->bgm_koto_env * 0.20f;
            audio->bgm_koto_env *= 0.99972f;
        }
        return s;
    }
    case BGM_VICTORY: {
        audio->bgm_victory_t += 1.0f / sr;
        const int target_step = (int)(audio->bgm_victory_t / 0.28f);
        if (target_step >= 0 && target_step < 5 && target_step >= audio->bgm_victory_step) {
            audio->bgm_koto_freq = k_yo_scale[k_victory_notes[target_step]];
            audio->bgm_koto_env = 0.48f;
            audio->bgm_koto_ph = 0.0f;
            audio->bgm_victory_step = target_step + 1;
        }
        float s = wind_sig * 0.35f;
        if (audio->bgm_koto_env > 0.001f) {
            audio->bgm_koto_ph += audio->bgm_koto_freq / sr;
            s += (sinf(audio->bgm_koto_ph * k_bgm_two_pi) * 0.72f +
                  sinf(audio->bgm_koto_ph * k_bgm_two_pi * 2.0f) * 0.28f * audio->bgm_koto_env) *
                 audio->bgm_koto_env * 0.30f;
            audio->bgm_koto_env *= 0.99986f;
        }
        return s;
    }
    default:
        return 0.0f;
    }
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
        const float cur = sample_track(audio, audio->active_bgm, wind_sig);
        const float prev = audio->prev_bgm == BGM_NONE ? wind_sig * 0.35f : wind_sig * 0.20f;
        out = prev * (1.0f - audio->xfade_t) + cur * audio->xfade_t;
        audio->xfade_t += k_xfade_step;
        if (audio->xfade_t > 1.0f) {
            audio->xfade_t = 1.0f;
        }
    } else {
        out = sample_track(audio, audio->active_bgm, wind_sig);
    }

    audio->bgm_sample++;
    if (audio->bgm_drum_ph > 1000.0f) {
        audio->bgm_drum_ph -= 1000.0f;
    }
    if (audio->bgm_koto_ph > 1000.0f) {
        audio->bgm_koto_ph -= 1000.0f;
    }
    return out;
}
