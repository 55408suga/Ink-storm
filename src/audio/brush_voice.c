#include "synth_voices.h"

#include "audio_dsp.h"
#include "core.h"
#include "meta.h"

#include <math.h>
#include <string.h>

enum { BRUSH_HOLD_SAMPLES = 1200 };

static const float k_brush_attack = 0.0022f;
static const float k_brush_release = 0.00075f;
static const float k_brush_fast_release = 0.0020f;
static const float k_brush_speed_glide = 0.0012f;
static const float k_surge_bloom = 0.00025f;
static const float k_surge_fade = 0.000087f;
static const float k_brush_silent = 1e-4f;

typedef enum {
    BSVF_HP1 = 0,
    BSVF_HP2,
    BSVF_LP1,
    BSVF_LP2,
    BSVF_LP3,
    BSVF_SWISH,
    BSVF_WASH,
    BSVF_BED,
    BSVF_GLOW,
    BSVF_VEIL_D4,
    BSVF_VEIL_A4,
    BSVF_VEIL_E5,
    BSVF_VEIL_G5,
    BSVF_COUNT
} BrushSvfSlot;
_Static_assert(BSVF_COUNT == AUDIO_BRUSH_SVF_COUNT, "brush filter bank size mismatch");

typedef enum {
    BWALK_SWAY = 0,
    BWALK_DELAY1,
    BWALK_DELAY2,
    BWALK_FLUTTER,
    BWALK_GLOW,
    BWALK_VIB,
    BWALK_COUNT
} BrushWalkSlot;
_Static_assert(BWALK_COUNT == AUDIO_BRUSH_WALK_COUNT, "brush drift bank size mismatch");
_Static_assert((AUDIO_BRUSH_DELAY_LEN & (AUDIO_BRUSH_DELAY_LEN - 1)) == 0,
               "brush delay length must be a power of two");

typedef struct {
    float hp_hz;
    float lp_lo;
    float lp_hi;
    float rate_lo;
    float rate_hi;
    float swish_mix;
    float body_hz;
    float level;
} BrushTone;

static const BrushTone k_brush_tones[BRUSH_STYLE_COUNT] = {
    [BRUSH_STYLE_HEAVY] = {220.0f, 1200.0f, 2050.0f, 0.68f, 0.88f, 0.30f, 460.0f, 1.00f},
    [BRUSH_STYLE_EMBER] = {290.0f, 1550.0f, 2550.0f, 0.76f, 0.98f, 0.42f, 560.0f, 0.92f},
    [BRUSH_STYLE_QUILL] = {340.0f, 1750.0f, 2850.0f, 0.82f, 1.08f, 0.85f, 680.0f, 0.88f},
};

static const BrushTone *brush_tone(int32_t style)
{
    return (style >= 0 && style < BRUSH_STYLE_COUNT) ? &k_brush_tones[style]
                                                     : &k_brush_tones[BRUSH_STYLE_EMBER];
}

static float brush_white(AudioState *a) { return ink_rng_f01(&a->brush_rng) * 2.0f - 1.0f; }

static float brush_walk(AudioState *a, BrushWalkSlot slot, float hz)
{
    return audio_walk_step(&a->brush_walk[slot], brush_white(a), hz);
}

static AudioSvfOut brush_svf(AudioState *a, BrushSvfSlot slot, float in, float hz, float q)
{
    return audio_svf_step(&a->brush_svf[slot], in, hz, q);
}

void audio_brush_reset_dsp(AudioState *a)
{
    memset(a->brush_pink, 0, sizeof a->brush_pink);
    memset(a->brush_walk, 0, sizeof a->brush_walk);
    memset(a->brush_svf, 0, sizeof a->brush_svf);
    memset(a->brush_delay, 0, sizeof a->brush_delay);
    a->brush_delay_pos = 0u;
    audio_grain_setup(&a->brush_flow_grain, a->brush_flow_len, 0.06f, 0.94f,
                      0.50f * (float)AUDIO_SAMPLE_RATE);
    audio_grain_setup(&a->brush_swish_grain, a->brush_swish_len, 0.05f, 0.85f,
                      0.14f * (float)AUDIO_SAMPLE_RATE);
    a->brush_dsp_live = false;
}

/* Returns the stroke speed the tone should follow this sample. While the brush stays down, a tick
 * with no pointer motion keeps the last moving speed for ~27 ms. A lift is not held: holding it
 * delayed every release by the hold length. A stroke starting from silence takes its speed at
 * once, since gliding up from zero muffled the start of a fast flick. */
