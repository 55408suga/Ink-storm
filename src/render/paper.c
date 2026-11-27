#include "ink_prims.h"
#include "render_internal.h"
#include "sprites.h"

#include "core.h"
#include "rlgl.h"

#include <math.h>
#include <stddef.h>

enum { PAPER_TEX_SIZE = 512 };

static const float k_paper_two_pi = 6.28318530718f;

/* ---------------------------------------------------------------- seamless washi */

static uint32_t paper_mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static float lattice(uint32_t seed, int x, int y, int period_x, int period_y)
{
    const uint32_t xi = (uint32_t)(((x % period_x) + period_x) % period_x);
    const uint32_t yi = (uint32_t)(((y % period_y) + period_y) % period_y);
    const uint32_t h = paper_mix(seed ^ paper_mix(xi * 0x8da6b343u ^ paper_mix(yi * 0xd8163841u)));
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}

/* Value noise on a torus: lattice indices wrap with the period, so the tile repeats exactly. */
static float torus_noise(uint32_t seed, float u, float v, int period_x, int period_y)
{
    const float x = u * (float)period_x;
    const float y = v * (float)period_y;
    const int x0 = (int)floorf(x);
    const int y0 = (int)floorf(y);
    const float fx = x - (float)x0;
    const float fy = y - (float)y0;
    const float sx = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    const float sy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);
    const float a = lattice(seed, x0, y0, period_x, period_y);
    const float b = lattice(seed, x0 + 1, y0, period_x, period_y);
    const float c = lattice(seed, x0, y0 + 1, period_x, period_y);
    const float d = lattice(seed, x0 + 1, y0 + 1, period_x, period_y);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

static void deposit(Color *px, float x, float y, float amount)
{
    /* Bilinear splat with wrap-around keeps fibres continuous across the tile seam. */
    const int x0 = (int)floorf(x);
    const int y0 = (int)floorf(y);
    const float fx = x - (float)x0;
    const float fy = y - (float)y0;
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    for (int k = 0; k < 4; ++k) {
        const int xx = ((x0 + (k & 1)) % PAPER_TEX_SIZE + PAPER_TEX_SIZE) % PAPER_TEX_SIZE;
        const int yy = ((y0 + (k >> 1)) % PAPER_TEX_SIZE + PAPER_TEX_SIZE) % PAPER_TEX_SIZE;
        Color *c = &px[yy * PAPER_TEX_SIZE + xx];
        const float d = amount * w[k];
        c->r = (unsigned char)ink_clampf((float)c->r + d, 0.0f, 255.0f);
        c->g = (unsigned char)ink_clampf((float)c->g + d, 0.0f, 255.0f);
        c->b = (unsigned char)ink_clampf((float)c->b + d * 1.08f, 0.0f, 255.0f);
    }
}

static void lay_fibres(Color *px, uint64_t *rng)
{
    /* Curved kozo (mulberry) fibres: thin, long, slightly brighter or darker than the sheet. */
    for (int f = 0; f < 420; ++f) {
        float x = ink_rng_f01(rng) * (float)PAPER_TEX_SIZE;
        float y = ink_rng_f01(rng) * (float)PAPER_TEX_SIZE;
        float ang = ink_rng_f01(rng) * k_paper_two_pi;
        const int len = 16 + (int)(ink_rng_f01(rng) * 70.0f);
        const int steps = len * 2;
        /* At most ~40 degrees of total bend: fibres sweep, they never curl into rings. */
        const float bend = (ink_rng_f01(rng) - 0.5f) * 1.4f / (float)steps;
        const bool bright = ink_rng_f01(rng) < 0.7f;
        const float amount =
            bright ? 2.0f + ink_rng_f01(rng) * 3.5f : -(1.2f + ink_rng_f01(rng) * 2.2f);
        for (int s = 0; s < steps; ++s) {
            const float taper = sinf(3.14159265f * (float)s / (float)steps);
            deposit(px, x, y, amount * taper);
            x += cosf(ang) * 0.5f;
            y += sinf(ang) * 0.5f;
            ang += bend;
        }
    }
}

