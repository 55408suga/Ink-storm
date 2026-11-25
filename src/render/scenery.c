#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

/* Pre-baked sumi-e landscapes framing the battlefield. Everything here is drawn once into the
 * opaque background cache, so vertex counts only affect the bake, never the frame budget. The
 * centre of the screen is kept to pale mist so ink, fire, storm and enemies stay legible. */

static const float k_pi = 3.14159265359f;

static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static float h01(uint32_t seed, uint32_t i)
{
    return (float)(mix32(seed * 0x9e3779b9u ^ mix32(i + 0x632be5abu)) >> 8) * (1.0f / 16777216.0f);
}

static float hs(uint32_t seed, uint32_t i) { return h01(seed, i) * 2.0f - 1.0f; }

static float vnoise(uint32_t seed, float x)
{
    const float fl = floorf(x);
    const float t = x - fl;
    const float u = t * t * (3.0f - 2.0f * t);
    const uint32_t i = (uint32_t)(int32_t)fl;
    return h01(seed, i) + (h01(seed, i + 1u) - h01(seed, i)) * u;
}

static float fbm(uint32_t seed, float x)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float norm = 0.0f;
    for (int o = 0; o < 4; ++o) {
        sum += vnoise(seed + (uint32_t)o * 131u, x) * amp;
        norm += amp;
        x *= 2.03f;
        amp *= 0.5f;
    }
    return sum / norm;
}

static Color tone_c(PaletteId id, float alpha) { return pal_color_alpha(id, alpha); }

static BrushProfile brush(float w, float min_ratio, float entry, float exit_r, float dry,
                          uint32_t seed)
{
    return (BrushProfile){.w_max = w,
                          .w_min_ratio = min_ratio,
                          .entry = entry,
                          .exit = exit_r,
                          .dry = dry,
                          .seed = seed};
}

static void stroke_n(RenderState *r, const Vector2 *pts, int n, BrushProfile p, PaletteId c,
                     float alpha)
{
    ink_stroke(r, pts, n, p, (InkTone){.color = c, .alpha = fminf(alpha, 1.0f)}, 1.0f);
}

static Vector2 lerp_pt(const Vector2 *pts, int n, float t)
{
    const float f = ink_clampf(t, 0.0f, 1.0f) * (float)(n - 1);
    const int i = f >= (float)(n - 1) ? n - 2 : (int)f;
    const float u = f - (float)i;
    return (Vector2){pts[i].x + (pts[i + 1].x - pts[i].x) * u,
                     pts[i].y + (pts[i + 1].y - pts[i].y) * u};
}

/* ---------------------------------------------------------------- mist */

static void mist_band(RenderState *r, float y0, float y1, float alpha_mid)
{
    const float l = -(float)RENDER_BG_PAD;
    const float rgt = (float)(INK_SCREEN_W + RENDER_BG_PAD);
    const float ym = (y0 + y1) * 0.5f;
    const Vector2 a[2] = {{l, y0}, {rgt, y0}};
    const Vector2 m[2] = {{l, ym}, {rgt, ym}};
    const Vector2 b[2] = {{l, y1}, {rgt, y1}};
    ink_fill_gradient_strip(r, a, m, 2, tone_c(PAL_GOFUN, 0.0f), tone_c(PAL_GOFUN, alpha_mid));
    ink_fill_gradient_strip(r, m, b, 2, tone_c(PAL_GOFUN, alpha_mid), tone_c(PAL_GOFUN, 0.0f));
}

static void mist_puff(RenderState *r, Vector2 c, float rx, float ry, float alpha)
{
    ink_radial_glow(r, c, rx, ry, tone_c(PAL_GOFUN, alpha), tone_c(PAL_GOFUN, 0.0f));
}

/* Rolling cloud-mist: overlapping puffs at varying heights with faint grey undersides, so the
 * layer reads as drifting cloud instead of a ruled band. */
static void mist_drift(RenderState *r, float y, float spread, float alpha, uint32_t seed)
{
    mist_band(r, y - spread * 0.5f, y + spread * 0.5f, alpha * 0.3f);
    const int puffs = 16;
    const float step = ((float)INK_SCREEN_W + 2.0f * (float)RENDER_BG_PAD + 120.0f) / (float)puffs;
    for (int k = 0; k < puffs; ++k) {
        const float x = -(float)RENDER_BG_PAD - 60.0f +
                        ((float)k + 0.2f + 0.6f * h01(seed, (uint32_t)k)) * step;
        const float yy = y + hs(seed + 1u, (uint32_t)k) * spread * 0.45f;
        const float rx = step * (0.9f + 0.9f * h01(seed + 2u, (uint32_t)k));
        const float ry = spread * (0.28f + 0.30f * h01(seed + 3u, (uint32_t)k));
        if (h01(seed + 5u, (uint32_t)k) > 0.45f) {
            ink_radial_glow(r, (Vector2){x + rx * 0.08f, yy + ry * 0.55f}, rx * 0.75f, ry * 0.45f,
                            tone_c(PAL_SUMI_CLEAR, alpha * 0.14f), tone_c(PAL_SUMI_CLEAR, 0.0f));
        }
        mist_puff(r, (Vector2){x, yy}, rx, ry,
                  alpha * (0.55f + 0.45f * h01(seed + 4u, (uint32_t)k)));
    }
}

static void centre_mist(RenderState *r, float alpha)
{
    /* Lifts anything that drifted toward the battlefield back to pale paper. */
    ink_radial_glow(r, (Vector2){640.0f, 384.0f}, 540.0f, 290.0f, tone_c(PAL_GOFUN, alpha),
                    tone_c(PAL_GOFUN, 0.0f));
}

/* ---------------------------------------------------------------- mountains */

typedef struct {
    float cx, base_y; /* summit column and the line where the foot dissolves */
    float height, half_w;
    PaletteId tone;
    float alpha;
    float contour; /* crisp ridge line width (0 = none) */
    uint32_t seed;
} PeakSpec;

