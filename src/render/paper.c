#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

enum { PAPER_TEX_SIZE = 256 };

static const float k_paper_two_pi = 6.28318530718f;

void render_paper_init(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    /* Achromatic white-base washi grain texture so multiplying by pal_paper_for_act(act)
     * preserves exact Act 1/2/3 lightness (L* >= 80) without double-multiplication darkening. */
    Image img = GenImagePerlinNoise(PAPER_TEX_SIZE, PAPER_TEX_SIZE, 13, 29, 3.2f);
    Color *px = LoadImageColors(img);
    if (px != NULL) {
        uint64_t rng = UINT64_C(0x5741534849504150);
        for (int y = 0; y < PAPER_TEX_SIZE; ++y) {
            for (int x = 0; x < PAPER_TEX_SIZE; ++x) {
                const int idx = y * PAPER_TEX_SIZE + x;
                const float n = ((float)px[idx].r / 255.0f) - 0.5f;
                const float grain = n * 12.0f + (ink_rng_f01(&rng) - 0.5f) * 4.0f;
                const unsigned char v = (unsigned char)ink_clampf(248.0f + grain, 232.0f, 255.0f);
                px[idx].r = v;
                px[idx].g = v;
                px[idx].b = v;
                px[idx].a = 255;
            }
        }
        /* Subtle achromatic mulberry fiber lines */
        for (int f = 0; f < 42; ++f) {
            const int fx = (int)(ink_rng_next(&rng) % (uint64_t)PAPER_TEX_SIZE);
            const int fy = (int)(ink_rng_next(&rng) % (uint64_t)PAPER_TEX_SIZE);
            const int len = 10 + (int)(ink_rng_next(&rng) % 24u);
            const bool horiz = (ink_rng_next(&rng) & 1u) != 0u;
            const int delta = (ink_rng_next(&rng) & 2u) ? 5 : -6;
            for (int k = 0; k < len; ++k) {
                const int px_x = (fx + (horiz ? k : 0)) % PAPER_TEX_SIZE;
                const int px_y = (fy + (horiz ? 0 : k)) % PAPER_TEX_SIZE;
                Color *c = &px[px_y * PAPER_TEX_SIZE + px_x];
                const unsigned char v =
                    (unsigned char)ink_clampf((float)c->r + (float)delta, 228.0f, 255.0f);
                c->r = v;
                c->g = v;
                c->b = v;
            }
        }

        Image out = {
            .data = px,
            .width = PAPER_TEX_SIZE,
            .height = PAPER_TEX_SIZE,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        };
        r->paper_tex = LoadTextureFromImage(out);
        UnloadImageColors(px);
    }
    UnloadImage(img);

    if (r->paper_tex.id != 0) {
        SetTextureFilter(r->paper_tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(r->paper_tex, TEXTURE_WRAP_REPEAT);
    }

    r->bg_cache = LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H);
    r->bg_cached_act = -1;
}

void render_paper_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->paper_tex.id != 0) {
        UnloadTexture(r->paper_tex);
        r->paper_tex = (Texture2D){0};
    }
    if (r->bg_cache.id != 0) {
        UnloadRenderTexture(r->bg_cache);
        r->bg_cache = (RenderTexture2D){0};
    }
    r->bg_cached_act = -1;
}

void render_paper_draw_base(RenderState *r, int act_index)
{
    const PaletteId paper_pal = pal_paper_for_act(act_index);
    const Color tint = pal_color(paper_pal);
    ClearBackground(tint);
    if (r->paper_tex.id != 0) {
        const Rectangle src = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
        const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
        DrawTexturePro(r->paper_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, tint);
    }
}