static void build_washi(Color *px)
{
    for (int y = 0; y < PAPER_TEX_SIZE; ++y) {
        const float v = ((float)y + 0.5f) / (float)PAPER_TEX_SIZE;
        for (int x = 0; x < PAPER_TEX_SIZE; ++x) {
            const float u = ((float)x + 0.5f) / (float)PAPER_TEX_SIZE;
            const float mottle =
                torus_noise(0x11u, u, v, 3, 3) * 0.6f + torus_noise(0x12u, u, v, 6, 6) * 0.4f;
            const float cloud =
                torus_noise(0x21u, u, v, 16, 16) * 0.6f + torus_noise(0x22u, u, v, 32, 32) * 0.4f;
            const float tooth = torus_noise(0x31u, u, v, 128, 128) * 0.5f +
                                lattice(0x32u, x, y, PAPER_TEX_SIZE, PAPER_TEX_SIZE) * 0.5f;
            const float laid = torus_noise(0x41u, u, v, 4, 96);
            const float lum = 245.0f + (mottle - 0.5f) * 8.0f + (cloud - 0.5f) * 7.0f +
                              (tooth - 0.5f) * 6.0f + (laid - 0.5f) * 2.5f;
            /* Darker mottles are a touch warmer, like sizing that settled unevenly. */
            const float warm = (0.5f - mottle) * 3.0f;
            px[y * PAPER_TEX_SIZE + x] = (Color){
                (unsigned char)ink_clampf(lum, 224.0f, 255.0f),
                (unsigned char)ink_clampf(lum - 0.5f, 224.0f, 255.0f),
                (unsigned char)ink_clampf(lum - 1.5f - warm, 220.0f, 255.0f),
                255,
            };
        }
    }
    uint64_t rng = UINT64_C(0x5741534849504150);
    lay_fibres(px, &rng);
}