static float brush_step_envelopes(AudioState *a)
{
    const float gate = a->paused ? 0.0f : ink_clampf(a->brush_target, 0.0f, 1.0f);
    const bool held = !a->paused && a->brush_active;
    float speed = a->brush_speed;
    if (!held) {
        a->brush_hold = 0u;
    } else if (a->brush_speed > 0.0f) {
        a->brush_hold = BRUSH_HOLD_SAMPLES;
        a->brush_hold_speed = a->brush_speed;
    } else if (a->brush_hold > 0u) {
        --a->brush_hold;
        speed = a->brush_hold_speed;
    }
    if (gate > 0.0f) {
        a->brush_fast_release = false;
    }
    if (held && a->brush_env < 0.02f && speed > a->brush_speed_smooth) {
        a->brush_speed_smooth = speed;
    }

    float k = k_brush_release;
    if (gate > a->brush_env) {
        k = k_brush_attack;
    } else if (a->paused || a->brush_fast_release) {
        k = k_brush_fast_release;
    }
    a->brush_env += (gate - a->brush_env) * k;
    a->brush_speed_smooth += (speed - a->brush_speed_smooth) * k_brush_speed_glide;

    const float veil = (a->brush_surge && !a->brush_dry) ? 1.0f : 0.0f;
    a->brush_surge_env +=
        (veil - a->brush_surge_env) * (veil > a->brush_surge_env ? k_surge_bloom : k_surge_fade);
    return ink_clampf(a->brush_speed_smooth, 0.0f, 1.0f);
}

static float delay_tap(const float *line, uint32_t write_pos, float tap)
{
    const float fl = floorf(tap);
    const uint32_t whole = (uint32_t)fl;
    const float frac = tap - fl;
    const uint32_t mask = AUDIO_BRUSH_DELAY_LEN - 1u;
    const float x0 = line[(write_pos - whole) & mask];
    const float x1 = line[(write_pos - whole - 1u) & mask];
    return x0 + (x1 - x0) * frac;
}

/* Without the recorded loops the bed is pink noise stirred through two slowly wandering short
 * delays: a liquid swirl rather than a tone, so no oscillator ever beeps through. */
static float brush_procedural_bed(AudioState *a, const BrushTone *tone, float spd, float pink_a,
                                  float pink_b)
{
    const float sway = 1.0f + 0.16f * brush_walk(a, BWALK_SWAY, 2.1f);
    const float wash_in = brush_svf(a, BSVF_WASH, pink_b, 620.0f + 480.0f * spd, 0.72f).bp;
    const float m1 = brush_walk(a, BWALK_DELAY1, 0.75f + 0.9f * spd);
    const float m2 = brush_walk(a, BWALK_DELAY2, 1.10f + 1.1f * spd);
    const float tap1 = ink_clampf(250.0f + 70.0f * m1 - 40.0f * spd, 176.0f, 400.0f);
    const float tap2 = ink_clampf(400.0f + 90.0f * m2 - 60.0f * spd, 260.0f, 530.0f);
    const uint32_t pos = a->brush_delay_pos;
    const float rd1 = delay_tap(a->brush_delay[0], pos, tap1);
    const float rd2 = delay_tap(a->brush_delay[1], pos, tap2);
    a->brush_delay[0][pos] = wash_in + rd1 * 0.34f;
    a->brush_delay[1][pos] = wash_in + rd2 * 0.28f;
    a->brush_delay_pos = (pos + 1u) & (AUDIO_BRUSH_DELAY_LEN - 1u);

    const float bed = brush_svf(a, BSVF_BED, pink_a * sway, tone->body_hz + 440.0f * spd, 0.65f).bp;
    return (bed * 0.52f + (rd1 * 0.55f + rd2 * 0.45f) * 0.82f) * 4.0f;
}

static float brush_swish_layer(AudioState *a, const BrushTone *tone, float spd, float pink_b)
{
    const float bloom = audio_smoothstep(0.30f, 0.90f, spd) * tone->swish_mix;
    if (a->brush_swish_pcm != NULL) {
        const float rate = 0.65f + 0.25f * spd + 0.20f * (tone->rate_hi - 0.88f);
        const float sw = audio_grain_step(&a->brush_swish_grain, a->brush_swish_pcm,
                                          a->brush_swish_len, rate, &a->brush_rng);
        return brush_svf(a, BSVF_SWISH, sw, 1200.0f + 700.0f * spd, 0.71f).lp * bloom * 1.4f;
    }
    return brush_svf(a, BSVF_SWISH, pink_b, 900.0f + 900.0f * spd, 0.70f).bp * bloom * 0.9f;
}

