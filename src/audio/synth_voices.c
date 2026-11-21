#include "synth_voices.h"

#include "core.h"

#include <math.h>
#include <string.h>

static const float k_two_pi = 6.28318530718f;

/* Japanese Yo-scale pentatonic (D4, E4, G4, A4, B4, D5, E5, G5) */
const float k_yo_scale[8] = {293.66f, 329.63f, 392.00f, 440.00f,
                             493.88f, 587.33f, 659.25f, 783.99f};

float audio_noise_next(AudioState *audio) { return ink_rng_f01(&audio->rng) * 2.0f - 1.0f; }

void audio_voice_spawn(AudioState *audio, AudioVoiceKind kind, uint64_t start_sample, float freq,
                       float param)
{
    int slot = -1;
    uint64_t oldest_seq = UINT64_MAX;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!audio->voices[i].active) {
            slot = i;
            break;
        }
        if (audio->voices[i].birth_seq < oldest_seq) {
            oldest_seq = audio->voices[i].birth_seq;
            slot = i;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    AudioVoice *v = &audio->voices[slot];
    memset(v, 0, sizeof *v);
    v->kind = kind;
    v->active = true;
    v->start_sample = start_sample;
    v->birth_seq = ++audio->voice_seq;
    v->env = 1.0f;
    v->freq = freq;
    v->param = param;
}