void render_paper_init(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    /* Near-white, almost achromatic sheet: multiplying by pal_paper_for_act(act) keeps each act's
     * paper lightness instead of darkening it twice. */
    Image img = GenImageColor(PAPER_TEX_SIZE, PAPER_TEX_SIZE, pal_white());
    if (img.data != NULL) {
        build_washi((Color *)img.data);
        r->paper_tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    if (r->paper_tex.id != 0) {
        SetTextureFilter(r->paper_tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(r->paper_tex, TEXTURE_WRAP_REPEAT);
    }

    r->bg_cache =
        LoadRenderTexture(INK_SCREEN_W + 2 * RENDER_BG_PAD, INK_SCREEN_H + 2 * RENDER_BG_PAD);
    if (!render_texture_usable(r->bg_cache)) {
        if (r->bg_cache.id != 0) {
            UnloadRenderTexture(r->bg_cache);
        }
        r->bg_cache = (RenderTexture2D){0};
        TraceLog(LOG_WARNING, "INKSTORM: background cache unavailable; painting scenery directly");
    } else {
        SetTextureFilter(r->bg_cache.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(r->bg_cache.texture, TEXTURE_WRAP_CLAMP);
    }
    r->bg_cached_kind = -1;
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
    r->bg_cached_kind = -1;
    r->bg_cached_act = -1;
}

static PaletteId paper_for_key(BgKey key)
{
    return key.kind == BG_LOBBY ? PAL_PAPER_ACT1 : pal_paper_for_act(key.act_index);
}

/* Paints paper + scenery in world coordinates over the padded area around the screen. */
static void paint_background(RenderState *r, BgKey key)
{
    const Color tint = pal_color(paper_for_key(key));
    const Rectangle area = {-(float)RENDER_BG_PAD, -(float)RENDER_BG_PAD,
                            (float)(INK_SCREEN_W + 2 * RENDER_BG_PAD),
                            (float)(INK_SCREEN_H + 2 * RENDER_BG_PAD)};
    DrawRectangleRec(area, tint);
    if (r->paper_tex.id != 0) {
        DrawTexturePro(r->paper_tex, area, area, (Vector2){0.0f, 0.0f}, 0.0f, tint);
    }
    render_scenery_draw(r, key);
}

static void bake_background(RenderState *r, BgKey key)
{
    BeginTextureMode(r->bg_cache);
    ClearBackground(pal_color(paper_for_key(key)));
    /* Colour blends normally but destination alpha stays 1, so the cache is opaque and drawing
     * it back never squares translucent stroke alpha. */
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                              RL_FUNC_ADD);
    render_set_blend(r, BLEND_CUSTOM_SEPARATE);
    rlPushMatrix();
    rlTranslatef((float)RENDER_BG_PAD, (float)RENDER_BG_PAD, 0.0f);
    paint_background(r, key);
    rlPopMatrix();
    render_reset_blend(r);
    EndTextureMode();
    r->bg_cached_kind = (int32_t)key.kind;
    r->bg_cached_act = key.act_index;
}

void render_paper_prepare_bg(RenderState *r, BgKey key)
{
    if (r == NULL || r->bg_cache.id == 0) {
        return;
    }
    const BgKey cached = {.kind = (BgKind)r->bg_cached_kind, .act_index = r->bg_cached_act};
    if (r->bg_cached_kind < 0 || !render_bg_key_equal(cached, key)) {
        bake_background(r, key);
    }
}

void render_paper_draw_bg(RenderState *r, BgKey key)
{
    if (r == NULL) {
        return;
    }
    const BgKey cached = {.kind = (BgKind)r->bg_cached_kind, .act_index = r->bg_cached_act};
    if (r->bg_cache.id == 0 || r->bg_cached_kind < 0 || !render_bg_key_equal(cached, key)) {
        render_set_blend(r, BLEND_ALPHA);
        paint_background(r, key);
        return;
    }
    const Texture2D tex = r->bg_cache.texture;
    const Rectangle src = {0.0f, 0.0f, (float)tex.width, -(float)tex.height};
    const Rectangle dst = {-(float)RENDER_BG_PAD, -(float)RENDER_BG_PAD, (float)tex.width,
                           (float)tex.height};
    render_set_blend(r, BLEND_ALPHA);
    DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
}

/* ---------------------------------------------------------------- terrain */

static void fill_quad(RenderState *r, Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col)
{
    const Vector2 q[4] = {a, b, c, d};
    ink_fill_poly(r, q, 4, col);
}

static float rock_jitter(uint32_t seed, uint32_t i)
{
    return (float)(paper_mix(seed * 0x9e3779b9u ^ paper_mix(i + 17u)) >> 8) * (1.0f / 16777216.0f);
}

static void draw_rock_pillar(RenderState *r, Vector2 c, float rad, uint32_t seed, int act_index)
{
    const SpriteId sid = render_sprite_resolve(r, SPR_TERRAIN_ROCK);
    if (sid != SPR_NONE) {
        const SpriteDef *d = sprite_def(sid);
        ink_radial_glow(r, (Vector2){c.x + rad * 0.35f, c.y + rad * 0.42f}, rad * 1.25f,
                        rad * 0.90f, pal_color_alpha(PAL_SUMI_HEAVY, 0.24f),
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.0f));
        const SpriteXform xf = {
            .x = c.x,
            .y = c.y,
            .size = sprite_display_size(d, rad),
            .sx = (seed & 1u) != 0u ? -1.0f : 1.0f,
            .sy = 1.0f,
            .rot = ((float)(seed % 7u) - 3.0f) * 0.035f,
        };
        render_sprite_draw_halo(r, sid, &xf,
                                pal_lerp(pal_paper_for_act(act_index), PAL_GOFUN, 0.5f, 0.85f));
        render_sprite_draw(r, sid, &xf, pal_white());
        return;
    }

    /* A standing stone seen from above at a slight tilt: shaded side walls, a lit and uneven crown,
     * shunpo veins, moss dots and a crisp contour, resting on pale mist with a shadow cast to the
     * lower right. */
    enum { V = 11 };
    ink_radial_glow(r, c, rad * 1.9f, rad * 1.5f, pal_color_alpha(PAL_GOFUN, 0.6f),
                    pal_color_alpha(PAL_GOFUN, 0.0f));
    ink_radial_glow(r, (Vector2){c.x + rad * 0.5f, c.y + rad * 0.6f}, rad * 1.45f, rad * 1.0f,
                    pal_color_alpha(PAL_SUMI_HEAVY, 0.36f), pal_color_alpha(PAL_SUMI_HEAVY, 0.0f));

    Vector2 base[V];
    Vector2 crown[V];
    const float lift = rad * 0.30f;
    for (int i = 0; i < V; ++i) {
        const float a = k_paper_two_pi * (float)i / (float)V + 0.25f;
        const float jb = 0.90f + 0.20f * rock_jitter(seed, (uint32_t)i);
        const float jc = 0.68f + 0.22f * rock_jitter(seed + 1u, (uint32_t)i);
        base[i] = (Vector2){c.x + cosf(a) * rad * jb, c.y + sinf(a) * rad * jb};
        crown[i] = (Vector2){c.x - rad * 0.05f + cosf(a) * rad * jc,
                             c.y - lift + sinf(a) * rad * jc * 0.82f};
    }
    ink_fill_poly(r, base, V, pal_color_alpha(PAL_SUMI_HEAVY, 0.97f));
    for (int i = 0; i < V; ++i) {
        const int n = (i + 1) % V;
        const float mx = (base[i].x + base[n].x) * 0.5f - c.x;
        const float my = (base[i].y + base[n].y) * 0.5f - c.y;
        const float lit = ink_clampf(0.55f - 0.6f * (mx * 0.8f + my * 0.6f) / rad, 0.0f, 1.0f);
        fill_quad(r, crown[i], base[i], base[n], crown[n],
                  pal_lerp(PAL_SUMI_DARK, PAL_SUMI_LIGHT, lit, 0.96f));
    }
    ink_fill_poly(r, crown, V, pal_color_alpha(PAL_SUMI_CLEAR, 0.98f));
    const Vector2 crown_c = {c.x - rad * 0.05f, c.y - lift};
    const Vector2 shade[5] = {crown_c, crown[0], crown[1], crown[2], crown[3]};
    ink_fill_poly(r, shade, 5, pal_color_alpha(PAL_SUMI_LIGHT, 0.42f));
    ink_wash(r, (Vector2){crown_c.x - rad * 0.22f, crown_c.y - rad * 0.16f}, rad * 0.36f,
             rad * 0.22f, -0.3f, (InkTone){.color = PAL_GOFUN, .alpha = 0.6f}, 1, seed + 1u);

    /* Shunpo: a few uneven curved veins of varying length rather than even stripes. */
    for (int k = 0; k < 3; ++k) {
        const float ox =
            (-0.40f + 0.34f * (float)k + (rock_jitter(seed + 2u, (uint32_t)k) - 0.5f) * 0.2f) * rad;
        const float top = -lift - rad * (0.25f + 0.35f * rock_jitter(seed + 3u, (uint32_t)k));
        const float bot = rad * (0.15f + 0.55f * rock_jitter(seed + 4u, (uint32_t)k));
        const float bend = (rock_jitter(seed + 5u, (uint32_t)k) - 0.5f) * rad * 0.35f;
        const Vector2 v[3] = {{c.x + ox, c.y + top},
                              {c.x + ox + bend, c.y + (top + bot) * 0.5f},
                              {c.x + ox + bend * 0.4f, c.y + bot}};
        ink_stroke(r, v, 3,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.2f,
                                  .entry = 0.2f,
                                  .exit = 0.5f,
                                  .dry = 0.6f,
                                  .seed = seed + 10u + (uint32_t)k},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.6f}, 1.0f);
    }
    const Vector2 crack[3] = {{c.x + rad * 0.20f, c.y - lift - rad * 0.05f},
                              {c.x + rad * 0.36f, c.y + rad * 0.18f},
                              {c.x + rad * 0.30f, c.y + rad * 0.72f}};
    ink_stroke(r, crack, 3,
               (BrushProfile){.w_max = 3.0f,
                              .w_min_ratio = 0.3f,
                              .entry = 0.1f,
                              .exit = 0.5f,
                              .dry = 0.3f,
                              .seed = seed + 30u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.85f}, 1.0f);
    for (int k = 0; k < 4; ++k) {
        const Vector2 dot = crown[(seed + (uint32_t)k * 3u) % V];
        ink_wash(
            r, (Vector2){dot.x + (crown_c.x - dot.x) * 0.15f, dot.y + (crown_c.y - dot.y) * 0.15f},
            2.2f, 1.6f, 0.3f, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.8f}, 1,
            seed + 50u + (uint32_t)k);
    }
    ink_outline_tone(r, crown, V, true, 2.0f, (Vector2){-0.6f, -0.8f},
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.7f}, seed + 40u);
    ink_outline(r, base, V, true, 4.6f, (Vector2){0.707f, 0.707f}, seed + 3u);
}