static void bake_act1_background(RenderState *r)
{
    /* Act 1 edges only (within 120px margin, opacity <= 0.35, >= 80% empty paper) */
    const InkTone mist_tone = {.color = PAL_SUMI_CLEAR, .alpha = 0.28f};
    ink_wash(r, (Vector2){220.0f, 44.0f}, 190.0f, 38.0f, -0.05f, mist_tone, 2, 1001u);
    ink_wash(r, (Vector2){650.0f, 36.0f}, 240.0f, 34.0f, 0.04f, mist_tone, 2, 1002u);
    ink_wash(r, (Vector2){1060.0f, 50.0f}, 180.0f, 42.0f, -0.06f, mist_tone, 2, 1003u);

    static const float grass_x[4] = {96.0f, 152.0f, 1110.0f, 1178.0f};
    static const float grass_lean[4] = {36.0f, 52.0f, -44.0f, -28.0f};
    static const float grass_h[4] = {94.0f, 76.0f, 102.0f, 82.0f};
    const InkTone grass_tone = {.color = PAL_SUMI_LIGHT, .alpha = 0.30f};
    for (int i = 0; i < 4; ++i) {
        const float bx = grass_x[i];
        const float by = (float)INK_SCREEN_H + 6.0f;
        const Vector2 pts[4] = {
            {bx, by},
            {bx + grass_lean[i] * 0.25f, by - grass_h[i] * 0.38f},
            {bx + grass_lean[i] * 0.65f, by - grass_h[i] * 0.74f},
            {bx + grass_lean[i], by - grass_h[i]},
        };
        const BrushProfile prof = {.w_max = 4.2f,
                                   .w_min_ratio = 0.28f,
                                   .entry = 0.12f,
                                   .exit = 0.55f,
                                   .dry = 0.45f,
                                   .seed = 2000u + (uint32_t)i * 31u};
        ink_stroke(r, pts, 4, prof, grass_tone, 1.0f);
    }
}

static void bake_act2_background(RenderState *r)
{
    /* Act 2 dusk edges: bamboo stalks on left/right 120px margins + crow flock on top edge */
    static const float bamboo_x[6] = {28.0f, 64.0f, 96.0f, 1184.0f, 1218.0f, 1252.0f};
    static const float bamboo_lean[6] = {10.0f, -8.0f, 14.0f, -12.0f, 8.0f, -10.0f};
    const InkTone stalk_tone = {.color = PAL_SUMI_LIGHT, .alpha = 0.30f};
    const InkTone node_tone = {.color = PAL_SUMI_HEAVY, .alpha = 0.32f};

    for (int i = 0; i < 6; ++i) {
        const float bx = bamboo_x[i];
        const float lean = bamboo_lean[i];
        const Vector2 stalk[4] = {
            {bx, (float)INK_SCREEN_H + 8.0f},
            {bx + lean * 0.3f, 520.0f},
            {bx + lean * 0.7f, 260.0f},
            {bx + lean, 32.0f},
        };
        ink_stroke(r, stalk, 4,
                   (BrushProfile){.w_max = (i % 2 == 0) ? 7.5f : 5.2f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.08f,
                                  .exit = 0.15f,
                                  .dry = 0.35f,
                                  .seed = 2100u + (uint32_t)i * 19u},
                   stalk_tone, 1.0f);
        /* Bamboo nodes & sparse edge leaves */
        for (int seg = 1; seg <= 4; ++seg) {
            const float ny = (float)INK_SCREEN_H - (float)seg * 145.0f;
            const float nx = bx + lean * ((float)seg * 0.22f);
            const Vector2 node[2] = {{nx - 7.0f, ny + 1.5f}, {nx + 7.0f, ny - 1.5f}};
            ink_stroke(r, node, 2,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.25f,
                                      .dry = 0.20f,
                                      .seed = 2150u + (uint32_t)(i * 8 + seg)},
                       node_tone, 1.0f);
            if ((seg + i) % 2 == 0) {
                const float dir = (i < 3) ? 1.0f : -1.0f;
                const Vector2 leaf[3] = {
                    {nx, ny},
                    {nx + dir * 22.0f, ny - 10.0f},
                    {nx + dir * 42.0f, ny - 4.0f},
                };
                ink_stroke(r, leaf, 3,
                           (BrushProfile){.w_max = 3.4f,
                                          .w_min_ratio = 0.20f,
                                          .entry = 0.10f,
                                          .exit = 0.65f,
                                          .dry = 0.30f,
                                          .seed = 2200u + (uint32_t)(i * 10 + seg)},
                           stalk_tone, 1.0f);
            }
        }
    }

    /* Flock of 7 crows along top edge (< 120px) */
    static const float crow_x[7] = {240.0f, 295.0f, 360.0f, 520.0f, 820.0f, 910.0f, 985.0f};
    static const float crow_y[7] = {74.0f, 56.0f, 84.0f, 64.0f, 70.0f, 52.0f, 80.0f};
    const InkTone crow_tone = {.color = PAL_SUMI_HEAVY, .alpha = 0.33f};
    for (int c = 0; c < 7; ++c) {
        const float cx = crow_x[c];
        const float cy = crow_y[c];
        const float s = 12.0f + (float)(c % 3) * 3.5f;
        const Vector2 wing[3] = {
            {cx - s, cy + s * 0.22f}, {cx, cy - s * 0.28f}, {cx + s, cy + s * 0.16f}};
        ink_stroke(r, wing, 3,
                   (BrushProfile){.w_max = 3.4f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.25f,
                                  .exit = 0.45f,
                                  .dry = 0.25f,
                                  .seed = 2300u + (uint32_t)c * 13u},
                   crow_tone, 1.0f);
    }
}

