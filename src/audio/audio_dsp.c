#include "audio_dsp.h"

#include "audio.h"
#include "core.h"

#include <math.h>
#include <string.h>

static const float k_dsp_pi = 3.14159265359f;
static const float k_dsp_two_pi = 6.28318530718f;

AudioSvfCoeffs audio_svf_make_coeffs(float cutoff_hz, float q)
{
    const float fc = ink_clampf(cutoff_hz, 20.0f, 18000.0f);
    const float g = tanf(k_dsp_pi * fc / (float)AUDIO_SAMPLE_RATE);
    const float k = 1.0f / ink_clampf(q, 0.35f, 16.0f);
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    return (AudioSvfCoeffs){.k = k, .a1 = a1, .a2 = a2, .a3 = a3};
}

AudioSvfOut audio_svf_step_coeffs(AudioSvf *f, float in, AudioSvfCoeffs c)
{
    const float v3 = in - f->ic2eq;
    const float v1 = c.a1 * f->ic1eq + c.a2 * v3;
    const float v2 = f->ic2eq + c.a2 * f->ic1eq + c.a3 * v3;
    f->ic1eq = 2.0f * v1 - f->ic1eq;
    f->ic2eq = 2.0f * v2 - f->ic2eq;
    return (AudioSvfOut){.lp = v2, .bp = v1, .hp = in - c.k * v1 - v2};
}

AudioSvfOut audio_svf_step(AudioSvf *f, float in, float cutoff_hz, float q)
{
    return audio_svf_step_coeffs(f, in, audio_svf_make_coeffs(cutoff_hz, q));
}

float audio_pink_step(AudioPink *p, float white)
{
    p->b[0] = 0.99886f * p->b[0] + white * 0.0555179f;
    p->b[1] = 0.99332f * p->b[1] + white * 0.0750759f;
    p->b[2] = 0.96900f * p->b[2] + white * 0.1538520f;
    p->b[3] = 0.86650f * p->b[3] + white * 0.3104856f;
    p->b[4] = 0.55000f * p->b[4] + white * 0.5329522f;
    p->b[5] = -0.7616f * p->b[5] - white * 0.0168980f;
    const float out =
        (p->b[0] + p->b[1] + p->b[2] + p->b[3] + p->b[4] + p->b[5] + p->b[6] + white * 0.5362f) *
        0.11f;
    p->b[6] = white * 0.115926f;
    return out;
}

/* The raw two-pole output shrinks with sqrt(a), so a slow walk would barely move; scaling by the
 * cascade's exact noise gain keeps the spread near 0.5 at every rate. */
float audio_walk_step(AudioWalk *w, float white, float hz)
{
    const float a = ink_clampf(k_dsp_two_pi * hz / (float)AUDIO_SAMPLE_RATE, 1e-5f, 0.5f);
    w->y1 += a * (white - w->y1);
    w->y2 += a * (w->y1 - w->y2);
    const float r2 = (1.0f - a) * (1.0f - a);
    const float one_minus = 1.0f - r2;
    const float gain_sq = a * a * a * a * (1.0f + r2) / (one_minus * one_minus * one_minus);
    const float white_sd = 0.57735f;
    const float norm = 0.5f / (white_sd * sqrtf(gain_sq));
    return ink_clampf(w->y2 * norm, -1.5f, 1.5f);
}