/* One mountain: summit-weighted wash that dissolves into mist at its feet, hemp-fibre texture
 * strokes down the slopes, moss dots along the crest and an optional crisp ridge line. */
static void draw_peak(RenderState *r, PeakSpec s)
{
    enum { N = 30 };
    Vector2 top[N];
    Vector2 bottom[N];
    Color top_c[N];
    Color bottom_c[N];
    float prof[N];
    const float sharp = 1.1f + 0.9f * h01(s.seed, 0u);
    const float skew = hs(s.seed, 1u) * 0.28f;
    const float fade = 36.0f + s.height * 0.55f;
    for (int i = 0; i < N; ++i) {
        const float t = (float)i / (float)(N - 1);
        const float u = t * 2.0f - 1.0f;
        const float d = u < skew ? (u - skew) / (1.0f + skew) : (u - skew) / (1.0f - skew);
        /* cosf at d = +-1 rounds slightly below zero, and powf of a negative base is NaN. */
        const float base = powf(fmaxf(0.0f, cosf(d * k_pi * 0.5f)), sharp);
        const float rough = (fbm(s.seed + 2u, t * 9.0f) - 0.5f) * 0.42f * base;
        prof[i] = ink_clampf(base + rough, 0.0f, 1.08f);
        top[i] = (Vector2){s.cx + u * s.half_w, s.base_y - s.height * prof[i]};
        /* An uneven foot keeps neighbouring ranges from ending on one straight line. */
        bottom[i] =
            (Vector2){top[i].x, s.base_y + fade * (0.45f + 0.75f * fbm(s.seed + 11u, t * 4.0f))};
        top_c[i] = tone_c(s.tone, s.alpha * fminf(prof[i] * 1.8f, 1.0f));
        bottom_c[i] = tone_c(s.tone, 0.0f);
    }
    ink_fill_strip_shaded(r, top, bottom, top_c, bottom_c, N);
    /* Lit flank: a pale veil over the side facing the light (left). */
    Vector2 lit_b[N];
    Color lit_t[N];
    Color lit_bc[N];
    for (int i = 0; i < N; ++i) {
        lit_b[i] = (Vector2){top[i].x, top[i].y + s.height * 0.6f * prof[i] + 8.0f};
        const float left =
            ink_clampf(((float)i / (float)(N - 1) * 2.0f - 1.0f - skew) * -2.0f, 0.0f, 1.0f);
        lit_t[i] = tone_c(PAL_GOFUN, 0.32f * left * prof[i]);
        lit_bc[i] = tone_c(PAL_GOFUN, 0.0f);
    }
    ink_fill_strip_shaded(r, top, lit_b, lit_t, lit_bc, N);
    /* Darker crest: ink collects along the summit line before the slope dissolves. */
    Vector2 crest_b[N];
    Color crest_t[N];
    Color crest_bc[N];
    for (int i = 0; i < N; ++i) {
        crest_b[i] = (Vector2){top[i].x, top[i].y + 6.0f + s.height * 0.22f * prof[i]};
        crest_t[i] = tone_c(s.tone, s.alpha * 0.9f * fminf(prof[i] * 2.0f, 1.0f));
        crest_bc[i] = tone_c(s.tone, 0.0f);
    }
    ink_fill_strip_shaded(r, top, crest_b, crest_t, crest_bc, N);

    const int strokes = 6 + (int)(s.height / 10.0f);
    for (int k = 0; k < strokes; ++k) {
        const float t = 0.12f + 0.76f * h01(s.seed + 3u, (uint32_t)k);
        const Vector2 p = lerp_pt(top, N, t);
        const float local_h = s.base_y - p.y;
        if (local_h < 10.0f) {
            continue;
        }
        const float side = (t * 2.0f - 1.0f) < skew ? -1.0f : 1.0f;
        const float len = local_h * (0.30f + 0.35f * h01(s.seed + 4u, (uint32_t)k));
        const Vector2 pts[3] = {{p.x, p.y + 2.0f},
                                {p.x + side * len * 0.18f, p.y + len * 0.5f},
                                {p.x + side * len * 0.42f, p.y + len}};
        stroke_n(
            r, pts, 3,
            brush(1.4f + s.contour * 0.45f, 0.25f, 0.08f, 0.6f, 0.75f, s.seed + 30u + (uint32_t)k),
            s.tone, s.alpha * 1.15f);
    }
    if (s.contour > 0.0f) {
        Vector2 line[N];
        int n = 0;
        for (int i = 0; i < N; ++i) {
            if (prof[i] > 0.08f) {
                line[n++] = top[i];
            }
        }
        if (n >= 2) {
            stroke_n(r, line, n, brush(s.contour, 0.22f, 0.12f, 0.16f, 0.5f, s.seed + 7u), s.tone,
                     s.alpha * 2.4f);
        }
        for (int k = 0; k < 7; ++k) {
            const Vector2 p = lerp_pt(top, N, 0.25f + 0.5f * h01(s.seed + 8u, (uint32_t)k));
            ink_wash(r, (Vector2){p.x, p.y + 3.0f}, 2.2f + s.contour * 0.5f, 1.6f, 0.4f,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = fminf(0.9f, s.alpha * 2.4f)}, 1,
                     s.seed + 50u + (uint32_t)k);
        }
        /* Distant conifer clusters standing in the mist on the slopes. */
        const int clusters = 1 + (int)(s.half_w / 110.0f);
        for (int k = 0; k < clusters; ++k) {
            const float t = 0.22f + 0.56f * h01(s.seed + 60u, (uint32_t)k);
            const Vector2 p = lerp_pt(top, N, t);
            for (int l = 0; l < 4; ++l) {
                const uint32_t id = (uint32_t)(k * 4 + l);
                const float tx = p.x + ((float)l - 1.5f) * 7.0f + hs(s.seed + 61u, id) * 2.5f;
                const float ty = p.y + 5.0f + h01(s.seed + 62u, id) * 5.0f;
                const float th = 10.0f + 7.0f * h01(s.seed + 63u, id);
                const float tw = th * 0.34f;
                const Vector2 tri[3] = {{tx, ty - th}, {tx + tw, ty}, {tx - tw, ty}};
                ink_fill_poly(r, tri, 3, tone_c(PAL_SUMI_HEAVY, fminf(0.7f, s.alpha * 1.9f)));
            }
        }
    }
}

