#include "ink_prims.h"

#include "text_layout.h"

#include "core.h"
#include "rlgl.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

enum { INK_MAX_SAMPLES = 256 };

static const float k_pi = 3.14159265359f;
static const float k_two_pi = 6.28318530718f;

static float hash_f01(uint32_t seed, uint32_t idx)
{
    uint64_t h = ink_hash_u32(INK_HASH_INIT, seed);
    h = ink_hash_u32(h, idx);
    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (float)(h >> 40) * (1.0f / 16777216.0f);
}

static float noise1d(uint32_t seed, float x)
{
    const int xi = (int)floorf(x);
    const float xf = x - (float)xi;
    const float u = xf * xf * (3.0f - 2.0f * xf);
    const float a = hash_f01(seed, (uint32_t)xi);
    const float b = hash_f01(seed, (uint32_t)(xi + 1));
    return (a + (b - a) * u) * 2.0f - 1.0f;
}

static Vector2 catmull_rom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (Vector2){
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
    };
}

static float vec_dist(Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static int resample_polyline(const Vector2 *pts, int n, bool closed, Vector2 *out, int cap)
{
    if (n < 2 || cap < 2) {
        return 0;
    }
    int out_n = 0;
    const int segs = closed ? n : (n - 1);
    for (int s = 0; s < segs && out_n < cap - 1; ++s) {
        const int i1 = s % n;
        const int i2 = (s + 1) % n;
        const int i0 = closed ? ((s + n - 1) % n) : (s > 0 ? s - 1 : 0);
        const int i3 = closed ? ((s + 2) % n) : (s + 2 < n ? s + 2 : n - 1);
        const float d = vec_dist(pts[i1], pts[i2]);
        int steps = (int)ceilf(d / 4.0f);
        if (steps < 2) {
            steps = 2;
        }
        if (steps > 24) {
            steps = 24;
        }
        for (int k = 0; k < steps && out_n < cap - 1; ++k) {
            const float t = (float)k / (float)steps;
            out[out_n++] = catmull_rom(pts[i0], pts[i1], pts[i2], pts[i3], t);
        }
    }
    if (out_n < cap) {
        out[out_n++] = closed ? pts[0] : pts[n - 1];
    }
    return out_n;
}

static void emit_tri_ccw(RenderState *r, Vector2 a, Color ca, Vector2 b, Color cb, Vector2 c,
                         Color cc)
{
    /* Raylib 2D uses Y-down ortho where screen-space CCW (cross < 0) is front-facing. */
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) {
        const Vector2 tb = b;
        const Color tcb = cb;
        b = c;
        cb = cc;
        c = tb;
        cc = tcb;
    }
    rlColor4ub(ca.r, ca.g, ca.b, ca.a);
    rlVertex2f(a.x, a.y);
    rlColor4ub(cb.r, cb.g, cb.b, cb.a);
    rlVertex2f(b.x, b.y);
    rlColor4ub(cc.r, cc.g, cc.b, cc.a);
    rlVertex2f(c.x, c.y);
    r->metrics.prim_verts += 3;
}