static void bake_act3_background(RenderState *r)
{
    /* Act 3 spirit peak storm: swirling dark ink clouds in top 15% + 1 dragon-vein ridge stroke */
    const InkTone cloud_tone = {.color = PAL_SUMI_HEAVY, .alpha = 0.28f};
    const InkTone cloud_pale = {.color = PAL_SUMI_LIGHT, .alpha = 0.25f};
    ink_wash(r, (Vector2){180.0f, 42.0f}, 210.0f, 44.0f, -0.08f, cloud_tone, 2, 2401u);
    ink_wash(r, (Vector2){520.0f, 32.0f}, 260.0f, 46.0f, 0.05f, cloud_pale, 2, 2402u);
    ink_wash(r, (Vector2){910.0f, 38.0f}, 240.0f, 48.0f, -0.04f, cloud_tone, 2, 2403u);
    ink_wash(r, (Vector2){1170.0f, 52.0f}, 180.0f, 40.0f, 0.08f, cloud_pale, 2, 2404u);

    /* Continuous dragon-vein mountain ridge across top edge with dry-brush hihaku */
    const Vector2 ridge[9] = {
        {-12.0f, 108.0f}, {165.0f, 82.0f}, {340.0f, 96.0f},  {510.0f, 64.0f},  {648.0f, 52.0f},
        {810.0f, 78.0f},  {960.0f, 62.0f}, {1120.0f, 88.0f}, {1292.0f, 72.0f},
    };
    ink_stroke(r, ridge, 9,
               (BrushProfile){.w_max = 11.5f,
                              .w_min_ratio = 0.22f,
                              .entry = 0.10f,
                              .exit = 0.22f,
                              .dry = 0.45f,
                              .seed = 2420u},
               (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.34f}, 1.0f);

    /* Dry-brush mountainside shading strokes within top 115px */
    static const float slope_x[4] = {240.0f, 560.0f, 760.0f, 1030.0f};
    for (int i = 0; i < 4; ++i) {
        const Vector2 sl[3] = {
            {slope_x[i], 68.0f},
            {slope_x[i] - 32.0f, 92.0f},
            {slope_x[i] - 74.0f, 114.0f},
        };
        ink_stroke(r, sl, 3,
                   (BrushProfile){.w_max = 5.5f,
                                  .w_min_ratio = 0.20f,
                                  .entry = 0.12f,
                                  .exit = 0.55f,
                                  .dry = 0.65f,
                                  .seed = 2440u + (uint32_t)i * 17u},
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.26f}, 1.0f);
    }
}