/* ---------------------------------------------------------------- stone columns */

/* Tall weathered stone standing at the screen edge, filled right up to a rounded, slightly
 * off-centre summit: pale lit face, vertical veins, one contour that is heavier on the shadow side,
 * and a foot that dissolves into mist. */
static void draw_stone_column(RenderState *r, float cx, float y_top, float y_bot, float w,
                              float alpha, uint32_t seed)
{
    enum { N = 18 };
    Vector2 left[N];
    Vector2 right[N];
    Color lc[N];
    Color rc[N];
    float shape[N];
    const float dome = 0.10f + 0.06f * h01(seed, 9u); /* share of the height spent widening */
    const float apex_off = hs(seed, 8u) * w * 0.16f;
    for (int k = 0; k < N; ++k) {
        /* Rows bunch up near the summit so the rounded crown stays smooth. */
        const float t = powf((float)k / (float)(N - 1), 1.7f);
        const float y = y_top + (y_bot - y_top) * t;
        const float grow = sqrtf(fminf(t / dome, 1.0f));
        const float bulge = sinf(t * k_pi) * 0.08f;
        shape[k] = grow * (1.0f + bulge);
        const float xc = cx + apex_off * (1.0f - grow);
        const float lw = (fbm(seed, t * 3.0f) - 0.5f) * w * 0.28f * grow;
        const float rw = (fbm(seed + 1u, t * 3.0f) - 0.5f) * w * 0.28f * grow;
        left[k] = (Vector2){xc - w * 0.5f * shape[k] + lw, y};
        right[k] = (Vector2){xc + w * 0.5f * shape[k] + rw, y};
        const float foot = 1.0f - ink_clampf((t - 0.62f) / 0.38f, 0.0f, 1.0f);
        lc[k] = tone_c(PAL_SUMI_CLEAR, alpha * 0.95f * foot);
        rc[k] = tone_c(PAL_SUMI_LIGHT, alpha * 0.92f * foot);
    }
    ink_fill_strip_shaded(r, left, right, lc, rc, N);
    /* Shadow face: a darker band hugging the right edge. */
    Vector2 inner[N];
    Color ic[N];
    Color sc[N];
    for (int k = 0; k < N; ++k) {
        const float t = (right[k].y - y_top) / (y_bot - y_top);
        inner[k] = (Vector2){right[k].x - w * 0.32f * shape[k], right[k].y};
        const float foot = 1.0f - ink_clampf((t - 0.55f) / 0.45f, 0.0f, 1.0f);
        ic[k] = tone_c(PAL_SUMI_HEAVY, 0.0f);
        sc[k] = tone_c(PAL_SUMI_HEAVY, alpha * 0.55f * foot);
    }
    ink_fill_strip_shaded(r, inner, right, ic, sc, N);
    ink_wash(r, (Vector2){cx + apex_off * 0.5f - w * 0.12f, y_top + (y_bot - y_top) * 0.05f},
             w * 0.22f, w * 0.12f, -0.4f, (InkTone){.color = PAL_GOFUN, .alpha = alpha * 0.45f}, 1,
             seed + 6u);

    /* Vertical veins (wrinkle strokes) like weathered limestone, starting below the crown. */
    const int veins = 3 + (int)(w / 40.0f);
    for (int v = 0; v < veins; ++v) {
        const float off =
            -0.36f + 0.72f * ((float)v + 0.5f) / (float)veins + hs(seed + 2u, (uint32_t)v) * 0.06f;
        const float t0 = 0.22f + 0.2f * h01(seed + 3u, (uint32_t)v);
        const float t1 = fminf(0.95f, t0 + 0.30f + 0.30f * h01(seed + 4u, (uint32_t)v));
        Vector2 pts[5];
        for (int k = 0; k < 5; ++k) {
            const float t = t0 + (t1 - t0) * (float)k / 4.0f;
            const Vector2 a = lerp_pt(left, N, t);
            const Vector2 b = lerp_pt(right, N, t);
            const float wob = sinf(t * 17.0f + (float)v * 2.1f) * w * 0.05f;
            pts[k] = (Vector2){a.x + (b.x - a.x) * (0.5f + off) + wob, a.y};
        }
        stroke_n(r, pts, 5,
                 brush(2.4f + w * 0.035f, 0.2f, 0.2f, 0.45f, 0.7f, seed + 10u + (uint32_t)v),
                 PAL_SUMI_HEAVY, alpha * (0.42f + 0.25f * h01(seed + 5u, (uint32_t)v)));
    }
    /* Moss dots along the crown. */
    for (int k = 0; k < 5; ++k) {
        const Vector2 p = lerp_pt(k % 2 == 0 ? left : right, N, 0.08f + 0.05f * (float)k);
        ink_wash(r, (Vector2){p.x + (k % 2 == 0 ? 3.0f : -3.0f), p.y + 2.0f}, 2.6f, 1.8f, 0.3f,
                 (InkTone){.color = PAL_SUMI_DARK, .alpha = fminf(0.9f, alpha * 1.1f)}, 1,
                 seed + 40u + (uint32_t)k);
    }
    /* One contour over the summit: thin on the lit left, heavy on the shadowed right, both ends
     * lifting off before the foot dissolves. */
    enum { C = 13 };
    Vector2 contour[C * 2 - 1];
    int n = 0;
    for (int k = C - 1; k >= 0; --k) {
        contour[n++] = left[k];
    }
    for (int k = 1; k < C; ++k) {
        contour[n++] = right[k];
    }
    ink_outline_tone(r, contour, n, false, 5.6f + w * 0.03f, (Vector2){0.95f, 0.3f},
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = fminf(1.0f, alpha * 1.1f)},
                     seed + 20u);
}

/* ---------------------------------------------------------------- trees */