static void emit_ribbon(RenderState *r, const Vector2 *pts, const float *widths,
                        const float *alphas, int n, PaletteId pal)
{
    if (n < 2) {
        return;
    }
    const int needed_verts = (n - 1) * 6;
    if (needed_verts <= 0) {
        return;
    }
    rlCheckRenderBatchLimit(needed_verts);
    const Color base = pal_color(pal);

    Vector2 left_prev = pts[0];
    Vector2 right_prev = pts[0];
    Color c_prev = base;
    c_prev.a = (unsigned char)(ink_clampf(alphas[0], 0.0f, 1.0f) * 255.0f);

    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; ++i) {
        Vector2 tan_v;
        if (i == 0) {
            tan_v = (Vector2){pts[1].x - pts[0].x, pts[1].y - pts[0].y};
        } else if (i == n - 1) {
            tan_v = (Vector2){pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y};
        } else {
            tan_v = (Vector2){pts[i + 1].x - pts[i - 1].x, pts[i + 1].y - pts[i - 1].y};
        }
        const float len = sqrtf(tan_v.x * tan_v.x + tan_v.y * tan_v.y);
        Vector2 nrm = {0.0f, 1.0f};
        if (len > 1e-4f) {
            nrm = (Vector2){-tan_v.y / len, tan_v.x / len};
        }
        const float hw = widths[i] * 0.5f;
        const Vector2 left = {pts[i].x + nrm.x * hw, pts[i].y + nrm.y * hw};
        const Vector2 right = {pts[i].x - nrm.x * hw, pts[i].y - nrm.y * hw};
        Color c_cur = base;
        c_cur.a = (unsigned char)(ink_clampf(alphas[i], 0.0f, 1.0f) * 255.0f);

        if (i > 0) {
            emit_tri_ccw(r, left_prev, c_prev, right_prev, c_prev, left, c_cur);
            emit_tri_ccw(r, right_prev, c_prev, right, c_cur, left, c_cur);
        }
        left_prev = left;
        right_prev = right;
        c_prev = c_cur;
    }
    rlEnd();
}

void ink_stroke(RenderState *r, const Vector2 *pts, int n, BrushProfile prof, InkTone tone,
                float reveal)
{
    if (r == NULL || pts == NULL || n < 2 || reveal <= 0.001f || tone.alpha <= 0.001f) {
        return;
    }

    Vector2 samples[INK_MAX_SAMPLES];
    const int total = resample_polyline(pts, n, false, samples, INK_MAX_SAMPLES);
    if (total < 2) {
        return;
    }

    int count = (int)ceilf((float)total * ink_clampf(reveal, 0.0f, 1.0f));
    if (count < 2) {
        count = 2;
    }
    if (count > total) {
        count = total;
    }

    const float w_max = prof.w_max > 1.2f ? prof.w_max : 1.2f;
    const float w_min = fmaxf(1.2f, w_max * ink_clampf(prof.w_min_ratio, 0.05f, 0.4f));
    const float entry = ink_clampf(prof.entry, 0.0f, 0.45f);
    const float exit_r = ink_clampf(prof.exit, 0.0f, 0.45f);
    const float dry = ink_clampf(prof.dry, 0.0f, 1.0f);

    Vector2 jittered[INK_MAX_SAMPLES] = {0};
    float widths[INK_MAX_SAMPLES] = {0};
    float alphas[INK_MAX_SAMPLES] = {0};

    for (int i = 0; i < count; ++i) {
        const float t = (float)i / (float)(total - 1);
        float w;
        if (entry > 0.001f && t < entry) {
            const float u = t / entry;
            w = w_min + (w_max - w_min) * (u * (2.0f - u));
        } else if (exit_r > 0.001f && t > 1.0f - exit_r) {
            const float u = (1.0f - t) / exit_r;
            w = w_min + (w_max - w_min) * (u * u);
        } else {
            w = w_max * (1.0f + 0.08f * noise1d(prof.seed, t * 14.0f));
        }
        widths[i] = fmaxf(1.2f, w);

        float a = tone.alpha;
        if (dry > 0.001f && t > 0.70f) {
            const float dt = (t - 0.70f) / 0.30f;
            const float streak = 0.5f + 0.5f * noise1d(prof.seed ^ 0x9e37u, t * 28.0f);
            a *= ink_clampf(1.0f - dry * dt * (0.45f + 0.55f * streak), 0.12f, 1.0f);
        }
        if (i == 0 && entry > 0.001f) {
            a *= 0.65f;
        } else if (i == count - 1 && exit_r > 0.001f) {
            a *= 0.45f;
        }
        alphas[i] = a;

        const float jx = noise1d(prof.seed + 17u, (float)i * 0.45f) * 0.55f;
        const float jy = noise1d(prof.seed + 53u, (float)i * 0.45f) * 0.55f;
        jittered[i] = (Vector2){samples[i].x + jx, samples[i].y + jy};
    }

    emit_ribbon(r, jittered, widths, alphas, count, tone.color);
}