static float step_single_voice(AudioVoice *v, float nz)
{
    const float sr = (float)AUDIO_SAMPLE_RATE;
    float out = 0.0f;
    v->age++;

    switch (v->kind) {
    case AUDIO_VOICE_KILL_TAIKO: {
        const float f = 135.0f + 380.0f * v->env * v->env;
        v->phase += f / sr;
        const float body = sinf(v->phase * k_two_pi) * 0.72f + sinf(v->phase * 15.1f) * 0.24f;
        const float snap = nz * v->env * v->env * 0.34f;
        out = (body + snap) * v->env * 0.56f;
        v->env *= 0.99918f;
        break;
    }
    case AUDIO_VOICE_CHAIN_KOTO: {
        v->phase += v->freq / sr;
        const float tone = sinf(v->phase * k_two_pi) * 0.64f +
                           sinf(v->phase * k_two_pi * 2.0f) * 0.25f * v->env +
                           sinf(v->phase * k_two_pi * 3.0f) * 0.11f * v->env;
        out = tone * v->env * 0.42f;
        v->env *= 0.99970f;
        break;
    }
    case AUDIO_VOICE_DEFLECT_BAMBOO: {
        const float f = 780.0f + 420.0f * v->env * v->env;
        v->phase += f / sr;
        v->phase2 += (f * 2.41f) / sr;
        const float clack = (sinf(v->phase * k_two_pi) * 0.68f +
                             sinf(v->phase2 * k_two_pi) * 0.32f + nz * 0.22f * v->env);
        out = clack * v->env * 0.48f;
        v->env *= 0.99875f;
        break;
    }
    case AUDIO_VOICE_SLAM_TAIKO: {
        const float f = 76.0f + 220.0f * v->env * v->env;
        v->phase += f / sr;
        const float sub = sinf(v->phase * k_two_pi) * 0.78f + sinf(v->phase * 11.4f) * 0.22f;
        v->lp += 0.14f * (nz - v->lp);
        out = (sub + v->lp * 0.45f * v->env) * v->env * 0.66f;
        v->env *= 0.99945f;
        break;
    }
    case AUDIO_VOICE_IGNITE_FLAME: {
        v->lp += 0.08f * (nz - v->lp);
        v->bp += 0.03f * (v->lp - v->bp);
        const float crackle = (nz > 0.78f) ? nz * 0.72f : 0.0f;
        v->phase += (120.0f + 90.0f * v->env) / sr;
        const float roar = v->lp * 0.75f + (v->lp - v->bp) * 0.50f +
                           sinf(v->phase * k_two_pi) * 0.18f + crackle * 0.38f;
        out = roar * v->env * 0.58f;
        v->env *= 0.99968f;
        break;
    }
    case AUDIO_VOICE_STORM_ZAP: {
        const float f = 1320.0f + 680.0f * v->env;
        v->phase += f / sr;
        v->phase2 += (f * 1.73f) / sr;
        const float spark =
            nz * 0.52f + sinf(v->phase * k_two_pi) * 0.34f + sinf(v->phase2 * k_two_pi) * 0.22f;
        out = spark * v->env * 0.46f;
        v->env *= 0.99892f;
        break;
    }
    case AUDIO_VOICE_CYCLONE_SWIRL: {
        v->lp += 0.10f * (nz - v->lp);
        v->bp += 0.045f * (v->lp - v->bp);
        const float f = 145.0f + 260.0f * (1.0f - v->env);
        v->phase += f / sr;
        out = (sinf(v->phase * k_two_pi) * 0.34f + (v->lp - v->bp) * 0.95f) * v->env * 0.52f;
        v->env *= 0.99968f;
        break;
    }
    case AUDIO_VOICE_GALE_SLASH: {
        v->lp += 0.16f * (nz - v->lp);
        const float f = 175.0f + 460.0f * v->env;
        v->phase += f / sr;
        out = ((nz - v->lp) * 0.56f + sinf(v->phase * k_two_pi) * 0.28f) * v->env * 0.50f;
        v->env *= 0.99935f;
        break;
    }
    case AUDIO_VOICE_ORB_DROP: {
        const float f = 640.0f + 420.0f * (1.0f - v->env);
        v->phase += f / sr;
        out = sinf(v->phase * k_two_pi) * v->env * 0.38f;
        v->env *= 0.99930f;
        break;
    }
    case AUDIO_VOICE_SHRINE_GONG: {
        const float f = 152.0f * (1.0f + 0.12f * v->env * v->env);
        v->phase += f / sr;
        v->phase2 += (f * 2.756f) / sr;
        const float bell = sinf(v->phase * k_two_pi) * 0.58f + sinf(v->phase2 * k_two_pi) * 0.32f +
                           sinf(v->phase * k_two_pi * 4.07f) * 0.14f * v->env;
        out = bell * v->env * 0.58f;
        v->env *= 0.99974f;
        break;
    }
    case AUDIO_VOICE_SURGE_KOTO: {
        const int step = (int)(v->age / 3600u);
        const int idx = step < 6 ? step : 5;
        const float f = k_yo_scale[idx + 2];
        v->phase += f / sr;
        const float local = 1.0f - (float)(v->age % 3600u) / 3600.0f;
        const float pluck =
            sinf(v->phase * k_two_pi) * 0.72f + sinf(v->phase * k_two_pi * 2.0f) * 0.28f * local;
        out = pluck * (0.35f + 0.65f * local) * v->env * 0.44f;
        v->env *= 0.99982f;
        break;
    }
    case AUDIO_VOICE_BOSS_TAIKO: {
        const float f = 68.0f * (1.0f + 0.45f * v->env * v->env);
        v->phase += f / sr;
        const float body = sinf(v->phase * k_two_pi) * 0.70f + sinf(v->phase * 14.8f) * 0.30f;
        out = body * v->env * 0.68f;
        v->env *= 0.99978f;
        break;
    }
    case AUDIO_VOICE_WAVE_CLEAR: {
        const float f1 = k_yo_scale[2]; /* G4 */
        const float f2 = k_yo_scale[5]; /* D5 */
        v->phase += f1 / sr;
        v->phase2 += f2 / sr;
        const float chord = sinf(v->phase * k_two_pi) * 0.52f + sinf(v->phase2 * k_two_pi) * 0.48f;
        out = chord * v->env * 0.44f;
        v->env *= 0.99978f;
        break;
    }
    case AUDIO_VOICE_ACT_CLEAR: {
        const int step = (int)(v->age / 4400u);
        const int idx = step < 5 ? step : 4;
        const float f_drum = 82.0f * (1.0f + 0.3f * v->env);
        v->phase += f_drum / sr;
        v->phase2 += k_yo_scale[idx + 2] / sr;
        const float drum = sinf(v->phase * k_two_pi) * 0.45f * v->env;
        const float koto = sinf(v->phase2 * k_two_pi) * 0.45f;
        out = (drum + koto) * v->env * 0.52f;
        v->env *= 0.99984f;
        break;
    }
    case AUDIO_VOICE_REWARD_PICK: {
        const float f_stamp = 110.0f + 160.0f * v->env * v->env;
        v->phase += f_stamp / sr;
        v->phase2 += k_yo_scale[4] / sr;
        out = (sinf(v->phase * k_two_pi) * 0.52f * v->env + sinf(v->phase2 * k_two_pi) * 0.42f) *
              v->env * 0.48f;
        v->env *= 0.99962f;
        break;
    }
    case AUDIO_VOICE_REWARD_REROLL: {
        v->lp += 0.24f * (nz - v->lp);
        v->bp += 0.08f * (v->lp - v->bp);
        const float swish = (v->lp - v->bp) * 1.25f;
        out = swish * v->env * 0.52f;
        v->env *= 0.99915f;
        break;
    }
    default:
        v->active = false;
        return 0.0f;
    }

    if (v->phase > 1000.0f) {
        v->phase -= 1000.0f;
    }
    if (v->phase2 > 1000.0f) {
        v->phase2 -= 1000.0f;
    }
    if (v->env < 0.0008f || v->age > (uint32_t)(AUDIO_SAMPLE_RATE * 3)) {
        v->active = false;
    }
    return out;
}

float audio_voices_step(AudioState *audio, uint64_t sample_idx, float nz)
{
    float mix = 0.0f;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        AudioVoice *v = &audio->voices[i];
        if (!v->active || sample_idx < v->start_sample) {
            continue;
        }
        mix += step_single_voice(v, nz);
    }

    /* Continuous brush-on-paper scrape */
    audio->brush_env += (audio->brush_target - audio->brush_env) * 0.0045f;
    if (audio->brush_env > 0.001f) {
        audio->brush_lp += 0.22f * (nz - audio->brush_lp);
        audio->brush_bp += 0.07f * (audio->brush_lp - audio->brush_bp);
        mix += (audio->brush_lp - audio->brush_bp) * audio->brush_env * 0.24f;
    }

    return mix;
}