static void pine_pad(RenderState *r, Vector2 c, float rad, float alpha, uint32_t seed)
{
    ink_wash(r, (Vector2){c.x, c.y + rad * 0.18f}, rad * 1.1f, rad * 0.40f, hs(seed, 1u) * 0.15f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = alpha * 0.5f}, 2, seed);
    for (int layer = 0; layer < 2; ++layer) {
        const int needles = 13;
        const Vector2 base = {c.x + (float)layer * rad * 0.18f,
                              c.y + rad * (0.20f + 0.08f * (float)layer)};
        const float lr = rad * (layer == 0 ? 1.0f : 0.78f);
        for (int k = 0; k < needles; ++k) {
            const float t = (float)k / (float)(needles - 1);
            const float ang =
                k_pi * (1.05f + 0.90f * t) + hs(seed + 2u + (uint32_t)layer, (uint32_t)k) * 0.07f;
            const float len = lr * (0.80f + 0.32f * h01(seed + 3u + (uint32_t)layer, (uint32_t)k));
            const Vector2 pts[2] = {base,
                                    {base.x + cosf(ang) * len, base.y + sinf(ang) * len * 0.70f}};
            stroke_n(
                r, pts, 2,
                brush(1.6f, 0.35f, 0.02f, 0.72f, 0.2f, seed + 10u + (uint32_t)(k + layer * 20)),
                layer == 0 ? PAL_SUMI_DARK : PAL_SUMI_SCORCH, alpha * (layer == 0 ? 0.8f : 0.9f));
        }
    }
}

/* lean > 0 grows to the right; scale 1 is roughly 250 px tall. */
static void draw_pine(RenderState *r, Vector2 root, float scale, float lean, float alpha,
                      uint32_t seed)
{
    enum { T = 9 };
    Vector2 trunk[T];
    const float phase = h01(seed, 1u) * 6.0f;
    for (int k = 0; k < T; ++k) {
        const float t = (float)k / (float)(T - 1);
        trunk[k] = (Vector2){
            root.x + lean * scale * (t * t * 130.0f + sinf(t * 5.4f + phase) * 26.0f),
            root.y - t * 250.0f * scale,
        };
    }
    stroke_n(r, trunk, T, brush(19.0f * scale, 0.28f, 0.03f, 0.35f, 0.5f, seed + 2u),
             PAL_SUMI_HEAVY, alpha * 0.85f);
    Vector2 edge[T];
    for (int k = 0; k < T; ++k) {
        edge[k] = (Vector2){trunk[k].x + 5.5f * scale, trunk[k].y};
    }
    stroke_n(r, edge, T, brush(6.0f * scale, 0.3f, 0.05f, 0.45f, 0.6f, seed + 3u), PAL_SUMI_SCORCH,
             alpha * 0.9f);
    for (int k = 0; k < 9; ++k) {
        const Vector2 p = lerp_pt(trunk, T, 0.08f + 0.09f * (float)k);
        const float w = 7.0f * scale;
        const Vector2 arc[3] = {
            {p.x - w, p.y - 1.0f}, {p.x, p.y + 3.5f * scale}, {p.x + w * 0.8f, p.y}};
        stroke_n(r, arc, 3, brush(1.7f, 0.3f, 0.2f, 0.4f, 0.55f, seed + 40u + (uint32_t)k),
                 PAL_SUMI_SCORCH, alpha * 0.65f);
    }

    static const float k_at[6] = {0.34f, 0.47f, 0.60f, 0.72f, 0.84f, 0.97f};
    for (int b = 0; b < 6; ++b) {
        const Vector2 s = lerp_pt(trunk, T, k_at[b]);
        const float toward = lean >= 0.0f ? 1.0f : -1.0f;
        const float side = (b % 3 == 1) ? -toward * 0.6f : toward;
        const float len = (90.0f + 70.0f * h01(seed + 5u, (uint32_t)b)) * scale * fabsf(side) *
                          (1.0f - 0.25f * k_at[b]);
        const float droop = (hs(seed + 6u, (uint32_t)b) * 0.22f + 0.05f) * len;
        const Vector2 pts[4] = {
            s,
            {s.x + side * len * 0.30f, s.y - 14.0f * scale},
            {s.x + side * len * 0.66f, s.y + droop * 0.4f},
            {s.x + side * len, s.y + droop - 6.0f * scale},
        };
        stroke_n(r, pts, 4,
                 brush(8.0f * scale, 0.22f, 0.04f, 0.55f, 0.5f, seed + 60u + (uint32_t)b),
                 PAL_SUMI_DARK, alpha * 0.9f);
        pine_pad(r, pts[3], 34.0f * scale, alpha, seed + 80u + (uint32_t)b * 7u);
        pine_pad(r, (Vector2){pts[2].x, pts[2].y - 10.0f * scale}, 26.0f * scale, alpha * 0.9f,
                 seed + 120u + (uint32_t)b * 7u);
    }
    pine_pad(r, (Vector2){trunk[T - 1].x, trunk[T - 1].y - 8.0f * scale}, 38.0f * scale, alpha,
             seed + 200u);
}