void ink_wash(RenderState *r, Vector2 center, float rx, float ry, float rot, InkTone tone,
              int layers, uint32_t seed)
{
    if (r == NULL || rx <= 0.5f || ry <= 0.5f || tone.alpha <= 0.001f) {
        return;
    }
    if (layers < 1) {
        layers = 1;
    }
    if (layers > 4) {
        layers = 4;
    }

    const Color base = pal_color(tone.color);
    const int segs = 16;
    const float cos_r = cosf(rot);
    const float sin_r = sinf(rot);

    rlCheckRenderBatchLimit(layers * segs * 3);
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int l = 0; l < layers; ++l) {
        const uint32_t ls = seed + (uint32_t)l * 7919u;
        const float ox = (hash_f01(ls, 1u) - 0.5f) * rx * 0.18f;
        const float oy = (hash_f01(ls, 2u) - 0.5f) * ry * 0.18f;
        const float scale = 1.0f - (float)l * 0.10f;
        const Vector2 c = {center.x + ox, center.y + oy};

        const float outer_a = ink_clampf(tone.alpha / (float)layers * 1.35f, 0.05f, 0.95f);
        const float inner_a = outer_a * 0.48f;
        Color c_out = base;
        c_out.a = (unsigned char)(outer_a * 255.0f);
        Color c_in = base;
        c_in.a = (unsigned char)(inner_a * 255.0f);

        Vector2 ring[20];
        for (int i = 0; i < segs; ++i) {
            const float ang = k_two_pi * (float)i / (float)segs;
            const float rad_j = 0.85f + 0.30f * hash_f01(ls, 10u + (uint32_t)i);
            const float lx = cosf(ang) * rx * scale * rad_j;
            const float ly = sinf(ang) * ry * scale * rad_j;
            ring[i] = (Vector2){
                c.x + lx * cos_r - ly * sin_r,
                c.y + lx * sin_r + ly * cos_r,
            };
        }

        for (int i = 0; i < segs; ++i) {
            const int nxt = (i + 1) % segs;
            emit_tri_ccw(r, c, c_in, ring[i], c_out, ring[nxt], c_out);
        }
    }
    rlEnd();
}

void ink_teardrop(RenderState *r, Vector2 at, Vector2 dir, float len, float wid, Color c)
{
    if (r == NULL || len <= 0.5f || wid <= 0.5f || c.a == 0) {
        return;
    }
    const float dl = sqrtf(dir.x * dir.x + dir.y * dir.y);
    const Vector2 fwd = dl > 1e-4f ? (Vector2){dir.x / dl, dir.y / dl} : (Vector2){1.0f, 0.0f};
    const Vector2 side = {-fwd.y, fwd.x};

    Vector2 poly[8];
    poly[0] = (Vector2){at.x + fwd.x * len, at.y + fwd.y * len};
    poly[1] = (Vector2){at.x + fwd.x * (len * 0.2f) + side.x * (wid * 0.52f),
                        at.y + fwd.y * (len * 0.2f) + side.y * (wid * 0.52f)};
    poly[2] = (Vector2){at.x - fwd.x * (len * 0.35f) + side.x * (wid * 0.45f),
                        at.y - fwd.y * (len * 0.35f) + side.y * (wid * 0.45f)};
    poly[3] = (Vector2){at.x - fwd.x * (len * 0.55f), at.y - fwd.y * (len * 0.55f)};
    poly[4] = (Vector2){at.x - fwd.x * (len * 0.35f) - side.x * (wid * 0.45f),
                        at.y - fwd.y * (len * 0.35f) - side.y * (wid * 0.45f)};
    poly[5] = (Vector2){at.x + fwd.x * (len * 0.2f) - side.x * (wid * 0.52f),
                        at.y + fwd.y * (len * 0.2f) - side.y * (wid * 0.52f)};
    ink_fill_poly(r, poly, 6, c);
}

