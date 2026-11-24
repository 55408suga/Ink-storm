#include "synth_voices.h"

#include "core.h"
#include "meta.h"

#include <math.h>
#include <string.h>

static const float k_two_pi = 6.28318530718f;

/* Japanese Yo-scale pentatonic (D4, E4, G4, A4, B4, D5, E5, G5) */
const float k_yo_scale[8] = {293.66f, 329.63f, 392.00f, 440.00f,
                             493.88f, 587.33f, 659.25f, 783.99f};

float audio_noise_next(AudioState *audio) { return ink_rng_f01(&audio->rng) * 2.0f - 1.0f; }

static int voice_tier(const AudioVoice *v)
{
    if (v->releasing) {
        return 0;
    }
    switch (v->kind) {
    case AUDIO_VOICE_WAVE_CLEAR:
    case AUDIO_VOICE_ACT_CLEAR:
    case AUDIO_VOICE_SHRINE_GONG:
    case AUDIO_VOICE_BOSS_TAIKO:
    case AUDIO_VOICE_CATACLYSM:
    case AUDIO_VOICE_GAME_OVER:
        return 3;
    case AUDIO_VOICE_KILL_TAIKO:
    case AUDIO_VOICE_CHAIN_KOTO:
    case AUDIO_VOICE_DEFLECT_BAMBOO:
    case AUDIO_VOICE_IGNITE_FLAME:
    case AUDIO_VOICE_STORM_ZAP:
    case AUDIO_VOICE_ORB_DROP:
    case AUDIO_VOICE_SKILL_DENIED:
    case AUDIO_VOICE_UI_CLICK:
        return 1;
    default:
        return 2;
    }
}

static int voice_kind_cap(AudioVoiceKind kind)
{
    switch (kind) {
    case AUDIO_VOICE_KILL_TAIKO:
        return 4;
    case AUDIO_VOICE_STORM_ZAP:
    case AUDIO_VOICE_ORB_DROP:
    case AUDIO_VOICE_DEFLECT_BAMBOO:
        return 3;
    case AUDIO_VOICE_CHAIN_KOTO:
    case AUDIO_VOICE_IGNITE_FLAME:
    case AUDIO_VOICE_UI_CLICK:
        return 2;
    case AUDIO_VOICE_SKILL_DENIED:
    case AUDIO_VOICE_PURIFY:
        return 1;
    default:
        return AUDIO_MAX_VOICES;
    }
}

/* Spammy hits recycle their own oldest voice first, and a full bank only steals from an equal or
 * lower tier, so a burst of zaps and kills can never cut off a clear chime or a shrine gong. */
static int pick_voice_slot(const AudioState *audio, AudioVoiceKind kind)
{
    const AudioVoice probe = {.kind = kind};
    const int tier = voice_tier(&probe);
    int same = 0;
    int oldest_same = -1;
    int free_slot = -1;
    int victim = -1;
    int victim_tier = 1 << 30;
    uint64_t oldest_same_seq = UINT64_MAX;
    uint64_t victim_seq = UINT64_MAX;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        const AudioVoice *v = &audio->voices[i];
        if (!v->active) {
            if (free_slot < 0) {
                free_slot = i;
            }
            continue;
        }
        if (v->kind == kind && !v->releasing) {
            ++same;
            if (v->birth_seq < oldest_same_seq) {
                oldest_same_seq = v->birth_seq;
                oldest_same = i;
            }
        }
        const int t = voice_tier(v);
        if (t < victim_tier || (t == victim_tier && v->birth_seq < victim_seq)) {
            victim_tier = t;
            victim_seq = v->birth_seq;
            victim = i;
        }
    }
    if (same >= voice_kind_cap(kind) && oldest_same >= 0) {
        return oldest_same;
    }
    if (free_slot >= 0) {
        return free_slot;
    }
    return victim_tier <= tier ? victim : -1;
}