static void draw_bamboo(RenderState *r, float x, float y_bot, float y_top, float lean, float w,
                        PaletteId tone, float alpha, uint32_t seed)
{
    float y = y_bot;
    int seg = 0;
    while (y > y_top && seg < 12) {
        const float seg_h = 74.0f + 40.0f * h01(seed, (uint32_t)seg);
        const float y_next = fmaxf(y_top, y - seg_h);
        const float t0 = (y_bot - y) / (y_bot - y_top);
        const float t1 = (y_bot - y_next) / (y_bot - y_top);
        const Vector2 seg_pts[2] = {{x + lean * t0, y - 3.0f}, {x + lean * t1, y_next + 3.0f}};
        stroke_n(r, seg_pts, 2, brush(w, 0.8f, 0.05f, 0.05f, 0.3f, seed + 10u + (uint32_t)seg),
                 tone, alpha);
        const Vector2 b = seg_pts[1];
        const Vector2 node[2] = {{b.x - w * 0.8f, b.y - 1.5f}, {b.x + w * 0.8f, b.y + 1.0f}};
        stroke_n(r, node, 2, brush(w * 0.5f, 0.4f, 0.2f, 0.3f, 0.2f, seed + 30u + (uint32_t)seg),
                 PAL_SUMI_DARK, alpha);
        if (seg > 0 && h01(seed + 2u, (uint32_t)seg) > 0.42f) {
            /* A spray of slender leaves (个 / 介 groupings). */
            const float dir = h01(seed + 4u, (uint32_t)seg) > 0.5f ? 1.0f : -1.0f;
            const int leaves = 3 + (int)(h01(seed + 7u, (uint32_t)seg) * 2.5f);
            for (int l = 0; l < leaves; ++l) {
                const uint32_t id = (uint32_t)(seg * 8 + l);
                const float ang = 0.15f + 0.30f * (float)l + hs(seed + 5u, id) * 0.12f;
                const float len = (34.0f + 18.0f * h01(seed + 6u, id)) * (0.55f + w / 12.0f);
                const Vector2 p0 = {b.x + dir * 2.0f, b.y + 3.0f};
                const Vector2 pts[3] = {
                    p0,
                    {p0.x + dir * cosf(ang) * len * 0.5f, p0.y + sinf(ang) * len * 0.42f},
                    {p0.x + dir * cosf(ang + 0.12f) * len, p0.y + sinf(ang + 0.12f) * len},
                };
                stroke_n(r, pts, 3,
                         brush(fmaxf(3.0f, w * 1.25f), 0.12f, 0.30f, 0.62f, 0.15f, seed + 50u + id),
                         tone, alpha * 1.05f);
            }
        }
        y = y_next;
        ++seg;
    }
}

/* ---------------------------------------------------------------- rocks, water, clouds */

static void draw_boulder(RenderState *r, Vector2 c, float rx, float ry, float alpha, uint32_t seed)
{
    enum { V = 10 };
    Vector2 poly[V];
    for (int i = 0; i < V; ++i) {
        const float a = k_pi * 2.0f * (float)i / (float)V + hs(seed, (uint32_t)i) * 0.16f;
        const float j = 0.84f + 0.26f * h01(seed + 1u, (uint32_t)i);
        const float flat = sinf(a) > 0.0f ? 0.62f : 1.0f;
        poly[i] = (Vector2){c.x + cosf(a) * rx * j, c.y + sinf(a) * ry * j * flat};
    }
    mist_puff(r, (Vector2){c.x, c.y + ry * 0.45f}, rx * 1.5f, ry * 0.9f, alpha * 0.4f);
    ink_fill_poly(r, poly, V, tone_c(PAL_SUMI_CLEAR, alpha * 0.95f));
    const Vector2 shade[6] = {
        {c.x + rx * 0.05f, c.y - ry * 0.1f}, poly[9], poly[0], poly[1], poly[2], poly[3]};
    ink_fill_poly(r, shade, 6, tone_c(PAL_SUMI_LIGHT, alpha * 0.75f));
    ink_wash(r, (Vector2){c.x - rx * 0.30f, c.y - ry * 0.35f}, rx * 0.38f, ry * 0.28f, 0.1f,
             (InkTone){.color = PAL_GOFUN, .alpha = alpha * 0.5f}, 1, seed + 3u);
    for (int k = 0; k < 5; ++k) {
        const float u = hs(seed + 4u, (uint32_t)k);
        const Vector2 p0 = {c.x + u * rx * 0.55f, c.y - ry * 0.55f + (float)k * ry * 0.18f};
        const Vector2 pts[3] = {
            p0, {p0.x + rx * 0.12f, p0.y + ry * 0.2f}, {p0.x + rx * 0.3f, p0.y + ry * 0.32f}};
        stroke_n(r, pts, 3,
                 brush(2.0f + rx * 0.02f, 0.25f, 0.1f, 0.6f, 0.75f, seed + 10u + (uint32_t)k),
                 PAL_SUMI_HEAVY, alpha * 0.7f);
    }
    ink_outline_tone(r, poly, V, true, 3.4f, (Vector2){0.6f, 0.8f},
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = alpha}, seed + 20u);
}

static void draw_waterfall(RenderState *r, float x, float y0, float y1, float w, uint32_t seed)
{
    /* The fall stays paper-white between darker wet rock; streaks thin out as it drops into mist.
     */
    const Vector2 l[2] = {{x - w * 0.5f, y0}, {x - w * 0.5f, y1}};
    const Vector2 rr[2] = {{x + w * 0.5f, y0}, {x + w * 0.5f, y1}};
    ink_fill_gradient_strip(r, l, rr, 2, tone_c(PAL_GOFUN, 0.85f), tone_c(PAL_GOFUN, 0.85f));
    for (int k = 0; k < 9; ++k) {
        const float sx = x - w * 0.42f + w * 0.84f * ((float)k + 0.5f) / 9.0f +
                         hs(seed + 7u, (uint32_t)k) * 2.0f;
        const float ys = y0 + (y1 - y0) * 0.15f * h01(seed + 10u, (uint32_t)k);
        const float ye = y1 - (y1 - y0) * 0.25f * h01(seed + 11u, (uint32_t)k);
        const Vector2 pts[3] = {
            {sx, ys}, {sx + hs(seed + 8u, (uint32_t)k) * 2.0f, (ys + ye) * 0.5f}, {sx, ye}};
        stroke_n(r, pts, 3,
                 brush(1.2f + 1.4f * h01(seed + 9u, (uint32_t)k), 0.3f, 0.1f, 0.4f, 0.85f,
                       seed + 70u + (uint32_t)k),
                 PAL_SUMI_LIGHT, 0.38f);
    }
    mist_puff(r, (Vector2){x, y1}, w * 2.6f, 42.0f, 0.95f);
    mist_puff(r, (Vector2){x + w * 0.8f, y1 + 18.0f}, w * 1.8f, 26.0f, 0.7f);
    for (int k = 0; k < 3; ++k) {
        const float rad = w * (1.1f + 0.6f * (float)k);
        ink_arc_stroke(r, (Vector2){x, y1 + 14.0f + (float)k * 7.0f}, rad, k_pi * 0.15f,
                       k_pi * 0.85f, brush(1.4f, 0.3f, 0.2f, 0.3f, 0.4f, seed + 90u + (uint32_t)k),
                       (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.5f - 0.12f * (float)k});
    }
}