void ink_splatter(RenderState *r, Vector2 at, Vector2 dir, float energy, InkTone tone,
                  uint64_t *vfx_rng)
{
    if (r == NULL || vfx_rng == NULL || energy <= 0.01f || tone.alpha <= 0.001f) {
        return;
    }
    const float dl = sqrtf(dir.x * dir.x + dir.y * dir.y);
    const Vector2 base_dir = dl > 1e-4f ? (Vector2){dir.x / dl, dir.y / dl} : (Vector2){1.0f, 0.0f};
    const float base_ang = atan2f(base_dir.y, base_dir.x);

    const uint32_t wash_seed = (uint32_t)(ink_rng_next(vfx_rng) & 0xffffffffu);
    ink_wash(r, at, 6.0f + energy * 10.0f, 5.0f + energy * 8.0f, base_ang, tone, 1, wash_seed);

    const int drops = 4 + (int)(ink_rng_next(vfx_rng) % 4u);
    const Color c = pal_color_alpha(tone.color, tone.alpha);
    for (int i = 0; i < drops; ++i) {
        const float spread = (ink_rng_f01(vfx_rng) - 0.5f) * (70.0f * k_pi / 180.0f);
        const float ang = base_ang + spread;
        const float t = 0.25f + 0.75f * ink_rng_f01(vfx_rng);
        const float dist = (10.0f + energy * 38.0f) * t;
        const Vector2 dd = {cosf(ang), sinf(ang)};
        const Vector2 p = {at.x + dd.x * dist, at.y + dd.y * dist};
        const float drop_len = (5.0f + energy * 7.0f) * (1.25f - 0.65f * t);
        ink_teardrop(r, p, dd, drop_len, drop_len * 0.52f, c);
    }

    const int streaks = 2 + (int)(ink_rng_next(vfx_rng) % 2u);
    for (int i = 0; i < streaks; ++i) {
        const float spread = (ink_rng_f01(vfx_rng) - 0.5f) * (64.0f * k_pi / 180.0f);
        const float ang = base_ang + spread;
        const float len = 14.0f + energy * (22.0f + 16.0f * ink_rng_f01(vfx_rng));
        const Vector2 dd = {cosf(ang), sinf(ang)};
        const Vector2 pts[2] = {at, {at.x + dd.x * len, at.y + dd.y * len}};
        const BrushProfile prof = {
            .w_max = 2.4f + energy * 2.2f,
            .w_min_ratio = 0.25f,
            .entry = 0.0f,
            .exit = 0.70f,
            .dry = 0.4f,
            .seed = (uint32_t)(ink_rng_next(vfx_rng) & 0xffffffffu),
        };
        ink_stroke(r, pts, 2, prof, tone, 1.0f);
    }
}