/* Breath through narrow resonators on the Lobby BGM's D Yo scale: a tuned mist, not a sine. */
static float brush_surge_veil(AudioState *a, float spd, float pink)
{
    const float vib = 1.0f + 0.006f * brush_walk(a, BWALK_VIB, 3.2f);
    const float d4 = brush_svf(a, BSVF_VEIL_D4, pink, 293.66f * vib, 14.0f).bp;
    const float a4 = brush_svf(a, BSVF_VEIL_A4, pink, 440.00f * vib, 15.0f).bp;
    const float e5 = brush_svf(a, BSVF_VEIL_E5, pink, 659.25f * vib, 14.0f).bp *
                     audio_smoothstep(0.25f, 0.85f, spd);
    const float g5 = brush_svf(a, BSVF_VEIL_G5, pink, 783.99f * vib, 13.0f).bp *
                     audio_smoothstep(0.55f, 1.00f, spd);
    return (d4 * 0.36f + a4 * 0.32f + e5 * 0.24f + g5 * 0.14f) * a->brush_surge_env * 0.22f;
}

float audio_brush_step(AudioState *a)
{
    const float spd = brush_step_envelopes(a);
    if (a->brush_env < k_brush_silent) {
        if (a->brush_dsp_live) {
            audio_brush_reset_dsp(a);
        }
        return 0.0f;
    }
    a->brush_dsp_live = true;

    const BrushTone *tone = brush_tone(a->brush_style);
    const bool dry = a->brush_dry;
    const float pink_a = audio_pink_step(&a->brush_pink[0], brush_white(a));
    const float pink_b = audio_pink_step(&a->brush_pink[1], brush_white(a));

    float src = 0.0f;
    if (a->brush_flow_pcm != NULL) {
        const float rate = tone->rate_lo + (tone->rate_hi - tone->rate_lo) * spd;
        src = audio_grain_step(&a->brush_flow_grain, a->brush_flow_pcm, a->brush_flow_len, rate,
                               &a->brush_rng) *
              2.2f;
    } else {
        src = brush_procedural_bed(a, tone, spd, pink_a, pink_b);
    }
    src += brush_swish_layer(a, tone, spd, pink_b);

    if (!dry && a->brush_style == BRUSH_STYLE_EMBER) {
        const float drift = 1.0f + 0.08f * brush_walk(a, BWALK_GLOW, 0.7f);
        src +=
            brush_svf(a, BSVF_GLOW, pink_a, 1850.0f * drift, 1.4f).bp * (0.5f + 0.5f * spd) * 0.55f;
    }

    float level = (0.25f + 0.75f * powf(spd, 0.9f)) * a->brush_env * tone->level;
    float hp_hz = tone->hp_hz;
    if (dry) {
        const float flutter = 0.80f + 0.20f * brush_walk(a, BWALK_FLUTTER, 11.0f + 5.0f * spd);
        level *= 0.75f * ink_clampf(flutter, 0.5f, 1.0f);
        hp_hz = 460.0f;
    }

    float mix = src * level;
    mix = brush_svf(a, BSVF_HP1, mix, hp_hz, 0.71f).hp;
    mix = brush_svf(a, BSVF_HP2, mix, hp_hz, 0.71f).hp;
    if (a->brush_surge_env > k_brush_silent) {
        mix += brush_surge_veil(a, spd, pink_b) * a->brush_env;
    }
    const float lp = tone->lp_lo + (tone->lp_hi - tone->lp_lo) * spd;
    mix = brush_svf(a, BSVF_LP1, mix, lp, 0.71f).lp;
    mix = brush_svf(a, BSVF_LP2, mix, lp * 1.30f, 0.71f).lp;
    mix = brush_svf(a, BSVF_LP3, mix, lp * 1.70f, 0.71f).lp;
    return mix * 0.28f;
}

bool audio_brush_validate_finite(const AudioState *a)
{
    if (!isfinite(a->brush_target) || !isfinite(a->brush_env) || !isfinite(a->brush_speed) ||
        !isfinite(a->brush_speed_smooth) || !isfinite(a->brush_hold_speed) ||
        !isfinite(a->brush_surge_env) || !audio_grain_finite(&a->brush_flow_grain) ||
        !audio_grain_finite(&a->brush_swish_grain)) {
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        if (!audio_pink_finite(&a->brush_pink[i])) {
            return false;
        }
    }
    for (int i = 0; i < AUDIO_BRUSH_WALK_COUNT; ++i) {
        if (!audio_walk_finite(&a->brush_walk[i])) {
            return false;
        }
    }
    for (int i = 0; i < AUDIO_BRUSH_SVF_COUNT; ++i) {
        if (!audio_svf_finite(&a->brush_svf[i])) {
            return false;
        }
    }
    for (int line = 0; line < 2; ++line) {
        for (int i = 0; i < AUDIO_BRUSH_DELAY_LEN; ++i) {
            if (!isfinite(a->brush_delay[line][i])) {
                return false;
            }
        }
    }
    return true;
}