static void draw_cloud(RenderState *r, Vector2 c, float w, float h, PaletteId tone, float alpha,
                       uint32_t seed)
{
    /* Billowing lobes: soft washes with darker undersides and a scalloped brush contour on top. */
    const int lobes = 5;
    for (int k = 0; k < lobes; ++k) {
        const float t = (float)k / (float)(lobes - 1);
        const float lx = c.x - w * 0.5f + w * t + hs(seed, (uint32_t)k) * w * 0.05f;
        const float lr = h * (0.42f + 0.30f * sinf(t * k_pi) + 0.12f * h01(seed + 1u, (uint32_t)k));
        const float ly = c.y + h * 0.15f - lr * 0.35f;
        ink_radial_glow(r, (Vector2){lx, ly + lr * 0.25f}, lr * 1.25f, lr * 0.9f,
                        tone_c(tone, alpha), tone_c(tone, 0.0f));
        ink_radial_glow(r, (Vector2){lx - lr * 0.2f, ly - lr * 0.2f}, lr * 0.8f, lr * 0.55f,
                        tone_c(PAL_GOFUN, alpha * 1.4f), tone_c(PAL_GOFUN, 0.0f));
        ink_arc_stroke(r, (Vector2){lx, ly}, lr, k_pi * 1.08f, k_pi * 1.92f,
                       brush(2.2f + h * 0.02f, 0.25f, 0.15f, 0.3f, 0.45f, seed + 20u + (uint32_t)k),
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = fminf(0.85f, alpha * 2.6f)});
    }
    const float cr = h * 0.26f;
    const Vector2 cc = {c.x + w * 0.5f + cr * 0.6f, c.y + h * 0.05f};
    ink_arc_stroke(r, cc, cr, k_pi * 0.9f, k_pi * 2.6f,
                   brush(2.0f, 0.25f, 0.1f, 0.6f, 0.4f, seed + 40u),
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = fminf(0.8f, alpha * 2.2f)});
}

static void draw_bird(RenderState *r, Vector2 c, float s, float alpha, uint32_t seed)
{
    const Vector2 wing[3] = {
        {c.x - s, c.y + s * 0.24f}, {c.x, c.y - s * 0.3f}, {c.x + s, c.y + s * 0.12f}};
    stroke_n(r, wing, 3, brush(fmaxf(2.0f, s * 0.24f), 0.22f, 0.25f, 0.45f, 0.25f, seed),
             PAL_SUMI_HEAVY, alpha);
}

static void draw_grass(RenderState *r, float bx, float by, float h, float lean, float alpha,
                       uint32_t seed)
{
    for (int k = 0; k < 6; ++k) {
        const float l = lean * (0.5f + 0.25f * (float)k) + hs(seed, (uint32_t)k) * 12.0f;
        const float hh = h * (0.6f + 0.4f * h01(seed + 1u, (uint32_t)k));
        const Vector2 pts[3] = {{bx + (float)k * 5.0f, by},
                                {bx + (float)k * 5.0f + l * 0.30f, by - hh * 0.55f},
                                {bx + (float)k * 5.0f + l, by - hh}};
        stroke_n(r, pts, 3, brush(3.0f, 0.2f, 0.1f, 0.65f, 0.45f, seed + 10u + (uint32_t)k),
                 PAL_SUMI_HEAVY, alpha);
    }
}

/* ---------------------------------------------------------------- compositions */

static void scene_lobby(RenderState *r)
{
    /* Moonlit range behind the title, a waterfall between stone columns on the left and a great
     * pine reaching in from the lower right. */
    mist_puff(r, (Vector2){930.0f, 96.0f}, 70.0f, 70.0f, 0.9f);
    ink_arc_stroke(r, (Vector2){930.0f, 96.0f}, 34.0f, 0.0f, k_pi * 2.0f,
                   brush(1.4f, 0.4f, 0.2f, 0.2f, 0.3f, 7002u),
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.35f});
    draw_peak(r, (PeakSpec){520.0f, 178.0f, 110.0f, 260.0f, PAL_SUMI_LIGHT, 0.20f, 0.0f, 7010u});
    draw_peak(r, (PeakSpec){1080.0f, 170.0f, 120.0f, 280.0f, PAL_SUMI_LIGHT, 0.20f, 0.0f, 7011u});
    draw_peak(r, (PeakSpec){760.0f, 196.0f, 92.0f, 200.0f, PAL_SUMI_LIGHT, 0.28f, 1.8f, 7012u});
    draw_peak(r, (PeakSpec){330.0f, 206.0f, 120.0f, 190.0f, PAL_SUMI_HEAVY, 0.30f, 2.8f, 7013u});
    draw_peak(r, (PeakSpec){1210.0f, 214.0f, 104.0f, 170.0f, PAL_SUMI_HEAVY, 0.30f, 2.8f, 7014u});
    mist_drift(r, 228.0f, 120.0f, 0.8f, 7050u);

    draw_stone_column(r, 26.0f, 40.0f, 620.0f, 64.0f, 0.75f, 7100u);
    draw_stone_column(r, 140.0f, 110.0f, 300.0f, 54.0f, 0.55f, 7101u);
    draw_waterfall(r, 76.0f, 60.0f, 600.0f, 22.0f, 7102u);
    draw_boulder(r, (Vector2){52.0f, 664.0f}, 80.0f, 38.0f, 0.8f, 7110u);
    draw_grass(r, 120.0f, 724.0f, 48.0f, 16.0f, 0.45f, 7130u);

    draw_pine(r, (Vector2){1300.0f, 760.0f}, 1.5f, -1.0f, 0.9f, 7200u);
    draw_boulder(r, (Vector2){1190.0f, 704.0f}, 84.0f, 32.0f, 0.75f, 7210u);
    for (int k = 0; k < 5; ++k) {
        draw_bird(r, (Vector2){400.0f + (float)k * 30.0f, 86.0f + (float)(k % 2) * 11.0f},
                  8.0f + (float)(k % 3), 0.5f, 7300u + (uint32_t)k);
    }
    centre_mist(r, 0.22f);
}