static void bake_endless_sunago(RenderState *r, int loop_n, int act_index)
{
    int flecks = 20 * loop_n;
    if (flecks > 120) {
        flecks = 120;
    }
    uint64_t rng = UINT64_C(0x53554e41474f0001) ^
                   ((uint64_t)(uint32_t)act_index * UINT64_C(0x9e3779b97f4a7c15));

    for (int i = 0; i < flecks; ++i) {
        const uint32_t edge = (uint32_t)(ink_rng_next(&rng) % 4u);
        float fx = 0.0f;
        float fy = 0.0f;
        if (edge == 0u) {
            fx = 12.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_W - 24.0f);
            fy = 10.0f + ink_rng_f01(&rng) * 102.0f;
        } else if (edge == 1u) {
            fx = 12.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_W - 24.0f);
            fy = (float)INK_SCREEN_H - 112.0f + ink_rng_f01(&rng) * 102.0f;
        } else if (edge == 2u) {
            fx = 10.0f + ink_rng_f01(&rng) * 102.0f;
            fy = 116.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_H - 232.0f);
        } else {
            fx = (float)INK_SCREEN_W - 112.0f + ink_rng_f01(&rng) * 102.0f;
            fy = 116.0f + ink_rng_f01(&rng) * ((float)INK_SCREEN_H - 232.0f);
        }

        const float rx = 2.4f + ink_rng_f01(&rng) * 3.4f;
        const float ry = 2.0f + ink_rng_f01(&rng) * 2.8f;
        const float ang = ink_rng_f01(&rng) * k_paper_two_pi;
        const float ca = cosf(ang);
        const float sa = sinf(ang);
        const float skew = 0.72f + ink_rng_f01(&rng) * 0.45f;
        const Vector2 local[4] = {
            {-rx, -ry * 0.65f},
            {rx * skew, -ry},
            {rx, ry * 0.85f},
            {-rx * 0.75f, ry * skew},
        };
        Vector2 poly[4];
        for (int v = 0; v < 4; ++v) {
            poly[v] = (Vector2){
                fx + local[v].x * ca - local[v].y * sa,
                fy + local[v].x * sa + local[v].y * ca,
            };
        }
        const PaletteId pal = (ink_rng_next(&rng) & 1u) != 0u ? PAL_KIN : PAL_KIN_HOT;
        const float alpha = 0.22f + ink_rng_f01(&rng) * 0.12f;
        ink_fill_poly(r, poly, 4, pal_color_alpha(pal, alpha));
    }
}

static void bake_act_background(RenderState *r, int act_index)
{
    if (r->bg_cache.id == 0) {
        return;
    }
    BeginTextureMode(r->bg_cache);
    ClearBackground(pal_blank());

    const int stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0;
    if (stage_act == 1) {
        bake_act2_background(r);
    } else if (stage_act == 2) {
        bake_act3_background(r);
    } else {
        bake_act1_background(r);
    }

    const int loop_n = act_index >= 0 ? (act_index / GAME_MAIN_ACT_COUNT) : 0;
    if (loop_n >= 1) {
        bake_endless_sunago(r, loop_n, act_index);
    }

    EndTextureMode();
    r->bg_cached_act = act_index;
}

void render_paper_prepare_bg(RenderState *r, int act_index)
{
    if (r == NULL || r->bg_cache.id == 0) {
        return;
    }
    if (r->bg_cached_act != act_index) {
        bake_act_background(r, act_index);
    }
}

