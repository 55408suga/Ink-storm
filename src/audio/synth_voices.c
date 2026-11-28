#include "synth_voices.h"

#include "audio_dsp.h"
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
    case AUDIO_VOICE_BOSS_BREAK:
    case AUDIO_VOICE_CATACLYSM:
    case AUDIO_VOICE_GAME_OVER:
        return 3;
    case AUDIO_VOICE_KILL_TAIKO:
    case AUDIO_VOICE_CHAIN_KOTO:
    case AUDIO_VOICE_DEFLECT_BAMBOO:
    case AUDIO_VOICE_IGNITE_FLAME:
    case AUDIO_VOICE_STORM_ZAP:
    case AUDIO_VOICE_THUNDERBURST:
    case AUDIO_VOICE_ORB_DROP:
    case AUDIO_VOICE_SKILL_DENIED:
    case AUDIO_VOICE_WARD_TAP:
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
    case AUDIO_VOICE_ORB_DROP:
    case AUDIO_VOICE_DEFLECT_BAMBOO:
        return 3;
    case AUDIO_VOICE_CHAIN_KOTO:
    case AUDIO_VOICE_IGNITE_FLAME:
    case AUDIO_VOICE_STORM_ZAP:
    case AUDIO_VOICE_THUNDERBURST:
    case AUDIO_VOICE_WARD_TAP:
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

#define THUNDERBURST_RETRIGGER_SAMPLES ((uint64_t)(AUDIO_SAMPLE_RATE * 24 / 100))
#define THUNDERBURST_CHAIN_SAMPLES ((uint64_t)(AUDIO_SAMPLE_RATE * 75 / 100))
#define THUNDERBURST_CHAIN_MAX 4u
#define STORM_ZAP_RETRIGGER_SAMPLES ((uint64_t)(AUDIO_SAMPLE_RATE * 12 / 100))

static bool reaction_voice_rate_limited(const AudioState *audio, AudioVoiceKind kind, uint64_t now)
{
    return (kind == AUDIO_VOICE_THUNDERBURST && now < audio->thunderburst_next_sample) ||
           (kind == AUDIO_VOICE_STORM_ZAP && now < audio->storm_zap_next_sample);
}

/* A sweep that keeps bursting rolls on as quieter repeats instead of re-firing one full-level
 * crack; the gain rides in param so the voice never needs to see the shared chain counter. */
static void start_thunderburst_chain(AudioState *audio, AudioVoice *v, uint64_t now)
{
    if (now < audio->thunderburst_chain_until) {
        if (audio->thunderburst_chain < THUNDERBURST_CHAIN_MAX) {
            audio->thunderburst_chain++;
        }
    } else {
        audio->thunderburst_chain = 0;
    }
    audio->thunderburst_next_sample = now + THUNDERBURST_RETRIGGER_SAMPLES;
    audio->thunderburst_chain_until = now + THUNDERBURST_CHAIN_SAMPLES;
    if (v->param <= 0.0f) {
        v->param = 1.0f - 0.13f * (float)audio->thunderburst_chain;
    }
}

/* The limits use the render clock rather than the requested start: a voice for a later tick only
 * reaches here when the renderer gets to it, so per-tick and batched rendering check it at the
 * same sample, and a caller passing a stale start cannot slip past the gap. */
