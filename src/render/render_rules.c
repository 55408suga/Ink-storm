#include "render_rules.h"

#include <math.h>
#include <stddef.h>

enum { POLYLINE_MIN_STEPS = 2, POLYLINE_MAX_STEPS = 24 };

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static float smooth(float lo, float hi, float x)
{
    const float t = clamp01((x - lo) / (hi - lo));
    return t * t * (3.0f - 2.0f * t);
}

static RenderRgba mix(RenderRgba a, RenderRgba b, float t)
{
    const float u = clamp01(t);
    return (RenderRgba){a.r + (b.r - a.r) * u, a.g + (b.g - a.g) * u, a.b + (b.b - a.b) * u,
                        a.a + (b.a - a.a) * u};
}

BgKey render_bg_key(bool combat_world, int32_t act_index)
{
    if (!combat_world) {
        return (BgKey){.kind = BG_LOBBY, .act_index = 0};
    }
    return (BgKey){.kind = BG_ACT, .act_index = act_index >= 0 ? act_index : 0};
}

bool render_bg_key_equal(BgKey a, BgKey b)
{
    return a.kind == b.kind && a.act_index == b.act_index;
}

bool render_fluid_use_gpu(bool post_enabled, unsigned int fluid_shader_id)
{
    return post_enabled && fluid_shader_id != 0u;
}

static unsigned char encode_density(float value, float inv_max)
{
    if (!(value > 0.0f)) {
        return 0u;
    }
    const float n = clamp01(value * inv_max);
    return (unsigned char)(sqrtf(n) * 255.0f + 0.5f);
}

void render_fluid_encode_field(const FluidState *f, unsigned char *rgba)
{
    if (f == NULL || rgba == NULL) {
        return;
    }
    const float *ink = fluid_channel_const(f, 0);
    const float *fire = fluid_channel_const(f, 1);
    const float *storm = fluid_channel_const(f, 2);
    const uint8_t *solid = fluid_solid_const(f);
    const float max_dye = isfinite(f->cfg.max_dye) && f->cfg.max_dye > 0.0f ? f->cfg.max_dye : 1.0f;
    const float inv_max = 1.0f / max_dye;
    for (int i = 0; i < FLUID_N; ++i) {
        unsigned char *px = rgba + (size_t)i * 4u;
        px[0] = encode_density(ink[i], inv_max);
        px[1] = encode_density(fire[i], inv_max);
        px[2] = encode_density(storm[i], inv_max);
        px[3] = solid[i] != 0u ? 255u : 0u;
    }
}

float render_fluid_decode_channel(unsigned char texel, float max_dye)
{
    const float n = (float)texel / 255.0f;
    return n * n * max_dye;
}

RenderRgba render_fluid_cpu_ink(const RenderFluidPalette *pal, float ink, float grad)
{
    const RenderRgba white = {1.0f, 1.0f, 1.0f, 1.0f};
    if (pal == NULL || !(ink > 0.0f)) {
        return white;
    }
    const float d = fminf(ink, 3.0f);
    const float cover = smooth(0.03f, 0.30f, d);
    const float depth = smooth(0.12f, 1.70f, d);

    RenderRgba tone = mix(pal->sumi_clear, pal->sumi_light, smooth(0.0f, 0.30f, depth));
    tone = mix(tone, pal->sumi_heavy, smooth(0.25f, 0.62f, depth));
    tone = mix(tone, pal->sumi_dark, smooth(0.55f, 0.88f, depth));
    tone = mix(tone, pal->sumi_scorch, smooth(0.85f, 1.0f, depth));

    /* Wet-edge pooling grows continuously with the density gradient; dense cores barely pool. */
    const float pool = smooth(0.02f, 0.45f, (isfinite(grad) ? grad : 0.0f) * 0.55f) *
                       (1.0f - 0.65f * depth) * cover;
    tone = mix(tone, pal->sumi_dark, pool * 0.60f);
    const float alpha = fminf(0.94f, cover * (0.30f + 0.62f * depth) + pool * 0.28f);
    return (RenderRgba){1.0f + (tone.r - 1.0f) * alpha, 1.0f + (tone.g - 1.0f) * alpha,
                        1.0f + (tone.b - 1.0f) * alpha, 1.0f};
}

RenderRgba render_fluid_cpu_elem(const RenderFluidPalette *pal, float fire, float storm)
{
    const RenderRgba clear = {0.0f, 0.0f, 0.0f, 0.0f};
    if (pal == NULL) {
        return clear;
    }
    const float f = isfinite(fire) ? fmaxf(fire, 0.0f) : 0.0f;
    const float s = isfinite(storm) ? fmaxf(storm, 0.0f) : 0.0f;
    const float af = smooth(0.05f, 0.50f, f) * 0.88f;
    const float as = smooth(0.05f, 0.46f, s) * 0.88f;
    const float sum = af + as;
    if (sum <= 1e-4f) {
        return clear;
    }
    const RenderRgba fire_c = mix(pal->shu, pal->shu_hot, smooth(0.32f, 0.95f, f));
    const RenderRgba storm_c = mix(pal->ao, pal->ao_hot, smooth(0.30f, 0.90f, s));
    RenderRgba c = mix(storm_c, fire_c, af / sum);
    /* Where fire and storm overlap the glow turns gold instead of one hiding the other. */
    const float gold = smooth(0.0f, 0.55f, fminf(af, as)) * 0.75f;
    c = mix(c, pal->kin_hot, gold);
    const float a = 1.0f - (1.0f - af) * (1.0f - as);
    return (RenderRgba){c.r * a, c.g * a, c.b * a, a};
}

int render_polyline_plan(const float *seg_len, int segs, int cap, int *steps)
{
    if (seg_len == NULL || steps == NULL || segs <= 0 || cap < 2) {
        return 0;
    }
    int total = 0;
    for (int s = 0; s < segs; ++s) {
        const float d = isfinite(seg_len[s]) && seg_len[s] > 0.0f ? seg_len[s] : 0.0f;
        const float wanted = ceilf(fminf(d, 4.0f * (float)POLYLINE_MAX_STEPS) / 4.0f);
        int n = (int)wanted;
        n = n < POLYLINE_MIN_STEPS ? POLYLINE_MIN_STEPS : n;
        steps[s] = n;
        total += n;
    }
    const int budget = cap - 1;
    if (total <= budget) {
        return total + 1;
    }
    if (segs >= budget) {
        for (int s = 0; s < segs; ++s) {
            steps[s] = s < budget ? 1 : 0;
        }
        return budget + 1;
    }
    /* Every segment keeps one sample; the spare budget is shared in proportion to its demand. */
    const int spare = budget - segs;
    const int demand = total - segs;
    int used = 0;
    for (int s = 0; s < segs; ++s) {
        steps[s] = 1 + (int)(((long long)(steps[s] - 1) * spare) / demand);
        used += steps[s];
    }
    return used + 1;
}