void render_paper_draw_bg(RenderState *r, int act_index)
{
    if (r->bg_cache.id == 0) {
        return;
    }
    (void)act_index;
    const Rectangle src = {0.0f, 0.0f, (float)r->bg_cache.texture.width,
                           -(float)r->bg_cache.texture.height};
    const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
    DrawTexturePro(r->bg_cache.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
}

static void draw_rock_pillar(RenderState *r, Vector2 c, float rad, uint32_t seed)
{
    /* Mokkotsu pale-ink washes + 3 short shunpo cracks + bold dark sumi contour stroke */
    ink_wash(r, (Vector2){c.x + 3.0f, c.y + 5.0f}, rad * 1.08f, rad * 0.94f, 0.18f,
             (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.56f}, 2, seed + 1u);
    ink_wash(r, (Vector2){c.x - 2.0f, c.y - 2.0f}, rad * 0.82f, rad * 0.78f, -0.22f,
             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.44f}, 2, seed + 2u);

    const Vector2 poly[7] = {
        {c.x - rad * 0.86f, c.y - rad * 0.48f}, {c.x - rad * 0.18f, c.y - rad * 1.02f},
        {c.x + rad * 0.72f, c.y - rad * 0.72f}, {c.x + rad * 1.00f, c.y + rad * 0.12f},
        {c.x + rad * 0.54f, c.y + rad * 0.96f}, {c.x - rad * 0.48f, c.y + rad * 0.88f},
        {c.x - rad * 0.96f, c.y + rad * 0.24f},
    };
    ink_outline(r, poly, 7, true, 4.6f, (Vector2){0.707f, 0.707f}, seed + 3u);

    /* 3 short Sesshu-style shunpo texture strokes */
    const Vector2 s1[2] = {{c.x - rad * 0.42f, c.y - rad * 0.35f},
                           {c.x + rad * 0.18f, c.y - rad * 0.52f}};
    const Vector2 s2[2] = {{c.x - rad * 0.25f, c.y + rad * 0.18f},
                           {c.x + rad * 0.45f, c.y - rad * 0.05f}};
    const Vector2 s3[2] = {{c.x - rad * 0.10f, c.y + rad * 0.52f},
                           {c.x + rad * 0.48f, c.y + rad * 0.38f}};
    const BrushProfile sp = {.w_max = 3.2f,
                             .w_min_ratio = 0.25f,
                             .entry = 0.12f,
                             .exit = 0.40f,
                             .dry = 0.35f,
                             .seed = seed + 10u};
    ink_stroke(r, s1, 2, sp, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.82f}, 1.0f);
    ink_stroke(r, s2, 2, sp, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.78f}, 1.0f);
    ink_stroke(r, s3, 2, sp, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.76f}, 1.0f);
}

static void draw_torii(RenderState *r, float cx, uint32_t seed)
{
    /* Solid pillars at (cx, 328) and (cx, 392) [16x16px], PAL_BENGARA (distinct from fire PAL_SHU).
     * Kasagi & nuki crossbars are visual only (not solid). */
    const float py[2] = {328.0f, 392.0f};
    for (int i = 0; i < 2; ++i) {
        const Vector2 p = {cx, py[i]};
        ink_wash(r, p, 11.0f, 11.0f, 0.05f, (InkTone){.color = PAL_BENGARA, .alpha = 0.88f}, 2,
                 seed + (uint32_t)i * 7u);
        const Vector2 box[4] = {
            {cx - 8.5f, py[i] - 8.5f},
            {cx + 8.5f, py[i] - 8.0f},
            {cx + 8.0f, py[i] + 8.5f},
            {cx - 8.5f, py[i] + 8.0f},
        };
        ink_outline_tone(r, box, 4, true, 3.6f, (Vector2){0.707f, 0.707f},
                         (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.92f},
                         seed + 20u + (uint32_t)i);
    }

    /* Subtle nuki tie-beam linking the two pillars vertically (side-view gate) */
    const Vector2 nuki[2] = {{cx + 3.0f, 322.0f}, {cx + 3.0f, 398.0f}};
    ink_stroke(r, nuki, 2,
               (BrushProfile){.w_max = 4.6f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.12f,
                              .exit = 0.15f,
                              .dry = 0.20f,
                              .seed = seed + 30u},
               (InkTone){.color = PAL_BENGARA, .alpha = 0.78f}, 1.0f);

    /* Sweeping dark sumi kasagi beam spanning across pillars (visual roof cap) */
    const Vector2 kasagi[3] = {
        {cx - 6.0f, 308.0f},
        {cx - 2.0f, 360.0f},
        {cx - 6.0f, 412.0f},
    };
    ink_stroke(r, kasagi, 3,
               (BrushProfile){.w_max = 8.4f,
                              .w_min_ratio = 0.28f,
                              .entry = 0.14f,
                              .exit = 0.16f,
                              .dry = 0.25f,
                              .seed = seed + 35u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.92f}, 1.0f);
}