void ink_outline_tone(RenderState *r, const Vector2 *ctrl, int n, bool closed, float w_max,
                      Vector2 shade_dir, InkTone tone, uint32_t seed)
{
    if (r == NULL || ctrl == NULL || n < 2 || tone.alpha <= 0.001f) {
        return;
    }

    Vector2 samples[INK_MAX_SAMPLES];
    const int total = resample_polyline(ctrl, n, closed, samples, INK_MAX_SAMPLES);
    if (total < 3) {
        return;
    }

    const float sdl = sqrtf(shade_dir.x * shade_dir.x + shade_dir.y * shade_dir.y);
    const Vector2 sd =
        sdl > 1e-4f ? (Vector2){shade_dir.x / sdl, shade_dir.y / sdl} : (Vector2){0.707f, 0.707f};

    /* 1-2 breaks of 6%..12% circumference for hand-brushed openness */
    const float gap1_c = 0.18f + 0.28f * hash_f01(seed, 101u);
    const float gap1_hw = 0.032f + 0.024f * hash_f01(seed, 102u);
    const bool has_gap2 = hash_f01(seed, 103u) > 0.45f;
    const float gap2_c = 0.68f + 0.22f * hash_f01(seed, 104u);
    const float gap2_hw = 0.028f + 0.020f * hash_f01(seed, 105u);

    Vector2 seg_pts[INK_MAX_SAMPLES];
    float seg_w[INK_MAX_SAMPLES];
    float seg_a[INK_MAX_SAMPLES];
    int seg_n = 0;

    for (int i = 0; i < total; ++i) {
        const float t = (float)i / (float)(total - 1);
        const bool in_gap1 = closed && fabsf(t - gap1_c) < gap1_hw;
        const bool in_gap2 = closed && has_gap2 && fabsf(t - gap2_c) < gap2_hw;
        if (in_gap1 || in_gap2) {
            if (seg_n >= 2) {
                seg_w[0] = fmaxf(1.2f, w_max * 0.30f);
                seg_w[seg_n - 1] = fmaxf(1.2f, w_max * 0.30f);
                seg_a[0] *= 0.55f;
                seg_a[seg_n - 1] *= 0.55f;
                emit_ribbon(r, seg_pts, seg_w, seg_a, seg_n, tone.color);
            }
            seg_n = 0;
            continue;
        }

        const int ip = i > 0 ? i - 1 : (closed ? total - 2 : 0);
        const int inxt = i + 1 < total ? i + 1 : (closed ? 1 : total - 1);
        const Vector2 tv = {samples[inxt].x - samples[ip].x, samples[inxt].y - samples[ip].y};
        const float tl = sqrtf(tv.x * tv.x + tv.y * tv.y);
        const Vector2 nrm = tl > 1e-4f ? (Vector2){-tv.y / tl, tv.x / tl} : (Vector2){0.0f, 1.0f};
        const float dot = nrm.x * sd.x + nrm.y * sd.y;
        const float mod = 0.38f + 0.62f * (dot > 0.0f ? dot : 0.0f);
        const float w = fmaxf(1.2f, w_max * mod * (1.0f + 0.07f * noise1d(seed, t * 18.0f)));

        const float jx = noise1d(seed + 31u, (float)i * 0.5f) * 0.55f;
        const float jy = noise1d(seed + 71u, (float)i * 0.5f) * 0.55f;
        seg_pts[seg_n] = (Vector2){samples[i].x + jx, samples[i].y + jy};
        seg_w[seg_n] = w;
        seg_a[seg_n] = tone.alpha;
        seg_n++;
    }

    if (seg_n >= 2) {
        seg_w[0] = fmaxf(1.2f, w_max * 0.30f);
        seg_w[seg_n - 1] = fmaxf(1.2f, w_max * 0.30f);
        seg_a[0] *= 0.55f;
        seg_a[seg_n - 1] *= 0.55f;
        emit_ribbon(r, seg_pts, seg_w, seg_a, seg_n, tone.color);
    }
}

void ink_outline(RenderState *r, const Vector2 *ctrl, int n, bool closed, float w_max,
                 Vector2 shade_dir, uint32_t seed)
{
    ink_outline_tone(r, ctrl, n, closed, w_max, shade_dir,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f}, seed);
}

void ink_fill_poly(RenderState *r, const Vector2 *pts, int n, Color c)
{
    if (r == NULL || pts == NULL || n < 3 || c.a == 0) {
        return;
    }
    Vector2 center = {0.0f, 0.0f};
    for (int i = 0; i < n; ++i) {
        center.x += pts[i].x;
        center.y += pts[i].y;
    }
    center.x /= (float)n;
    center.y /= (float)n;

    rlCheckRenderBatchLimit(n * 3);
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < n; ++i) {
        const int nxt = (i + 1) % n;
        emit_tri_ccw(r, center, c, pts[i], c, pts[nxt], c);
    }
    rlEnd();
}

void ink_knockout(RenderState *r, const Vector2 *ctrl, int n, float pad_px, int act_index)
{
    if (r == NULL || ctrl == NULL || n < 3) {
        return;
    }
    Vector2 center = {0.0f, 0.0f};
    for (int i = 0; i < n; ++i) {
        center.x += ctrl[i].x;
        center.y += ctrl[i].y;
    }
    center.x /= (float)n;
    center.y /= (float)n;

    Vector2 expanded[64];
    const int count = n < 64 ? n : 64;
    for (int i = 0; i < count; ++i) {
        const float dx = ctrl[i].x - center.x;
        const float dy = ctrl[i].y - center.y;
        const float d = sqrtf(dx * dx + dy * dy);
        if (d > 1e-4f) {
            expanded[i] = (Vector2){ctrl[i].x + (dx / d) * pad_px, ctrl[i].y + (dy / d) * pad_px};
        } else {
            expanded[i] = ctrl[i];
        }
    }

    const Color ko = pal_lerp(pal_paper_for_act(act_index), PAL_GOFUN, 0.5f, 0.85f);
    ink_fill_poly(r, expanded, count, ko);
}