static AudioVoice *allocate_voice(AudioState *audio, AudioVoiceKind kind, uint64_t start_sample,
                                  float freq, float param)
{
    const int slot = pick_voice_slot(audio, kind);
    if (slot < 0) {
        return NULL;
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
    return v;
}

/* Voices for later ticks wait here and take a slot only when their start sample is reached, so
 * per-kind caps and stealing see the same bank whether ticks are rendered one by one or in a
 * batch. */
void audio_voice_spawn(AudioState *audio, AudioVoiceKind kind, uint64_t start_sample, float freq,
                       float param)
{
    if (start_sample <= audio->samples_rendered) {
        (void)allocate_voice(audio, kind, start_sample, freq, param);
        return;
    }
    if (audio->pending_count >= AUDIO_PENDING_VOICE_CAP) {
        return;
    }
    const int32_t tail = (audio->pending_head + audio->pending_count) % AUDIO_PENDING_VOICE_CAP;
    audio->pending[tail] = (AudioPendingVoice){
        .kind = kind, .start_sample = start_sample, .freq = freq, .param = param};
    audio->pending_count++;
}

void audio_voice_spawn_ui(AudioState *audio, AudioVoiceKind kind, float freq, float param)
{
    AudioVoice *v = allocate_voice(audio, kind, audio->samples_rendered, freq, param);
    if (v != NULL) {
        v->ui = true;
    }
}

static void start_due_voices(AudioState *audio, uint64_t sample_idx)
{
    while (audio->pending_count > 0 &&
           audio->pending[audio->pending_head].start_sample <= sample_idx) {
        const AudioPendingVoice p = audio->pending[audio->pending_head];
        audio->pending_head = (audio->pending_head + 1) % AUDIO_PENDING_VOICE_CAP;
        audio->pending_count--;
        (void)allocate_voice(audio, p.kind, sample_idx, p.freq, p.param);
    }
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
    case AUDIO_VOICE_THUNDERBURST: {
        /* Deep taiko thump + sharp electric discharge burst */
        const float f_drum = 62.0f + 210.0f * v->env * v->env * v->env;
        v->phase += f_drum / sr;
        v->phase2 += (1480.0f + 920.0f * v->env) / sr;
        v->lp += 0.20f * (nz - v->lp);
        const float sub = sinf(v->phase * k_two_pi) * 0.74f + sinf(v->phase * 9.8f) * 0.20f;
        const float crack =
            ((nz - v->lp) * 0.65f + sinf(v->phase2 * k_two_pi) * 0.35f) * v->env * v->env;
        out = (sub + crack) * v->env * 0.74f;
        v->env *= 0.99958f;
        break;
    }
    case AUDIO_VOICE_CATACLYSM: {
        /* Grand taiko + gong resonance + ascending koto triad */
        const int step = (int)(v->age / 3200u);
        const int idx = step < 6 ? step : 5;
        const float f_drum = 54.0f * (1.0f + 0.65f * v->env * v->env);
        v->phase += f_drum / sr;
        v->phase2 += k_yo_scale[idx + 2] / sr;
        v->lp += 0.12f * (nz - v->lp);
        const float gong = sinf(v->phase * k_two_pi) * 0.58f +
                           sinf(v->phase * k_two_pi * 2.756f) * 0.28f * v->env +
                           sinf(v->phase * k_two_pi * 4.07f) * 0.14f * v->env;
        const float koto =
            (sinf(v->phase2 * k_two_pi) * 0.70f + sinf(v->phase2 * k_two_pi * 2.0f) * 0.30f) *
            0.42f;
        out = (gong + koto + v->lp * 0.22f * v->env) * v->env * 0.72f;
        v->env *= 0.99982f;
        break;
    }
    case AUDIO_VOICE_WIND_HOWL: {
        /* Low shakuhachi whistle + mountain gale howl */
        const float swell = v->age < 11000u ? ((float)v->age / 11000.0f) : 1.0f;
        const float f = 196.0f + 52.0f * sinf((float)v->age * 0.00025f);
        v->phase += f / sr;
        v->lp += 0.06f * (nz - v->lp);
        v->bp += 0.022f * (v->lp - v->bp);
        const float whistle =
            sinf(v->phase * k_two_pi) * 0.42f + sinf(v->phase * k_two_pi * 2.0f) * 0.14f;
        const float gale = (v->lp - v->bp) * 1.35f;
        out = (whistle + gale) * swell * v->env * 0.56f;
        v->env *= 0.99985f;
        break;
    }
    case AUDIO_VOICE_CYCLONE_SWIRL: {
        /* param carries the caster's BrushStyleId plus AUDIO_SKILL_PARAM_* flags. */
        const int bits = (int)lroundf(v->param);
        const int style = bits & 3;
        const bool dry = (bits & AUDIO_SKILL_PARAM_DRY) != 0;
        const bool surge = (bits & AUDIO_SKILL_PARAM_SURGE) != 0;
        const bool heavy = style == BRUSH_STYLE_HEAVY;
        const bool quill = style == BRUSH_STYLE_QUILL;
        const float lp_k = dry ? 0.30f : (heavy ? 0.07f : (quill ? 0.18f : 0.10f));
        v->lp += lp_k * (nz - v->lp);
        v->bp += 0.045f * (v->lp - v->bp);
        const float f = heavy   ? 105.0f + 190.0f * (1.0f - v->env)
                        : quill ? 230.0f + 420.0f * (1.0f - v->env)
                                : 145.0f + 260.0f * (1.0f - v->env);
        v->phase += f / sr;
        float body = sinf(v->phase * k_two_pi) * 0.34f + (v->lp - v->bp) * 0.95f;
        if (style == BRUSH_STYLE_EMBER && nz > 0.86f) {
            body += nz * 0.45f;
        } else if (quill) {
            v->phase2 += (f * 3.01f) / sr;
            body += sinf(v->phase2 * k_two_pi) * 0.14f * v->env;
        }
        if (surge) {
            body += sinf(v->phase * k_two_pi * 2.0f) * 0.18f * v->env;
        }
        out = body * v->env * (dry ? 0.36f : (heavy ? 0.60f : 0.52f));
        v->env *= 0.99968f;
        break;
    }
    case AUDIO_VOICE_GALE_SLASH: {
        const int bits = (int)lroundf(v->param);
        const int style = bits & 3;
        const bool dry = (bits & AUDIO_SKILL_PARAM_DRY) != 0;
        const bool surge = (bits & AUDIO_SKILL_PARAM_SURGE) != 0;
        const bool heavy = style == BRUSH_STYLE_HEAVY;
        const bool quill = style == BRUSH_STYLE_QUILL;
        v->lp += (dry ? 0.40f : (heavy ? 0.10f : (quill ? 0.26f : 0.16f))) * (nz - v->lp);
        const float f = heavy   ? 120.0f + 320.0f * v->env
                        : quill ? 260.0f + 640.0f * v->env
                                : 175.0f + 460.0f * v->env;
        v->phase += f / sr;
        float body = (nz - v->lp) * 0.56f + sinf(v->phase * k_two_pi) * 0.28f;
        if (heavy) {
            body += sinf(v->phase * k_two_pi * 0.5f) * 0.22f * v->env;
        } else if (style == BRUSH_STYLE_EMBER && nz > 0.82f) {
            body += nz * 0.40f * v->env;
        }
        if (surge) {
            body += sinf(v->phase * k_two_pi * 2.0f) * 0.16f * v->env;
        }
        out = body * v->env * (dry ? 0.34f : 0.50f);
        v->env *= quill ? 0.99915f : 0.99935f;
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
        const float strike = v->age < 2600u ? 1.0f - (float)v->age / 2600.0f : 0.0f;
        const float chord = sinf(v->phase * k_two_pi) * 0.46f + sinf(v->phase2 * k_two_pi) * 0.40f;
        const float suzu = sinf(v->phase2 * k_two_pi * 2.76f) * 0.30f * strike +
                           sinf(v->phase * k_two_pi * 5.40f) * 0.18f * strike +
                           nz * 0.08f * strike * strike;
        const float shimmer =
            sinf(v->phase2 * k_two_pi * 2.0f + 0.6f * sinf((float)v->age * 0.0009f)) * 0.12f;
        out = (chord + suzu + shimmer * v->env) * v->env * 0.48f;
        v->env *= 0.99982f;
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
    case AUDIO_VOICE_PART_BREAK: {
        /* Clear crystalline bell shatter + mid-taiko impact for Dragon Orb break */
        const float f_bell = v->freq > 100.0f ? v->freq : 987.77f;
        const float f_drum = 108.0f + 190.0f * v->env * v->env;
        v->phase += f_drum / sr;
        v->phase2 += f_bell / sr;
        v->lp += 0.28f * (nz - v->lp);
        const float drum = sinf(v->phase * k_two_pi) * 0.55f * v->env;
        const float bell = sinf(v->phase2 * k_two_pi) * 0.44f +
                           sinf(v->phase2 * k_two_pi * 2.32f) * 0.28f * v->env +
                           sinf(v->phase2 * k_two_pi * 3.85f) * 0.16f * v->env * v->env;
        const float shatter = (nz - v->lp) * 0.48f * v->env * v->env * v->env;
        out = (drum + bell + shatter) * v->env * 0.68f;
        v->env *= 0.99962f;
        break;
    }
    case AUDIO_VOICE_SKILL_DENIED: {
        /* Dry wooden clack (hyoshigi) */
        v->phase += (v->freq + 260.0f * v->env) / sr;
        v->phase2 += (v->freq * 2.64f) / sr;
        v->lp += 0.30f * (nz - v->lp);
        const float knock = sinf(v->phase * k_two_pi) * 0.50f + sinf(v->phase2 * k_two_pi) * 0.25f +
                            (nz - v->lp) * 0.30f * v->env;
        out = knock * v->env * 0.42f;
        v->env *= 0.99820f;
        break;
    }
    case AUDIO_VOICE_GAME_OVER: {
        /* Descending low koto over a muffled taiko */
        static const float k_fall[3] = {220.00f, 164.81f, 146.83f};
        const int step = (int)(v->age / 9000u);
        const float f = k_fall[step < 3 ? step : 2];
        v->phase += f / sr;
        v->phase2 += (58.0f + 120.0f * v->env * v->env) / sr;
        const float koto =
            sinf(v->phase * k_two_pi) * 0.50f + sinf(v->phase * k_two_pi * 2.0f) * 0.16f * v->env;
        const float drum = sinf(v->phase2 * k_two_pi) * 0.45f * v->env;
        out = (koto + drum) * v->env * 0.55f;
        v->env *= 0.99988f;
        break;
    }
    case AUDIO_VOICE_UI_CLICK: {
        v->phase += v->freq / sr;
        v->phase2 += (v->freq * (v->param > 0.5f ? 1.5f : 2.0f)) / sr;
        out = (sinf(v->phase * k_two_pi) * 0.40f + sinf(v->phase2 * k_two_pi) * 0.16f) * v->env *
              0.34f;
        v->env *= 0.99760f;
        break;
    }
    case AUDIO_VOICE_WARD_BELL: {
        v->phase += v->freq / sr;
        v->phase2 += (v->freq * 2.76f) / sr;
        const float bell = sinf(v->phase * k_two_pi) * 0.55f +
                           sinf(v->phase2 * k_two_pi) * 0.30f * v->env +
                           sinf(v->phase * k_two_pi * 0.5f) * 0.20f;
        out = bell * v->env * 0.46f;
        v->env *= 0.99955f;
        break;
    }
    case AUDIO_VOICE_PURIFY: {
        /* Breathy rising whoosh with a faint bell tail as the field is cleansed */
        v->lp += 0.12f * (nz - v->lp);
        v->bp += 0.04f * (v->lp - v->bp);
        v->phase += (v->freq * (0.6f + 0.4f * (1.0f - v->env))) / sr;
        const float swell = v->age < 4000u ? (float)v->age / 4000.0f : 1.0f;
        const float air = (v->lp - v->bp) * 0.85f * swell;
        out = (air + sinf(v->phase * k_two_pi) * 0.12f) * v->env * 0.44f;
        v->env *= 0.99970f;
        break;
    }
    default:
        v->active = false;
        return 0.0f;
    }

    if (v->releasing) {
        v->env *= 0.9985f;
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

static float brush_scrape_sample(AudioState *audio, float nz)
{
    const int style = audio->brush_style;
    float lp_k = 0.22f;
    float bp_k = 0.07f;
    float gain = 0.24f;
    if (audio->brush_dry) {
        lp_k = 0.46f;
        bp_k = 0.18f;
        gain = 0.17f;
    } else if (style == BRUSH_STYLE_HEAVY) {
        lp_k = 0.12f;
        bp_k = 0.035f;
        gain = 0.34f;
    } else if (style == BRUSH_STYLE_QUILL) {
        lp_k = 0.34f;
        bp_k = 0.12f;
        gain = 0.20f;
    }
    audio->brush_lp += lp_k * (nz - audio->brush_lp);
    audio->brush_bp += bp_k * (audio->brush_lp - audio->brush_bp);
    float s = (audio->brush_lp - audio->brush_bp) * gain;

    float tone_hz = 0.0f;
    float tone_amp = 0.0f;
    if (audio->brush_dry) {
        s *= 0.55f + 0.45f * (nz > 0.2f ? 1.0f : 0.0f);
    } else if (style == BRUSH_STYLE_HEAVY) {
        tone_hz = 82.0f;
        tone_amp = 0.05f;
    } else if (style == BRUSH_STYLE_EMBER) {
        if (nz > 0.93f) {
            s += nz * 0.22f;
        }
    } else if (style == BRUSH_STYLE_QUILL) {
        tone_hz = 880.0f;
        tone_amp = 0.025f;
    }
    if (audio->brush_surge && !audio->brush_dry) {
        tone_hz = tone_hz > 0.0f ? tone_hz * 2.0f : 523.25f;
        tone_amp += 0.035f;
    }
    if (tone_amp > 0.0f) {
        audio->brush_ph += tone_hz / (float)AUDIO_SAMPLE_RATE;
        if (audio->brush_ph > 1000.0f) {
            audio->brush_ph -= 1000.0f;
        }
        s += sinf(audio->brush_ph * k_two_pi) * tone_amp;
    }
    return s * audio->brush_env;
}

float audio_voices_step(AudioState *audio, uint64_t sample_idx, float nz)
{
    start_due_voices(audio, sample_idx);
    float mix = 0.0f;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        AudioVoice *v = &audio->voices[i];
        if (!v->active || sample_idx < v->start_sample) {
            continue;
        }
        mix += step_single_voice(v, nz);
    }

    const float target = audio->paused ? 0.0f : audio->brush_target;
    audio->brush_env += (target - audio->brush_env) * 0.0045f;
    if (audio->brush_env > 0.001f) {
        mix += brush_scrape_sample(audio, nz);
    }

    return mix;
}