static void draw_spirit_pillar(RenderState *r, Vector2 c, int32_t elem_kind, float timer,
                               uint32_t seed)
{
    const float rad = 16.0f;
    Vector2 poly[8];
    for (int i = 0; i < 8; ++i) {
        const float a = k_paper_two_pi * (float)i / 8.0f;
        const float j = 0.92f + 0.14f * ((float)((seed + (uint32_t)i * 11u) & 7u) / 7.0f);
        poly[i] = (Vector2){c.x + cosf(a) * rad * j, c.y + sinf(a) * rad * j};
    }

    /* Gofun stone body with pale sumi spiral engraving */
    ink_fill_poly(r, poly, 8, pal_color_alpha(PAL_GOFUN, 0.94f));
    ink_wash(r, c, rad * 0.85f, rad * 0.85f, 0.12f,
             (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.35f}, 1, seed + 1u);

    if (elem_kind == GIMMICK_ELEM_FIRE || elem_kind == GIMMICK_ELEM_STORM) {
        const PaletteId outer = elem_kind == GIMMICK_ELEM_FIRE ? PAL_SHU : PAL_AO;
        const PaletteId hot = elem_kind == GIMMICK_ELEM_FIRE ? PAL_SHU_HOT : PAL_AO_HOT;
        ink_wash(r, c, rad * 1.15f, rad * 1.15f, 0.0f, (InkTone){.color = outer, .alpha = 0.62f}, 2,
                 seed + 5u);
        ink_wash(r, c, rad * 0.62f, rad * 0.62f, 0.2f, (InkTone){.color = hot, .alpha = 0.88f}, 1,
                 seed + 6u);

        const float frac = ink_clampf(timer / 6.0f, 0.05f, 1.0f);
        ink_arc_stroke(r, c, rad + 6.5f, -1.5708f, -1.5708f + k_paper_two_pi * frac,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.1f,
                                      .exit = 0.2f,
                                      .dry = 0.1f,
                                      .seed = seed + 9u},
                       (InkTone){.color = outer, .alpha = 0.90f});
    } else {
        /* Pale sumi carved crest when unlit */
        ink_arc_stroke(r, c, rad * 0.48f, 0.2f, 5.2f,
                       (BrushProfile){.w_max = 2.4f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.30f,
                                      .dry = 0.2f,
                                      .seed = seed + 4u},
                       (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.80f});
    }

    ink_outline(r, poly, 8, true, 3.6f, (Vector2){0.707f, 0.707f}, seed + 2u);
}