static void scene_act1(RenderState *r)
{
    /* Dawn shrine: layered peaks under a pale sun, waterfall mist at the upper right and an old
     * pine at the lower left. */
    ink_radial_glow(r, (Vector2){300.0f, 78.0f}, 64.0f, 64.0f, tone_c(PAL_GOFUN, 0.35f),
                    tone_c(PAL_GOFUN, 0.0f));
    ink_radial_glow(r, (Vector2){300.0f, 78.0f}, 24.0f, 24.0f, tone_c(PAL_GOFUN, 0.35f),
                    tone_c(PAL_SUMI_CLEAR, 0.30f));
    draw_peak(r, (PeakSpec){150.0f, 120.0f, 70.0f, 240.0f, PAL_SUMI_LIGHT, 0.18f, 0.0f, 1010u});
    draw_peak(r, (PeakSpec){620.0f, 116.0f, 64.0f, 300.0f, PAL_SUMI_LIGHT, 0.18f, 0.0f, 1011u});
    draw_peak(r, (PeakSpec){980.0f, 122.0f, 78.0f, 260.0f, PAL_SUMI_LIGHT, 0.18f, 0.0f, 1012u});
    draw_peak(r, (PeakSpec){420.0f, 138.0f, 70.0f, 200.0f, PAL_SUMI_LIGHT, 0.26f, 1.8f, 1013u});
    draw_peak(r, (PeakSpec){800.0f, 142.0f, 62.0f, 210.0f, PAL_SUMI_HEAVY, 0.28f, 2.4f, 1014u});
    draw_peak(r, (PeakSpec){40.0f, 150.0f, 80.0f, 170.0f, PAL_SUMI_HEAVY, 0.30f, 2.6f, 1015u});
    mist_drift(r, 162.0f, 86.0f, 0.75f, 1050u);

    draw_stone_column(r, 1166.0f, 70.0f, 330.0f, 58.0f, 0.33f, 1100u);
    draw_stone_column(r, 1262.0f, 50.0f, 420.0f, 70.0f, 0.35f, 1101u);
    draw_waterfall(r, 1214.0f, 84.0f, 330.0f, 22.0f, 1102u);

    draw_pine(r, (Vector2){-6.0f, 760.0f}, 1.05f, 1.0f, 0.9f, 1200u);
    draw_boulder(r, (Vector2){86.0f, 712.0f}, 70.0f, 26.0f, 0.75f, 1210u);
    draw_boulder(r, (Vector2){1214.0f, 706.0f}, 76.0f, 30.0f, 0.7f, 1211u);
    draw_grass(r, 1100.0f, 726.0f, 64.0f, -28.0f, 0.42f, 1220u);
    draw_grass(r, 250.0f, 726.0f, 52.0f, 24.0f, 0.36f, 1222u);
    centre_mist(r, 0.24f);
}

static void scene_act2(RenderState *r)
{
    /* Dusk bamboo gorge: far ridges and crows over two depths of bamboo on each side. */
    ink_radial_glow(r, (Vector2){1000.0f, 70.0f}, 86.0f, 54.0f, tone_c(PAL_SHU, 0.14f),
                    tone_c(PAL_SHU, 0.0f));
    draw_peak(r, (PeakSpec){420.0f, 124.0f, 60.0f, 300.0f, PAL_SUMI_LIGHT, 0.18f, 0.0f, 2010u});
    draw_peak(r, (PeakSpec){860.0f, 128.0f, 70.0f, 280.0f, PAL_SUMI_LIGHT, 0.18f, 0.0f, 2011u});
    draw_peak(r, (PeakSpec){640.0f, 140.0f, 50.0f, 180.0f, PAL_SUMI_HEAVY, 0.26f, 2.2f, 2012u});
    mist_drift(r, 152.0f, 80.0f, 0.7f, 2050u);

    static const float far_x[10] = {14.0f,   40.0f,   70.0f,   104.0f,  140.0f,
                                    1140.0f, 1176.0f, 1210.0f, 1240.0f, 1268.0f};
    for (int i = 0; i < 10; ++i) {
        draw_bamboo(r, far_x[i], 760.0f, 40.0f + 40.0f * h01(2100u, (uint32_t)i),
                    hs(2101u, (uint32_t)i) * 20.0f, 3.4f, PAL_SUMI_LIGHT, 0.32f,
                    2110u + (uint32_t)i * 37u);
    }
    mist_band(r, 380.0f, 560.0f, 0.35f);
    static const float near_x[6] = {10.0f, 58.0f, 98.0f, 1186.0f, 1226.0f, 1270.0f};
    for (int i = 0; i < 6; ++i) {
        draw_bamboo(r, near_x[i], 760.0f, 10.0f, hs(2201u, (uint32_t)i) * 24.0f,
                    (i % 2 == 0) ? 8.0f : 6.2f, PAL_SUMI_DARK, 0.72f, 2210u + (uint32_t)i * 41u);
    }
    mist_band(r, 590.0f, 730.0f, 0.4f);
    draw_boulder(r, (Vector2){96.0f, 696.0f}, 96.0f, 36.0f, 0.85f, 2300u);
    draw_boulder(r, (Vector2){1190.0f, 700.0f}, 100.0f, 34.0f, 0.85f, 2310u);

    static const float crow_x[7] = {240.0f, 285.0f, 340.0f, 510.0f, 820.0f, 880.0f, 945.0f};
    static const float crow_y[7] = {84.0f, 70.0f, 92.0f, 76.0f, 82.0f, 66.0f, 90.0f};
    for (int c = 0; c < 7; ++c) {
        draw_bird(r, (Vector2){crow_x[c], crow_y[c]}, 11.0f + (float)(c % 3) * 3.0f, 0.55f,
                  2400u + (uint32_t)c * 13u);
    }
    centre_mist(r, 0.24f);
}