/* Two-character seals (e.g. 墨嵐, 三元) overflowed the stamp when drawn on one line; carved seals
 * stack them vertically, and anything longer is shrunk to fit the square. */
static void draw_seal_glyphs(const RenderState *r, TextId glyph, Vector2 at, float size,
                             PaletteId pal, float alpha)
{
    if (!r->use_ja || glyph == TEXT_NONE) {
        return;
    }
    const char *txt = text_get(glyph, true);
    const size_t len = strlen(txt);
    const size_t first = text_utf8_next(txt, 0, len);
    const size_t second = text_utf8_next(txt, first, len);
    if (first < len && second == len && first < 8 && len - first < 8) {
        char top[8];
        char bottom[8];
        memcpy(top, txt, first);
        top[first] = '\0';
        memcpy(bottom, txt + first, len - first);
        bottom[len - first] = '\0';
        const float gs = size * 0.42f;
        render_draw_text_centered(r, top, (Vector2){at.x, at.y - size * 0.215f}, gs, pal, alpha,
                                  true);
        render_draw_text_centered(r, bottom, (Vector2){at.x, at.y + size * 0.215f}, gs, pal, alpha,
                                  true);
        return;
    }
    render_draw_text_fit(r, txt, at, size * 0.80f, size * 0.62f, size * 0.25f, pal, alpha, true);
}

void seal_stamp(RenderState *r, TextId glyph, Vector2 at, float size, SealStyle style, float t)
{
    if (r == NULL || size <= 2.0f) {
        return;
    }
    const float clamped_t = ink_clampf(t, 0.0f, 1.0f);
    float scale = 1.0f;
    if (clamped_t < 0.20f) {
        const float u = clamped_t / 0.20f;
        scale = 1.30f - 0.30f * u;
    } else {
        scale = 1.0f + 0.03f * ((clamped_t - 0.20f) / 0.80f);
    }

    const uint32_t seed = (uint32_t)glyph * 131u + 41u;
    const float tilt = (hash_f01(seed, 1u) - 0.5f) * (8.0f * k_pi / 180.0f);
    const float cos_t = cosf(tilt);
    const float sin_t = sinf(tilt);
    const float hs = size * 0.5f * scale;

    /* Hand-carved organic square seal (12 perimeter vertices) */
    Vector2 poly[12];
    const float coords[12][2] = {
        {-1.0f, -1.0f}, {-0.33f, -1.0f}, {0.33f, -1.0f}, {1.0f, -1.0f},
        {1.0f, -0.33f}, {1.0f, 0.33f},   {1.0f, 1.0f},   {0.33f, 1.0f},
        {-0.33f, 1.0f}, {-1.0f, 1.0f},   {-1.0f, 0.33f}, {-1.0f, -0.33f},
    };
    for (int i = 0; i < 12; ++i) {
        const float jx = (hash_f01(seed, 10u + (uint32_t)i) - 0.5f) * 3.0f;
        const float jy = (hash_f01(seed, 30u + (uint32_t)i) - 0.5f) * 3.0f;
        const float lx = coords[i][0] * hs + jx;
        const float ly = coords[i][1] * hs + jy;
        poly[i] = (Vector2){
            at.x + lx * cos_t - ly * sin_t,
            at.y + lx * sin_t + ly * cos_t,
        };
    }

    if (style == SEAL_HAKUBUN) {
        ink_fill_poly(r, poly, 12, pal_color_alpha(PAL_SHU, 0.92f));
        /* Dry stone texture holes */
        for (int h = 0; h < 3; ++h) {
            const float hx = (hash_f01(seed, 60u + (uint32_t)h) - 0.5f) * hs * 1.2f;
            const float hy = (hash_f01(seed, 70u + (uint32_t)h) - 0.5f) * hs * 1.2f;
            const Vector2 hp = {at.x + hx, at.y + hy};
            ink_teardrop(r, hp, (Vector2){1.0f, 0.4f}, size * 0.08f, size * 0.05f,
                         pal_color_alpha(PAL_GOFUN, 0.45f));
        }
        draw_seal_glyphs(r, glyph, at, size * scale, PAL_GOFUN, 0.96f);
    } else {
        ink_fill_poly(r, poly, 12, pal_color_alpha(PAL_GOFUN, 0.90f));
        ink_outline_tone(r, poly, 12, true, 3.0f, (Vector2){0.7f, 0.7f},
                         (InkTone){.color = PAL_SHU, .alpha = 0.92f}, seed);
        draw_seal_glyphs(r, glyph, at, size * scale, PAL_SHU, 0.94f);
    }
}