void render_paper_draw_terrain(RenderState *r, const GameState *g, int act_index)
{
    if (r == NULL || g == NULL) {
        return;
    }
    render_set_blend(r, BLEND_ALPHA);

    const int stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0;
    if (stage_act == 1) {
        /* Act 2: 2 Torii gates + 4 rock pillars */
        draw_torii(r, 384.0f, 3010u);
        draw_torii(r, 896.0f, 3020u);
        draw_rock_pillar(r, (Vector2){340.0f, 190.0f}, 24.0f, 3101u);
        draw_rock_pillar(r, (Vector2){940.0f, 190.0f}, 24.0f, 3102u);
        draw_rock_pillar(r, (Vector2){340.0f, 530.0f}, 24.0f, 3103u);
        draw_rock_pillar(r, (Vector2){940.0f, 530.0f}, 24.0f, 3104u);
    } else if (stage_act == 2) {
        /* Act 3: upper 2 rock pillars */
        draw_rock_pillar(r, (Vector2){340.0f, 190.0f}, 24.0f, 3201u);
        draw_rock_pillar(r, (Vector2){940.0f, 190.0f}, 24.0f, 3202u);
    }

    /* Draw any spirit pillars present in ECS (Act 3 gameplay or Showcase Page 1) */
    int drawn_pillars = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const GimmickComp *gim = game_gimmick(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (gim != NULL && tr != NULL && gim->kind == GIMMICK_SPIRIT_PILLAR) {
            draw_spirit_pillar(r, (Vector2){tr->x, tr->y}, gim->elem_kind, gim->timer,
                               3300u + (uint32_t)drawn_pillars * 19u);
            drawn_pillars++;
        }
    }
    if (stage_act == 2 && drawn_pillars == 0) {
        static const Vector2 k_default_pillars[3] = {
            {640.0f, 130.0f}, {441.0f, 475.0f}, {839.0f, 475.0f}};
        for (int i = 0; i < 3; ++i) {
            draw_spirit_pillar(r, k_default_pillars[i], GIMMICK_ELEM_NONE, 0.0f,
                               3300u + (uint32_t)i * 19u);
        }
    }
}