static void scene_act3(RenderState *r)
{
    /* Storm-wracked spirit peak: jagged summits under churning ink clouds, stone columns on both
     * edges and a wind-bent pine clinging to the right. */
    draw_peak(r, (PeakSpec){260.0f, 140.0f, 96.0f, 170.0f, PAL_SUMI_HEAVY, 0.30f, 2.4f, 3010u});
    draw_peak(r, (PeakSpec){640.0f, 132.0f, 84.0f, 150.0f, PAL_SUMI_HEAVY, 0.26f, 2.2f, 3011u});
    draw_peak(r, (PeakSpec){1000.0f, 142.0f, 100.0f, 170.0f, PAL_SUMI_HEAVY, 0.30f, 2.4f, 3012u});
    draw_cloud(r, (Vector2){430.0f, 70.0f}, 250.0f, 58.0f, PAL_SUMI_HEAVY, 0.22f, 3020u);
    draw_cloud(r, (Vector2){820.0f, 62.0f}, 270.0f, 60.0f, PAL_SUMI_HEAVY, 0.22f, 3030u);
    mist_drift(r, 174.0f, 84.0f, 0.7f, 3050u);

    draw_stone_column(r, 30.0f, 60.0f, 700.0f, 78.0f, 0.35f, 3100u);
    draw_stone_column(r, 116.0f, 300.0f, 700.0f, 52.0f, 0.27f, 3101u);
    draw_stone_column(r, 1250.0f, 40.0f, 700.0f, 84.0f, 0.35f, 3110u);
    draw_stone_column(r, 1164.0f, 380.0f, 720.0f, 56.0f, 0.27f, 3111u);
    draw_pine(r, (Vector2){1250.0f, 300.0f}, 0.62f, -1.0f, 0.9f, 3200u);
    mist_band(r, 610.0f, 740.0f, 0.55f);
    draw_boulder(r, (Vector2){210.0f, 712.0f}, 64.0f, 24.0f, 0.7f, 3300u);
    draw_boulder(r, (Vector2){1060.0f, 714.0f}, 60.0f, 22.0f, 0.7f, 3310u);
    centre_mist(r, 0.24f);
}

static void scene_sunago(RenderState *r, int loop_n, int act_index)
{
    const int flecks = 20 * (loop_n < 1 ? 1 : (loop_n > 6 ? 6 : loop_n));
    uint64_t rng = UINT64_C(0x53554e41474f0001) ^
                   ((uint64_t)(uint32_t)act_index * UINT64_C(0x9e3779b97f4a7c15));
    for (int i = 0; i < flecks; ++i) {
        const uint32_t edge = (uint32_t)(ink_rng_next(&rng) % 4u);
        float fx = 0.0f;
        float fy = 0.0f;
        if (edge == 0u) {
            fx = 12.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_W - 24.0f);
            fy = 60.0f + ink_rng_f01(&rng) * 70.0f;
        } else if (edge == 1u) {
            fx = 12.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_W - 24.0f);
            fy = (float)INK_SCREEN_H - 100.0f + ink_rng_f01(&rng) * 92.0f;
        } else if (edge == 2u) {
            fx = 10.0f + ink_rng_f01(&rng) * 110.0f;
            fy = 120.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_H - 240.0f);
        } else {
            fx = (float)INK_SCREEN_W - 120.0f + ink_rng_f01(&rng) * 110.0f;
            fy = 120.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_H - 240.0f);
        }
        const bool foil = (i % 9) == 0; /* occasional cut-foil square (切箔) */
        const float rx = foil ? 6.0f + ink_rng_f01(&rng) * 4.0f : 1.6f + ink_rng_f01(&rng) * 2.8f;
        const float ry =
            foil ? rx * (0.8f + ink_rng_f01(&rng) * 0.3f) : 1.4f + ink_rng_f01(&rng) * 2.2f;
        const float ang = ink_rng_f01(&rng) * 2.0f * k_pi;
        const float ca = cosf(ang);
        const float sa = sinf(ang);
        const float skew = 0.72f + ink_rng_f01(&rng) * 0.45f;
        const Vector2 local[4] = {
            {-rx, -ry * 0.65f}, {rx * skew, -ry}, {rx, ry * 0.85f}, {-rx * 0.75f, ry * skew}};
        Vector2 poly[4];
        for (int v = 0; v < 4; ++v) {
            poly[v] = (Vector2){fx + local[v].x * ca - local[v].y * sa,
                                fy + local[v].x * sa + local[v].y * ca};
        }
        const PaletteId pal = (ink_rng_next(&rng) & 1u) != 0u ? PAL_KIN : PAL_KIN_HOT;
        const float alpha = (foil ? 0.60f : 0.66f) + ink_rng_f01(&rng) * 0.3f;
        if (foil) {
            ink_radial_glow(r, (Vector2){fx, fy}, rx * 2.4f, ry * 2.4f, tone_c(PAL_KIN_HOT, 0.22f),
                            tone_c(PAL_KIN_HOT, 0.0f));
        }
        ink_fill_poly(r, poly, 4, pal_color_alpha(pal, alpha));
    }
}

void render_scenery_draw(RenderState *r, BgKey key)
{
    if (r == NULL) {
        return;
    }
    if (key.kind == BG_LOBBY) {
        scene_lobby(r);
        return;
    }
    const int act = key.act_index >= 0 ? key.act_index : 0;
    const int stage_act = act % GAME_MAIN_ACT_COUNT;
    if (stage_act == 1) {
        scene_act2(r);
    } else if (stage_act == 2) {
        scene_act3(r);
    } else {
        scene_act1(r);
    }
    const int loop_n = act / GAME_MAIN_ACT_COUNT;
    if (loop_n >= 1) {
        scene_sunago(r, loop_n, act);
    }
}