static void draw_torii(RenderState *r, float cx, uint32_t seed, int act_index)
{
    const SpriteId sid = render_sprite_resolve(r, SPR_TERRAIN_TORII);
    if (sid != SPR_NONE) {
        const SpriteDef *d = sprite_def(sid);
        ink_radial_glow(r, (Vector2){cx + 6.0f, 332.0f}, 18.0f, 15.0f,
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.24f),
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.0f));
        ink_radial_glow(r, (Vector2){cx + 6.0f, 396.0f}, 18.0f, 15.0f,
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.24f),
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.0f));
        const SpriteXform xf = {
            .x = cx - 4.0f,
            .y = 360.0f,
            .size = sprite_display_size(d, 0.0f),
            .sx = 1.0f,
            .sy = 1.0f,
        };
        render_sprite_draw_halo(r, sid, &xf,
                                pal_lerp(pal_paper_for_act(act_index), PAL_GOFUN, 0.5f, 0.85f));
        render_sprite_draw(r, sid, &xf, pal_white());
        return;
    }

    /* Solid pillars at (cx, 328) and (cx, 392) [16x16px]; the beams are visual only. Seen from
     * above: foundation stones, round bengara posts (not PAL_SHU, which fire owns), a nuki tie-beam
     * and the dark kasagi cap, all casting one soft shadow to the lower right. */
    const float py[2] = {328.0f, 392.0f};
    const float kx = cx - 6.0f;
    fill_quad(r, (Vector2){kx + 2.0f, 306.0f}, (Vector2){kx + 22.0f, 312.0f},
              (Vector2){kx + 22.0f, 424.0f}, (Vector2){kx + 2.0f, 418.0f},
              pal_color_alpha(PAL_SUMI_HEAVY, 0.16f));
    for (int i = 0; i < 2; ++i) {
        ink_radial_glow(r, (Vector2){cx + 6.0f, py[i] + 6.0f}, 18.0f, 14.0f,
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.30f),
                        pal_color_alpha(PAL_SUMI_HEAVY, 0.0f));
        Vector2 stone[8];
        for (int k = 0; k < 8; ++k) {
            const float a = k_paper_two_pi * ((float)k + 0.5f) / 8.0f;
            stone[k] = (Vector2){cx + cosf(a) * 14.0f, py[i] + sinf(a) * 13.0f};
        }
        ink_fill_poly(r, stone, 8, pal_color_alpha(PAL_SUMI_CLEAR, 0.95f));
        ink_outline_tone(r, stone, 8, true, 1.8f, (Vector2){0.7f, 0.7f},
                         (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.8f}, seed + (uint32_t)i);
        ink_radial_glow(r, (Vector2){cx, py[i]}, 9.5f, 9.5f, pal_color_alpha(PAL_BENGARA, 1.0f),
                        pal_lerp(PAL_BENGARA, PAL_SUMI_DARK, 0.25f, 1.0f));
        ink_radial_glow(r, (Vector2){cx - 3.0f, py[i] - 3.0f}, 3.6f, 3.0f,
                        pal_lerp(PAL_BENGARA, PAL_GOFUN, 0.45f, 0.55f),
                        pal_lerp(PAL_BENGARA, PAL_GOFUN, 0.45f, 0.0f));
        ink_arc_stroke(r, (Vector2){cx, py[i]}, 9.5f, -0.6f, 2.4f,
                       (BrushProfile){.w_max = 2.6f,
                                      .w_min_ratio = 0.3f,
                                      .entry = 0.1f,
                                      .exit = 0.2f,
                                      .dry = 0.1f,
                                      .seed = seed + 8u + (uint32_t)i},
                       (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.85f});
    }

    /* Nuki: bengara tie-beam with a darker underside. */
    const BrushProfile beam = {.w_max = 6.0f,
                               .w_min_ratio = 0.6f,
                               .entry = 0.04f,
                               .exit = 0.04f,
                               .dry = 0.1f,
                               .seed = seed + 30u};
    const Vector2 nuki[2] = {{cx + 3.0f, 316.0f}, {cx + 3.0f, 404.0f}};
    ink_stroke(r, nuki, 2, beam, (InkTone){.color = PAL_BENGARA, .alpha = 0.95f}, 1.0f);
    const Vector2 nuki_shade[2] = {{cx + 6.0f, 318.0f}, {cx + 6.0f, 402.0f}};
    ink_stroke(r, nuki_shade, 2,
               (BrushProfile){.w_max = 1.6f,
                              .w_min_ratio = 0.5f,
                              .entry = 0.05f,
                              .exit = 0.05f,
                              .dry = 0.2f,
                              .seed = seed + 31u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.6f}, 1.0f);

    /* Kasagi: lacquered cap beam with upswept ends, a lit edge and a shadowed edge. */
    const Vector2 kasagi[4] = {
        {kx - 4.0f, 300.0f}, {kx, 330.0f}, {kx, 390.0f}, {kx - 4.0f, 420.0f}};
    ink_stroke(r, kasagi, 4,
               (BrushProfile){.w_max = 12.0f,
                              .w_min_ratio = 0.55f,
                              .entry = 0.06f,
                              .exit = 0.06f,
                              .dry = 0.15f,
                              .seed = seed + 35u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.96f}, 1.0f);
    const Vector2 kasagi_lit[4] = {
        {kx - 8.0f, 302.0f}, {kx - 4.5f, 330.0f}, {kx - 4.5f, 390.0f}, {kx - 8.0f, 418.0f}};
    ink_stroke(r, kasagi_lit, 4,
               (BrushProfile){.w_max = 2.0f,
                              .w_min_ratio = 0.5f,
                              .entry = 0.1f,
                              .exit = 0.1f,
                              .dry = 0.35f,
                              .seed = seed + 36u},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.75f}, 1.0f);
    const Vector2 shimaki[2] = {{kx + 7.0f, 310.0f}, {kx + 7.0f, 410.0f}};
    ink_stroke(r, shimaki, 2,
               (BrushProfile){.w_max = 5.0f,
                              .w_min_ratio = 0.6f,
                              .entry = 0.05f,
                              .exit = 0.05f,
                              .dry = 0.2f,
                              .seed = seed + 37u},
               (InkTone){.color = PAL_BENGARA, .alpha = 0.9f}, 1.0f);
    for (int g = 0; g < 3; ++g) {
        const float gy = 322.0f + (float)g * 34.0f;
        const Vector2 grain[2] = {{kx + 1.0f, gy}, {kx + 1.0f, gy + 18.0f}};
        ink_stroke(r, grain, 2,
                   (BrushProfile){.w_max = 1.2f,
                                  .w_min_ratio = 0.5f,
                                  .entry = 0.2f,
                                  .exit = 0.3f,
                                  .dry = 0.5f,
                                  .seed = seed + 40u + (uint32_t)g},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.6f}, 1.0f);
    }
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
        draw_torii(r, 384.0f, 3010u, act_index);
        draw_torii(r, 896.0f, 3020u, act_index);
        draw_rock_pillar(r, (Vector2){340.0f, 190.0f}, 24.0f, 3101u, act_index);
        draw_rock_pillar(r, (Vector2){940.0f, 190.0f}, 24.0f, 3102u, act_index);
        draw_rock_pillar(r, (Vector2){340.0f, 530.0f}, 24.0f, 3103u, act_index);
        draw_rock_pillar(r, (Vector2){940.0f, 530.0f}, 24.0f, 3104u, act_index);
    } else if (stage_act == 2) {
        /* Act 3: upper 2 rock pillars */
        draw_rock_pillar(r, (Vector2){340.0f, 190.0f}, 24.0f, 3201u, act_index);
        draw_rock_pillar(r, (Vector2){940.0f, 190.0f}, 24.0f, 3202u, act_index);
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

    /* Act 3 diagonal clear-ink rain lines drifting continuously with independent wrap phases */
    const int stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0;
    if (stage_act == 2) {
        const InkTone rain_tone = {.color = PAL_SUMI_CLEAR, .alpha = 0.30f};
        for (int i = 0; i < 16; ++i) {
            float bx = 0.0f;
            float by = 0.0f;
            render_rain_streak_pos(i, r->rain_phase_x, r->rain_phase_y, &bx, &by);
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