void render_paper_draw_weather(RenderState *r, const GameState *g, int act_index)
{
    if (r == NULL || g == NULL) {
        return;
    }
    render_set_blend(r, BLEND_ALPHA);

    /* Act 3 diagonal clear-ink rain lines drifting at 60 px/s */
    const int stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0;
    if (stage_act == 2) {
        const float drift = fmodf(r->vfx_time * 60.0f, 160.0f);
        const InkTone rain_tone = {.color = PAL_SUMI_CLEAR, .alpha = 0.30f};
        for (int i = 0; i < 16; ++i) {
            const float bx =
                fmodf((float)i * 88.0f + drift * 0.45f, (float)INK_SCREEN_W + 60.0f) - 30.0f;
            const float by =
                fmodf((float)(i * 137 % 720) + drift, (float)INK_SCREEN_H + 80.0f) - 40.0f;
            const Vector2 pts[2] = {{bx, by}, {bx - 16.0f, by + 34.0f}};
            ink_stroke(r, pts, 2,
                       (BrushProfile){.w_max = 1.8f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.45f,
                                      .dry = 0.30f,
                                      .seed = 3400u + (uint32_t)i},
                       rain_tone, 1.0f);
        }
    }

    /* Mountain wind (Yamaoroshi) telegraph: 1.5s edge paper flutter + 3 sweep arrows */
    if (r->wind_telegraph_t > 0.0f) {
        const float u = ink_clampf(1.0f - r->wind_telegraph_t / 1.5f, 0.0f, 1.0f);
        const float fade = u < 0.15f ? (u / 0.15f) : (u > 0.85f ? (1.0f - u) / 0.15f : 1.0f);
        const int32_t dir = r->wind_telegraph_dir;
        const InkTone flutter_tone = {.color = PAL_GOFUN, .alpha = 0.48f * fade};
        const InkTone arrow_tone = {.color = PAL_SUMI_HEAVY, .alpha = 0.72f * fade};

        /* Edge paper flutter ribbons on upwind border */
        for (int s = 0; s < 5; ++s) {
            const float wave = sinf(r->vfx_time * 18.0f + (float)s * 1.4f) * 14.0f;
            Vector2 fp[3];
            if (dir == 0) { /* left -> right */
                const float y = 110.0f + (float)s * 125.0f;
                fp[0] = (Vector2){4.0f, y};
                fp[1] = (Vector2){36.0f + wave * 0.5f, y + wave * 0.35f};
                fp[2] = (Vector2){68.0f + wave, y - wave * 0.25f};
            } else if (dir == 1) { /* right -> left */
                const float y = 110.0f + (float)s * 125.0f;
                fp[0] = (Vector2){(float)INK_SCREEN_W - 4.0f, y};
                fp[1] = (Vector2){(float)INK_SCREEN_W - 36.0f - wave * 0.5f, y + wave * 0.35f};
                fp[2] = (Vector2){(float)INK_SCREEN_W - 68.0f - wave, y - wave * 0.25f};
            } else if (dir == 2) { /* top -> bottom */
                const float x = 180.0f + (float)s * 230.0f;
                fp[0] = (Vector2){x, 60.0f};
                fp[1] = (Vector2){x + wave * 0.35f, 92.0f + wave * 0.5f};
                fp[2] = (Vector2){x - wave * 0.25f, 124.0f + wave};
            } else { /* bottom -> top */
                const float x = 180.0f + (float)s * 230.0f;
                fp[0] = (Vector2){x, (float)INK_SCREEN_H - 4.0f};
                fp[1] = (Vector2){x + wave * 0.35f, (float)INK_SCREEN_H - 36.0f - wave * 0.5f};
                fp[2] = (Vector2){x - wave * 0.25f, (float)INK_SCREEN_H - 68.0f - wave};
            }
            ink_stroke(r, fp, 3,
                       (BrushProfile){.w_max = 18.0f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.10f,
                                      .exit = 0.45f,
                                      .dry = 0.50f,
                                      .seed = 3450u + (uint32_t)s},
                       flutter_tone, 1.0f);
        }

        /* 3 directional sweeping clear-ink arrows */
        const float reveal = ink_clampf(fmodf(u * 2.2f, 1.0f) * 1.35f, 0.15f, 1.0f);
        for (int a = 0; a < 3; ++a) {
            Vector2 p0, p1, dxv;
            if (dir == 0) {
                const float y = 190.0f + (float)a * 170.0f;
                p0 = (Vector2){36.0f, y};
                p1 = (Vector2){230.0f, y};
                dxv = (Vector2){1.0f, 0.0f};
            } else if (dir == 1) {
                const float y = 190.0f + (float)a * 170.0f;
                p0 = (Vector2){(float)INK_SCREEN_W - 36.0f, y};
                p1 = (Vector2){(float)INK_SCREEN_W - 230.0f, y};
                dxv = (Vector2){-1.0f, 0.0f};
            } else if (dir == 2) {
                const float x = 320.0f + (float)a * 320.0f;
                p0 = (Vector2){x, 76.0f};
                p1 = (Vector2){x, 240.0f};
                dxv = (Vector2){0.0f, 1.0f};
            } else {
                const float x = 320.0f + (float)a * 320.0f;
                p0 = (Vector2){x, (float)INK_SCREEN_H - 36.0f};
                p1 = (Vector2){x, (float)INK_SCREEN_H - 200.0f};
                dxv = (Vector2){0.0f, -1.0f};
            }
            const Vector2 shaft[2] = {p0, p1};
            ink_stroke(r, shaft, 2,
                       (BrushProfile){.w_max = 6.2f,
                                      .w_min_ratio = 0.24f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.45f,
                                      .seed = 3470u + (uint32_t)a},
                       arrow_tone, reveal);
            if (reveal > 0.72f) {
                const Vector2 perp = {-dxv.y, dxv.x};
                const Vector2 head[3] = {
                    {p1.x - dxv.x * 20.0f + perp.x * 14.0f, p1.y - dxv.y * 20.0f + perp.y * 14.0f},
                    p1,
                    {p1.x - dxv.x * 20.0f - perp.x * 14.0f, p1.y - dxv.y * 20.0f - perp.y * 14.0f},
                };
                ink_stroke(r, head, 3,
                           (BrushProfile){.w_max = 4.6f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.20f,
                                          .exit = 0.30f,
                                          .dry = 0.25f,
                                          .seed = 3480u + (uint32_t)a},
                           arrow_tone, 1.0f);
            }
        }
    }
}