void ink_fill_panel(RenderState *r, Rectangle rect, PaletteId fill_pal, float fill_alpha,
                    PaletteId border_pal, float border_alpha, uint32_t seed)
{
    if (r == NULL || rect.width <= 1.0f || rect.height <= 1.0f) {
        return;
    }
    const Vector2 tl = {rect.x, rect.y};
    const Vector2 tr = {rect.x + rect.width, rect.y};
    const Vector2 br = {rect.x + rect.width, rect.y + rect.height};
    const Vector2 bl = {rect.x, rect.y + rect.height};
    const Vector2 corners[4] = {tl, tr, br, bl};
    ink_fill_poly(r, corners, 4, pal_color_alpha(fill_pal, fill_alpha));

    if (border_alpha > 0.001f) {
        const InkTone tone = {.color = border_pal, .alpha = border_alpha};
        const Vector2 edges[4][2] = {{tl, tr}, {tr, br}, {br, bl}, {bl, tl}};
        for (int e = 0; e < 4; ++e) {
            const Vector2 a = edges[e][0];
            const Vector2 b = edges[e][1];
            const Vector2 mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            const Vector2 pts[3] = {a, mid, b};
            const BrushProfile prof = {
                .w_max = 3.0f,
                .w_min_ratio = 0.35f,
                .entry = 0.08f,
                .exit = 0.14f,
                .dry = 0.22f,
                .seed = seed + (uint32_t)e * 29u,
            };
            ink_stroke(r, pts, 3, prof, tone, 1.0f);
        }
    }
}

void ink_arc_stroke(RenderState *r, Vector2 center, float radius, float a0, float a1,
                    BrushProfile prof, InkTone tone)
{
    if (r == NULL || radius <= 1.0f || fabsf(a1 - a0) < 0.01f) {
        return;
    }
    const int pts_n = 18;
    Vector2 pts[20];
    for (int i = 0; i < pts_n; ++i) {
        const float u = (float)i / (float)(pts_n - 1);
        const float a = a0 + (a1 - a0) * u;
        pts[i] = (Vector2){center.x + cosf(a) * radius, center.y + sinf(a) * radius};
    }
    ink_stroke(r, pts, pts_n, prof, tone, 1.0f);
}

void ink_dashed_stroke(RenderState *r, Vector2 a, Vector2 b, float dash_len, float gap_len,
                       float w_max, InkTone tone, uint32_t seed)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float total = sqrtf(dx * dx + dy * dy);
    if (total < 1.0f || dash_len < 1.0f) {
        return;
    }
    const float ux = dx / total;
    const float uy = dy / total;
    const float step = dash_len + gap_len;
    float pos = 0.0f;
    uint32_t idx = 0;
    while (pos < total) {
        const float end = fminf(total, pos + dash_len);
        const Vector2 seg[2] = {
            {a.x + ux * pos, a.y + uy * pos},
            {a.x + ux * end, a.y + uy * end},
        };
        const BrushProfile prof = {
            .w_max = w_max,
            .w_min_ratio = 0.35f,
            .entry = 0.2f,
            .exit = 0.25f,
            .dry = 0.15f,
            .seed = seed + idx * 17u,
        };
        ink_stroke(r, seg, 2, prof, tone, 1.0f);
        pos += step;
        idx++;
    }
}
