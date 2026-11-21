#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

enum { PAPER_TEX_SIZE = 256 };

void render_paper_init(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    Image img = GenImagePerlinNoise(PAPER_TEX_SIZE, PAPER_TEX_SIZE, 13, 29, 3.2f);
    Color *px = LoadImageColors(img);
    if (px != NULL) {
        uint64_t rng = UINT64_C(0x5741534849504150);
        const Color base = pal_color(PAL_PAPER_ACT1);
        for (int y = 0; y < PAPER_TEX_SIZE; ++y) {
            for (int x = 0; x < PAPER_TEX_SIZE; ++x) {
                const int idx = y * PAPER_TEX_SIZE + x;
                const float n = ((float)px[idx].r / 255.0f) - 0.5f;
                const float grain = n * 14.0f + (ink_rng_f01(&rng) - 0.5f) * 5.0f;
                const float r_v = ink_clampf((float)base.r + grain, 0.0f, 255.0f);
                const float g_v = ink_clampf((float)base.g + grain * 0.94f, 0.0f, 255.0f);
                const float b_v = ink_clampf((float)base.b + grain * 0.82f, 0.0f, 255.0f);
                px[idx].r = (unsigned char)r_v;
                px[idx].g = (unsigned char)g_v;
                px[idx].b = (unsigned char)b_v;
                px[idx].a = 255;
            }
        }
        /* Subtle mulberry fiber lines baked into CPU paper texture */
        for (int f = 0; f < 42; ++f) {
            int fx = (int)(ink_rng_next(&rng) % (uint64_t)PAPER_TEX_SIZE);
            int fy = (int)(ink_rng_next(&rng) % (uint64_t)PAPER_TEX_SIZE);
            const int len = 10 + (int)(ink_rng_next(&rng) % 24u);
            const bool horiz = (ink_rng_next(&rng) & 1u) != 0u;
            const int delta = (ink_rng_next(&rng) & 2u) ? 5 : -6;
            for (int k = 0; k < len; ++k) {
                const int px_x = (fx + (horiz ? k : 0)) % PAPER_TEX_SIZE;
                const int px_y = (fy + (horiz ? 0 : k)) % PAPER_TEX_SIZE;
                Color *c = &px[px_y * PAPER_TEX_SIZE + px_x];
                c->r = (unsigned char)ink_clampf((float)c->r + (float)delta, 0.0f, 255.0f);
                c->g = (unsigned char)ink_clampf((float)c->g + (float)delta * 0.9f, 0.0f, 255.0f);
                c->b = (unsigned char)ink_clampf((float)c->b + (float)delta * 0.8f, 0.0f, 255.0f);
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
    ClearBackground(pal_color(paper_pal));
    if (r->paper_tex.id != 0) {
        const Rectangle src = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
        const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
        /* Tint white for act 1 (already baked to ACT1), slight paper tint for others */
        const Color tint = act_index == 0 ? pal_white() : pal_color(paper_pal);
        DrawTexturePro(r->paper_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, tint);
    }
}

static void bake_act_background(RenderState *r, int act_index)
{
    if (r->bg_cache.id == 0) {
        return;
    }
    BeginTextureMode(r->bg_cache);
    ClearBackground(pal_blank());

    /* Act 1 edges only (within 120px margin, opacity <= 0.35, preserving >= 80% empty paper) */
    const InkTone mist_tone = {.color = PAL_SUMI_CLEAR, .alpha = 0.28f};
    ink_wash(r, (Vector2){220.0f, 44.0f}, 190.0f, 38.0f, -0.05f, mist_tone, 2, 1001u);
    ink_wash(r, (Vector2){650.0f, 36.0f}, 240.0f, 34.0f, 0.04f, mist_tone, 2, 1002u);
    ink_wash(r, (Vector2){1060.0f, 50.0f}, 180.0f, 42.0f, -0.06f, mist_tone, 2, 1003u);

    /* Bottom edge susuki grass (4 gentle pale-ink flick strokes within 120px of bottom) */
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
        const BrushProfile prof = {
            .w_max = 4.2f,
            .w_min_ratio = 0.28f,
            .entry = 0.12f,
            .exit = 0.55f,
            .dry = 0.45f,
            .seed = 2000u + (uint32_t)i * 31u + (uint32_t)act_index * 7u,
        };
        ink_stroke(r, pts, 4, prof, grass_tone, 1.0f);
    }

    EndTextureMode();
    r->bg_cached_act = act_index;
}

void render_paper_draw_bg(RenderState *r, int act_index)
{
    if (r->bg_cache.id == 0) {
        return;
    }
    if (r->bg_cached_act != act_index) {
        bake_act_background(r, act_index);
    }
    const Rectangle src = {0.0f, 0.0f, (float)r->bg_cache.texture.width,
                           -(float)r->bg_cache.texture.height};
    const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
    DrawTexturePro(r->bg_cache.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
}