static AudioVoice *allocate_voice(AudioState *audio, AudioVoiceKind kind, uint64_t start_sample,
                                  float freq, float param)
{
    const uint64_t now = audio->samples_rendered;
    if (reaction_voice_rate_limited(audio, kind, now)) {
        return NULL;
    }
    const int slot = pick_voice_slot(audio, kind);
    if (slot < 0) {
        return NULL;
    }
    AudioVoice *v = &audio->voices[slot];
    memset(v, 0, sizeof *v);
    v->kind = kind;
    v->active = true;
    if (slot + 1 > audio->voice_high_water) {
        audio->voice_high_water = slot + 1;
    }
    v->start_sample = start_sample;
    v->birth_seq = ++audio->voice_seq;
    v->env = 1.0f;
    v->freq = freq;
    v->param = param;
    if (kind == AUDIO_VOICE_THUNDERBURST) {
        start_thunderburst_chain(audio, v, now);
    } else if (kind == AUDIO_VOICE_STORM_ZAP) {
        audio->storm_zap_next_sample = now + STORM_ZAP_RETRIGGER_SAMPLES;
    }
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

typedef struct {
    int style;
    bool dry;
    bool surge;
} SkillFlavor;

/* param carries the caster's BrushStyleId plus AUDIO_SKILL_PARAM_* flags. */
static SkillFlavor skill_flavor(const AudioVoice *v)
{
    const int bits = (int)lroundf(v->param);
    return (SkillFlavor){
        .style = bits & 3,
        .dry = (bits & AUDIO_SKILL_PARAM_DRY) != 0,
        .surge = (bits & AUDIO_SKILL_PARAM_SURGE) != 0,
    };
}

/* Breath through two narrow resonators on the D Yo scale: a halo that rides the airflow. */
static float skill_surge_halo(AudioVoice *v, float pink, float lo_hz, float hi_hz)
{
    return audio_svf_step(&v->svf[2], pink, lo_hz, 13.0f).bp * 0.10f +
           audio_svf_step(&v->svf[3], pink, hi_hz, 14.0f).bp * 0.08f;
}

/* Two airflow bands spin around a rising-then-settling centre. Pink noise (and, when the
 * recorded loop is attached, a slowed read of it) excites them, so the vortex has body without
 * any audible oscillator. */
static float cyclone_swirl_sample(const AudioState *audio, AudioVoice *v, float nz)
{
    static const float k_lo[3] = {220.0f, 300.0f, 400.0f};
    static const float k_hi[3] = {640.0f, 820.0f, 1080.0f};
    static const float k_flow_rate[3] = {0.70f, 0.82f, 0.94f};
    const SkillFlavor fl = skill_flavor(v);
    const int style = fl.style < 3 ? fl.style : BRUSH_STYLE_EMBER;
    const float t = (float)v->age / (float)AUDIO_SAMPLE_RATE;

    const float onset = audio_smoothstep(0.0f, 0.003f, t);
    /* The vortex appears on the cast frame; a bloom that swells over ~90 ms reads as lag. */
    const float catch_in = 1.0f + 0.65f * (1.0f - audio_smoothstep(0.0f, 0.050f, t));
    if (t > 0.18f) {
        v->env *= 0.99984f;
    }
    const float sweep =
        audio_smoothstep(0.0f, 0.045f, t) * (1.0f - 0.45f * audio_smoothstep(0.06f, 0.95f, t));
    const float lift = 0.40f + 0.60f * sweep;
    v->phase += (2.4f + 2.2f * sweep) / (float)AUDIO_SAMPLE_RATE;
    const float spin = sinf(v->phase * k_two_pi);
    const float spin2 = sinf(v->phase * k_two_pi + 1.7f);

    const float pink = audio_pink_step(&v->pink, nz);
    const bool has_flow = audio->brush_flow_pcm != NULL;
    float src = pink * (has_flow ? 1.0f : 1.6f);
    if (has_flow) {
        if (v->age == 1u) {
            v->sample_pos = (float)(audio->brush_flow_len / 6u);
        }
        src +=
            audio_pcm_read(audio->brush_flow_pcm, audio->brush_flow_len, v->sample_pos, 2048.0f) *
            1.1f;
        v->sample_pos += k_flow_rate[style] * (fl.dry ? 1.15f : 1.0f);
    }

    const float thin = fl.dry ? 1.35f : 1.0f;
    const float lo = k_lo[style] * thin * (1.0f + 0.85f * lift) * (1.0f + 0.16f * spin);
    const float hi = k_hi[style] * thin * (1.0f + 0.30f * lift) * (1.0f - 0.14f * spin2);
    const float q = fl.dry ? 1.5f : 0.9f;
    float body = audio_svf_step(&v->svf[0], src, ink_clampf(lo, 220.0f, 1400.0f), q).bp * 0.95f +
                 audio_svf_step(&v->svf[1], src, ink_clampf(hi, 220.0f, 1400.0f), q).bp * 0.70f;
    if (fl.surge && !fl.dry) {
        body += skill_surge_halo(v, pink, 587.33f, 880.00f);
    }
    body = audio_svf_step(&v->svf[4], body, fl.dry ? 2000.0f : 2600.0f, 0.71f).lp;
    return body * onset * catch_in * v->env * (fl.dry ? 0.60f : 1.0f) * 0.85f;
}

/* A silk-fan stroke: the recorded swish, pitched per brush, over a velvet band of pink noise that
 * sweeps up and back down; low Q keeps the sweep from turning into a whistle. */
static float gale_slash_sample(const AudioState *audio, AudioVoice *v, float nz)
{
    static const float k_from[3] = {380.0f, 480.0f, 620.0f};
    static const float k_peak[3] = {1500.0f, 1900.0f, 2600.0f};
    static const float k_decay[3] = {0.99975f, 0.99970f, 0.99962f};
    static const float k_swish_rate[3] = {0.78f, 0.92f, 1.12f};
    const SkillFlavor fl = skill_flavor(v);
    const int style = fl.style < 3 ? fl.style : BRUSH_STYLE_EMBER;
    const float t = (float)v->age / (float)AUDIO_SAMPLE_RATE;

    const float onset = audio_smoothstep(0.0f, 0.003f, t);
    v->env *= k_decay[style];
    const float rise = audio_smoothstep(0.0f, 0.015f, t);
    const float fall = audio_smoothstep(0.04f, 0.34f, t);
    const float centre =
        k_from[style] + (k_peak[style] - k_from[style]) * rise * (1.0f - 0.65f * fall);
    /* An air flick on the band, not a louder recording, so the procedural fallback is as prompt. */
    const float flick = 1.0f - audio_smoothstep(0.0f, 0.022f, t);

    const float pink = audio_pink_step(&v->pink, nz);
    const bool has_swish = audio->brush_swish_pcm != NULL;
    const float q = fl.dry ? 1.6f : 1.1f;
    float body = audio_svf_step(&v->svf[0], pink, centre * (fl.dry ? 1.25f : 1.0f), q).bp *
                 (has_swish ? 1.5f : 2.6f) * (1.0f + 0.55f * flick);
    if (has_swish) {
        if (v->age == 1u) {
            v->sample_pos = (float)audio->brush_swish_onset;
        }
        const float sw =
            audio_pcm_read(audio->brush_swish_pcm, audio->brush_swish_len, v->sample_pos, 512.0f);
        v->sample_pos += k_swish_rate[style] * (fl.dry ? 1.12f : 1.0f);
        body += audio_svf_step(&v->svf[1], sw, 1800.0f + 1400.0f * rise * (1.0f - fall), 0.71f).lp *
                1.15f;
    }
    if (fl.surge && !fl.dry) {
        body += skill_surge_halo(v, pink, 440.00f, 659.25f);
    }
    if (fl.dry) {
        body = audio_svf_step(&v->svf[4], body, 520.0f, 0.71f).hp;
    }
    return body * onset * v->env * (fl.dry ? 0.62f : 1.0f) * 0.95f;
}

static float step_single_voice(const AudioState *audio, AudioVoice *v, float nz)
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
        /* Pink static through two band-passes and no oscillator: raw white noise and 2-3.5 kHz
         * sines whine once zaps overlap, and even a low decaying sine ping reads as a toy blip. */
        const float t = (float)v->age / sr;
        const float onset = audio_smoothstep(0.0f, 0.002f, t);
        const float pink = audio_pink_step(&v->pink, nz);
        const float body = audio_svf_step(&v->svf[0], pink, 950.0f + 450.0f * v->env, 1.35f).bp;
        const float sizzle = audio_svf_step(&v->svf[1], pink, 1900.0f, 1.7f).bp * (v->env * v->env);
        out = (body + sizzle) * onset * v->env * 0.78f;
        v->env *= 0.99905f;
        break;
    }
    case AUDIO_VOICE_THUNDERBURST: {
        /* A low taiko body under a band-limited snap: an unfiltered crack and a 1.5-2.4 kHz sine
         * turn chained bursts into a shrill ringing. param carries the chain gain, and repeats
         * lose the snap faster than the body so a sweep rolls like distant thunder. */
        const float t = (float)v->age / sr;
        const float onset = audio_smoothstep(0.0f, 0.002f, t);
        const float gain = (v->param > 0.0f && v->param <= 1.0f) ? v->param : 1.0f;
        const float env3 = v->env * v->env * v->env;
        v->phase += (58.0f + 160.0f * env3) / sr;
        const float pink = audio_pink_step(&v->pink, nz);
        const float drum =
            sinf(v->phase * k_two_pi) * 0.74f + sinf(v->phase * k_two_pi * 1.56f) * 0.18f;
        const float rumble = audio_svf_step(&v->svf[0], pink, 260.0f, 0.78f).lp * 0.55f;
        const float snap =
            audio_svf_step(&v->svf[1], pink, 680.0f + 520.0f * v->env, 1.35f).bp * env3 * 0.65f;
        out = (drum + rumble + snap * gain) * onset * v->env * gain * 0.68f;
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
    case AUDIO_VOICE_CYCLONE_SWIRL:
        out = cyclone_swirl_sample(audio, v, nz);
        break;
    case AUDIO_VOICE_GALE_SLASH:
        out = gale_slash_sample(audio, v, nz);
        break;
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
    case AUDIO_VOICE_BOSS_BREAK: {
        /* Acoustic hyoshigi wood clacks + soft 70 Hz taiko resonator (no pure-sine oscillator).
         * param >= 0.75: full Break (double clack 90ms apart + taiko thump).
         * param < 0.75: light extend clack (single strike at scaled gain). */
        const float t = (float)v->age / sr;
        const bool full_break = v->param >= 0.75f;
        const float gain = (v->param > 0.0f && v->param <= 1.0f) ? v->param : 1.0f;
        const float pink = audio_pink_step(&v->pink, nz);

        const float c1 = audio_smoothstep(0.0f, 0.0012f, t) * expf(-t / 0.010f);
        const float t2 = t - 0.090f;
        const float c2 = (full_break && t2 >= 0.0f)
                             ? (audio_smoothstep(0.0f, 0.0012f, t2) * expf(-t2 / 0.011f) * 0.92f)
                             : 0.0f;
        const float clack_exc = pink * (c1 + c2);
        const float hi_wood = audio_svf_step(&v->svf[0], clack_exc, 1750.0f, 4.8f).bp * 0.58f +
                              audio_svf_step(&v->svf[1], clack_exc, 2120.0f, 5.2f).bp * 0.36f;
        const float lo_wood = audio_svf_step(&v->svf[2], clack_exc, 680.0f, 2.2f).bp * 0.42f;
        const float clack = hi_wood + lo_wood;

        float thump = 0.0f;
        if (full_break) {
            const float mallet =
                audio_smoothstep(0.0f, 0.0015f, t) * expf(-t / 0.010f) * pink * 1.8f;
            const float body = audio_svf_step(&v->svf[3], mallet, 70.0f, 7.5f).bp;
            const float warm = audio_svf_step(&v->svf[4], body, 220.0f, 0.75f).lp;
            thump = warm * expf(-t / 0.180f) * 1.15f;
        }

        out = (clack * 0.34f + thump * 0.36f) * gain;
        const float max_t = full_break ? 0.55f : 0.14f;
        v->env = t >= max_t ? 0.0f : expf(-t / (full_break ? 0.16f : 0.03f));
        break;
    }
    case AUDIO_VOICE_WARD_TAP: {
        /* Muted wooden ward tap (900 Hz band-pass noise resonator, no pure sine). */
        const float t = (float)v->age / sr;
        const float gain = (v->param > 0.0f && v->param <= 1.0f) ? v->param : 0.3f;
        const float pink = audio_pink_step(&v->pink, nz);
        const float tap_env = audio_smoothstep(0.0f, 0.0012f, t) * expf(-t / 0.013f);
        const float wood = audio_svf_step(&v->svf[0], pink * tap_env, 900.0f, 4.2f).bp;
        const float damp = audio_svf_step(&v->svf[1], wood, 1600.0f, 0.71f).lp;
        out = damp * gain * 0.85f;
        v->env = t >= 0.12f ? 0.0f : expf(-t / 0.025f);
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

float audio_voices_step(AudioState *audio, uint64_t sample_idx, float nz)
{
    start_due_voices(audio, sample_idx);
    float mix = 0.0f;
    int32_t hw = audio->voice_high_water;
    if (hw > AUDIO_MAX_VOICES) {
        hw = AUDIO_MAX_VOICES;
    }
    int32_t last_active = -1;
    for (int i = 0; i < hw; ++i) {
        AudioVoice *v = &audio->voices[i];
        if (!v->active) {
            continue;
        }
        if (sample_idx >= v->start_sample) {
            mix += step_single_voice(audio, v, nz);
        }
        if (v->active) {
            last_active = i;
        }
    }
    audio->voice_high_water = last_active + 1;

    return mix + audio_brush_step(audio);
}