float audio_smoothstep(float edge0, float edge1, float x)
{
    const float t = ink_clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void audio_grain_setup(AudioGrainLoop *g, size_t pcm_len, float start_frac, float end_frac,
                       float grain_samples)
{
    memset(g, 0, sizeof *g);
    if (!isfinite(start_frac) || !isfinite(end_frac) || !isfinite(grain_samples)) {
        return;
    }
    /* Endpoints are formed in double: a float product rounds UINT32_MAX up to 2^32, which does
     * not fit the uint32_t window. */
    const double len = (double)(pcm_len > (size_t)UINT32_MAX ? (size_t)UINT32_MAX : pcm_len);
    const double lo = fmin(fmax((double)start_frac, 0.0), 1.0);
    const double hi = fmin(fmax((double)end_frac, lo), 1.0);
    const double start = floor(len * lo);
    const double end = floor(len * hi);
    if (end - start < 64.0) {
        return;
    }
    const uint32_t span = (uint32_t)(end - start);
    const float grain_len = fminf(grain_samples, (float)span * 0.85f);
    if (grain_len < 8.0f) {
        return;
    }
    g->start = (uint32_t)start;
    g->span = span;
    g->grain_len = grain_len;
    g->ph[0] = 0.0f;
    g->ph[1] = 0.5f;
    g->base[0] = 0.0f;
    g->base[1] = 0.35f * ((float)span - grain_len - 8.0f);
}

static float pcm_at(const int16_t *win, uint32_t span, int64_t i)
{
    const int64_t c = i < 0 ? 0 : (i >= (int64_t)span ? (int64_t)span - 1 : i);
    return (float)win[c];
}

/* Catmull-Rom alone overshoots on sharp edges (a full-scale square pair peaks near 1.25), so the
 * result is held between the two samples it interpolates; that keeps every read in [-1, 1]. */
static float sample_hermite(const int16_t *win, uint32_t span, float pos)
{
    if (!isfinite(pos)) {
        return 0.0f;
    }
    const float fl = floorf(ink_clampf(pos, 0.0f, (float)span));
    const int64_t i = (int64_t)fl;
    const float t = ink_clampf(pos - fl, 0.0f, 1.0f);
    const float y0 = pcm_at(win, span, i - 1);
    const float y1 = pcm_at(win, span, i);
    const float y2 = pcm_at(win, span, i + 1);
    const float y3 = pcm_at(win, span, i + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    const float y = ((c3 * t + c2) * t + c1) * t + y1;
    return ink_clampf(y, fminf(y1, y2), fmaxf(y1, y2));
}

static bool grain_window_fits(const AudioGrainLoop *g, size_t pcm_len)
{
    return g->span >= 64u && g->start <= pcm_len && g->span <= pcm_len - g->start &&
           isfinite(g->grain_len) && g->grain_len >= 8.0f && g->grain_len <= (float)g->span;
}

float audio_grain_step(AudioGrainLoop *g, const int16_t *pcm, size_t pcm_len, float rate,
                       uint64_t *rng)
{
    if (pcm == NULL || !grain_window_fits(g, pcm_len)) {
        return 0.0f;
    }
    const int16_t *win = pcm + g->start;
    const float safe_rate = isfinite(rate) ? ink_clampf(rate, 0.05f, 4.0f) : 1.0f;
    const float inc = safe_rate / g->grain_len;
    const float max_base = fmaxf((float)g->span - g->grain_len - 8.0f, 1.0f);
    float out = 0.0f;
    for (int h = 0; h < 2; ++h) {
        /* A phase outside [0, 1) would read outside the Hann window, so it restarts silent. */
        if (!(g->ph[h] >= 0.0f && g->ph[h] < 1.0f)) {
            g->ph[h] = 0.0f;
        }
        if (!(g->base[h] >= 0.0f && g->base[h] <= max_base)) {
            g->base[h] = isfinite(g->base[h]) ? ink_clampf(g->base[h], 0.0f, max_base) : 0.0f;
        }
        g->ph[h] += inc;
        if (g->ph[h] >= 1.0f) {
            g->ph[h] = fminf(g->ph[h] - 1.0f, 0.999f);
            g->base[h] = ink_rng_f01(rng) * max_base;
        }
        const float w = 0.5f - 0.5f * cosf(k_dsp_two_pi * g->ph[h]);
        out += sample_hermite(win, g->span, g->base[h] + g->ph[h] * g->grain_len) * w;
    }
    return out * (1.0f / 32768.0f);
}

float audio_pcm_read(const int16_t *pcm, size_t pcm_len, float pos, float fade)
{
    if (pcm == NULL || pcm_len < 4u || !isfinite(pos) || pos < 0.0f) {
        return 0.0f;
    }
    const uint32_t span = pcm_len > (size_t)UINT32_MAX ? UINT32_MAX : (uint32_t)pcm_len;
    const float last = (float)(span - 1u);
    if (pos >= last) {
        return 0.0f;
    }
    const float tail =
        isfinite(fade) && fade > 0.0f ? audio_smoothstep(0.0f, fade, last - pos) : 1.0f;
    return sample_hermite(pcm, span, pos) * tail * (1.0f / 32768.0f);
}

bool audio_svf_finite(const AudioSvf *f) { return isfinite(f->ic1eq) && isfinite(f->ic2eq); }

bool audio_pink_finite(const AudioPink *p)
{
    for (int i = 0; i < 7; ++i) {
        if (!isfinite(p->b[i])) {
            return false;
        }
    }
    return true;
}

bool audio_walk_finite(const AudioWalk *w) { return isfinite(w->y1) && isfinite(w->y2); }

bool audio_grain_finite(const AudioGrainLoop *g)
{
    return isfinite(g->grain_len) && isfinite(g->ph[0]) && isfinite(g->ph[1]) &&
           isfinite(g->base[0]) && isfinite(g->base[1]);
}
