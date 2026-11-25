#include "render_rules.h"

#include <math.h>
#include <stddef.h>

enum { POLYLINE_MIN_STEPS = 2, POLYLINE_MAX_STEPS = 24 };

int render_polyline_segment_steps(float seg_len)
{
    const float d = isfinite(seg_len) && seg_len > 0.0f ? seg_len : 0.0f;
    const int n = (int)ceilf(fminf(d, 4.0f * (float)POLYLINE_MAX_STEPS) / 4.0f);
    return n < POLYLINE_MIN_STEPS ? POLYLINE_MIN_STEPS : n;
}

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

bool render_fluid_use_gpu(bool post_enabled, unsigned int fluid_shader_id,
                          unsigned int paper_tex_id)
{
    return post_enabled && fluid_shader_id != 0u && paper_tex_id != 0u;
}

static unsigned char encode_density(float value, float max_dye)
{
    if (!(value > 0.0f)) {
        return 0u;
    }
    /* Divide instead of multiplying by 1 / max_dye: a tiny max_dye makes the reciprocal infinite.
     */
    const float ratio = value / max_dye;
    const float n = isfinite(ratio) ? clamp01(ratio) : 1.0f;
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
    for (int i = 0; i < FLUID_N; ++i) {
        unsigned char *px = rgba + (size_t)i * 4u;
        px[0] = encode_density(ink[i], max_dye);
        px[1] = encode_density(fire[i], max_dye);
        px[2] = encode_density(storm[i], max_dye);
        px[3] = solid[i] != 0u ? 255u : 0u;
    }
}

float render_fluid_decode_channel(unsigned char texel, float max_dye)
{
    const float n = (float)texel / 255.0f;
    return n * n * max_dye;
}

RenderRgba render_fluid_ink_curve(const RenderFluidPalette *pal, float ink, float grad)
{
    if (pal == NULL || !(ink >= 0.05f)) {
        return (RenderRgba){1.0f, 1.0f, 1.0f, 0.0f};
    }
    const float pale = smooth(0.05f, 0.25f, ink);
    const float heavy = smooth(0.25f, 0.50f, ink);
    const float dense = smooth(0.25f, 0.80f, ink);
    RenderRgba tone = mix(pal->sumi_clear, pal->sumi_light, pale);
    tone = mix(tone, pal->sumi_heavy, heavy);
    tone = mix(tone, pal->sumi_dark, smooth(0.50f, 0.72f, ink));
    tone = mix(tone, pal->sumi_scorch, smooth(0.72f, 0.80f, ink));

    /* Pooling stops where the tone reaches sumi_heavy, so mixing towards it only ever darkens. */
    const float pool =
        smooth(0.02f, 0.45f, (isfinite(grad) ? grad : 0.0f) * 0.55f) * (1.0f - heavy);
    tone = mix(tone, pal->sumi_heavy, pool * 0.60f);
    tone.a = 0.15f + 0.30f * pale + 0.47f * dense + 0.28f * pool;
    return tone;
}

RenderRgba render_fluid_cpu_ink(const RenderFluidPalette *pal, float ink, float grad)
{
    const RenderRgba t = render_fluid_ink_curve(pal, ink, grad);
    return (RenderRgba){1.0f + (t.r - 1.0f) * t.a, 1.0f + (t.g - 1.0f) * t.a,
                        1.0f + (t.b - 1.0f) * t.a, 1.0f};
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

static float usable_length(float len) { return isfinite(len) && len > 0.0f ? len : 0.0f; }

static int plan_by_arc_length(const float *seg_len, int segs, int budget, RenderPolySample *out)
{
    double total = 0.0;
    for (int s = 0; s < segs; ++s) {
        total += usable_length(seg_len[s]);
    }
    if (!(total > 0.0)) {
        for (int k = 0; k < budget; ++k) {
            out[k] = (RenderPolySample){(int32_t)(((long long)k * segs) / budget), 0.0f};
        }
        return budget;
    }
    int s = 0;
    double seg_start = 0.0;
    for (int k = 0; k < budget; ++k) {
        const double target = total * (double)k / (double)budget;
        while (s < segs - 1 && seg_start + usable_length(seg_len[s]) <= target) {
            seg_start += usable_length(seg_len[s]);
            ++s;
        }
        const double len = usable_length(seg_len[s]);
        const double t = len > 0.0 ? (target - seg_start) / len : 0.0;
        out[k] = (RenderPolySample){(int32_t)s, (float)(t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t))};
    }
    return budget;
}

int render_polyline_plan(const float *seg_len, int segs, int cap, RenderPolySample *out)
{
    if (seg_len == NULL || out == NULL || segs <= 0 || cap < 2) {
        return 0;
    }
    const int budget = cap - 1;
    if (segs >= budget) {
        return plan_by_arc_length(seg_len, segs, budget, out);
    }
    int total = 0;
    for (int s = 0; s < segs; ++s) {
        total += render_polyline_segment_steps(seg_len[s]);
    }
    /* Every segment keeps its start sample; any spare budget is shared in proportion to demand. */
    const int spare = budget - segs;
    const int demand = total - segs;
    int n = 0;
    for (int s = 0; s < segs; ++s) {
        int steps = render_polyline_segment_steps(seg_len[s]);
        if (total > budget) {
            steps = 1 + (int)(((long long)(steps - 1) * spare) / demand);
        }
        for (int k = 0; k < steps; ++k) {
            out[n++] = (RenderPolySample){(int32_t)s, (float)k / (float)steps};
        }
    }
    return n;
}
