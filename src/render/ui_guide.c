#include "ink_prims.h"
#include "render_internal.h"
#include "sprites.h"

#include "core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float k_guide_pi = 3.14159265359f;
static const float k_guide_two_pi = 6.28318530718f;

typedef struct {
    TextId title;
    TextId head[3];
    TextId body[3];
    PaletteId accent[3];
    TextId tip;
} GuideSlideText;

static const GuideSlideText k_slides[GUIDE_SLIDE_COUNT] = {
    {
        .title = TEXT_GUIDE_S0_TITLE,
        .head = {TEXT_GUIDE_S0_H1, TEXT_GUIDE_S0_H2, TEXT_GUIDE_S0_H3},
        .body = {TEXT_GUIDE_S0_B1, TEXT_GUIDE_S0_B2, TEXT_GUIDE_S0_B3},
        .accent = {PAL_BENGARA, PAL_SUMI_SCORCH, PAL_SHU},
        .tip = TEXT_GUIDE_S0_TIP,
    },
    {
        .title = TEXT_GUIDE_S1_TITLE,
        .head = {TEXT_GUIDE_S1_H1, TEXT_GUIDE_S1_H2, TEXT_GUIDE_S1_H3},
        .body = {TEXT_GUIDE_S1_B1, TEXT_GUIDE_S1_B2, TEXT_GUIDE_S1_B3},
        .accent = {PAL_SUMI_SCORCH, PAL_AO, PAL_BENGARA},
        .tip = TEXT_GUIDE_S1_TIP,
    },
    {
        .title = TEXT_GUIDE_S2_TITLE,
        .head = {TEXT_GUIDE_S2_H1, TEXT_GUIDE_S2_H2, TEXT_GUIDE_S2_H3},
        .body = {TEXT_GUIDE_S2_B1, TEXT_GUIDE_S2_B2, TEXT_GUIDE_S2_B3},
        .accent = {PAL_SHU, PAL_BENGARA, PAL_SUMI_SCORCH},
        .tip = TEXT_GUIDE_S2_TIP,
    },
    {
        .title = TEXT_GUIDE_S3_TITLE,
        .head = {TEXT_GUIDE_S3_H1, TEXT_GUIDE_S3_H2, TEXT_GUIDE_S3_H3},
        .body = {TEXT_GUIDE_S3_B1, TEXT_GUIDE_S3_B2, TEXT_GUIDE_S3_B3},
        .accent = {PAL_BENGARA, PAL_AO, PAL_SUMI_SCORCH},
        .tip = TEXT_GUIDE_S3_TIP,
    },
    {
        .title = TEXT_GUIDE_S4_TITLE,
        .head = {TEXT_GUIDE_S4_H1, TEXT_GUIDE_S4_H2, TEXT_GUIDE_S4_H3},
        .body = {TEXT_GUIDE_S4_B1, TEXT_GUIDE_S4_B2, TEXT_GUIDE_S4_B3},
        .accent = {PAL_SHU, PAL_AO, PAL_BENGARA},
        .tip = TEXT_GUIDE_S4_TIP,
    },
};

void render_guide_layout(GuideLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->panel_rect = (Rectangle){80.0f, 50.0f, 1120.0f, 620.0f};
    out->close_rect = (Rectangle){1034.0f, 68.0f, 134.0f, 38.0f};
    out->prev_rect = (Rectangle){116.0f, 582.0f, 174.0f, 44.0f};
    out->next_rect = (Rectangle){990.0f, 582.0f, 174.0f, 44.0f};

    const float dot_w = 54.0f;
    const float dot_h = 36.0f;
    const float dot_gap = 12.0f;
    const float total_w =
        (float)GUIDE_SLIDE_COUNT * dot_w + (float)(GUIDE_SLIDE_COUNT - 1) * dot_gap;
    const float x0 = ((float)INK_SCREEN_W - total_w) * 0.5f;
    for (int i = 0; i < GUIDE_SLIDE_COUNT; ++i) {
        out->dots[i] = (Rectangle){x0 + (float)i * (dot_w + dot_gap), 586.0f, dot_w, dot_h};
    }
}

static void draw_rect_solid(RenderState *r, Rectangle rc, Color c)
{
    if (rc.width <= 0.5f || rc.height <= 0.5f || c.a == 0) {
        return;
    }
    const Vector2 pts[4] = {
        {rc.x, rc.y},
        {rc.x + rc.width, rc.y},
        {rc.x + rc.width, rc.y + rc.height},
        {rc.x, rc.y + rc.height},
    };
    ink_fill_poly(r, pts, 4, c);
}

static void draw_keycap(RenderState *r, Rectangle rc, const char *label, PaletteId border,
                        uint32_t seed)
{
    ink_fill_panel(r, rc, PAL_SUMI_SCORCH, 0.95f, border, 0.94f, seed);
    render_draw_text_fit(r, label, (Vector2){rc.x + rc.width * 0.5f, rc.y + rc.height * 0.5f},
                         rc.width - 12.0f, 14.0f, 10.0f, PAL_GOFUN, 0.98f, false);
}

static void draw_arrow_head(RenderState *r, Vector2 tip, Vector2 dir, float len, float wid, Color c)
{
    const float mag = hypotf(dir.x, dir.y);
    if (mag <= 1e-4f) {
        return;
    }
    const float ux = dir.x / mag;
    const float uy = dir.y / mag;
    const float nx = -uy;
    const float ny = ux;
    const Vector2 tri[3] = {
        tip,
        {tip.x - ux * len + nx * wid, tip.y - uy * len + ny * wid},
        {tip.x - ux * len - nx * wid, tip.y - uy * len - ny * wid},
    };
    ink_fill_poly(r, tri, 3, c);
}

static void draw_fallback_glyph(RenderState *r, SpriteId req, Vector2 pos, float size_px)
{
    const float rad = size_px * 0.30f;
    if (req == SPR_SHRINE) {
        const Rectangle roof = {pos.x - rad * 1.15f, pos.y - rad * 0.72f, rad * 2.30f, rad * 0.30f};
        draw_rect_solid(r, roof, pal_color_alpha(PAL_BENGARA, 0.92f));
        draw_rect_solid(
            r, (Rectangle){pos.x - rad * 0.65f, pos.y - rad * 0.42f, rad * 0.24f, rad * 1.15f},
            pal_color_alpha(PAL_SUMI_DARK, 0.92f));
        draw_rect_solid(
            r, (Rectangle){pos.x + rad * 0.41f, pos.y - rad * 0.42f, rad * 0.24f, rad * 1.15f},
            pal_color_alpha(PAL_SUMI_DARK, 0.92f));
        ink_teardrop(r, (Vector2){pos.x, pos.y + rad * 0.15f}, (Vector2){0.0f, -1.0f}, rad * 0.55f,
                     rad * 0.34f, pal_color_alpha(PAL_SHU, 0.94f));
    } else if (req == SPR_LANTERN_FIRE || req == SPR_LANTERN_STORM) {
        const PaletteId p = req == SPR_LANTERN_FIRE ? PAL_SHU : PAL_AO;
        const PaletteId hot = req == SPR_LANTERN_FIRE ? PAL_SHU_HOT : PAL_AO_HOT;
        ink_wash(r, pos, rad * 0.85f, rad * 0.95f, 0.0f, (InkTone){.color = p, .alpha = 0.86f}, 2,
                 1201u);
        ink_teardrop(r, pos, (Vector2){0.0f, -1.0f}, rad * 0.75f, rad * 0.52f,
                     pal_color_alpha(hot, 0.95f));
    } else if (req == SPR_TERRAIN_ROCK) {
        const Vector2 rock[6] = {
            {pos.x - rad * 0.90f, pos.y + rad * 0.70f}, {pos.x - rad * 0.75f, pos.y - rad * 0.45f},
            {pos.x - rad * 0.10f, pos.y - rad * 0.95f}, {pos.x + rad * 0.70f, pos.y - rad * 0.55f},
            {pos.x + rad * 0.92f, pos.y + rad * 0.65f}, {pos.x, pos.y + rad * 0.85f},
        };
        ink_fill_poly(r, rock, 6, pal_color_alpha(PAL_SUMI_HEAVY, 0.92f));
        ink_outline(r, rock, 6, true, 3.0f, (Vector2){0.7f, 0.7f}, 1202u);
    } else {
        ink_wash(r, pos, rad * 0.85f, rad * 0.85f, 0.0f,
                 (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.88f}, 2, 1203u);
        ink_arc_stroke(r, pos, rad * 0.90f, 0.2f, 5.8f,
                       (BrushProfile){.w_max = 3.0f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.1f,
                                      .exit = 0.2f,
                                      .dry = 0.2f,
                                      .seed = 1204u},
                       (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f});
    }
}

static void draw_guide_sprite(RenderState *r, SpriteId req, Vector2 pos, float size_px, float sx,
                              float sy, float rot)
{
    const SpriteId sid = render_sprite_resolve(r, req);
    if (sid == SPR_NONE) {
        draw_fallback_glyph(r, req, pos, size_px);
        return;
    }
    const SpriteXform xf = {
        .x = pos.x,
        .y = pos.y,
        .size = size_px,
        .sx = sx,
        .sy = sy,
        .rot = rot,
        .flip_v = false,
        .dx = 0.0f,
        .dy = 0.0f,
    };
    render_sprite_draw_halo(r, sid, &xf, ink_knockout_color(0));
    render_sprite_draw(r, sid, &xf, pal_white());
}

/* Slide 0 (1/5): Core Rules — Protect Shrine, Clear Wave Quota, Face Boss */
static void draw_slide0_diagram(RenderState *r, Rectangle pic, float t)
{
    const float bob = sinf(t * 2.4f) * 3.0f;

    /* Top: Miniature dark-lacquer Objective Plaque */
    const Rectangle mini_obj = {pic.x + 150.0f, pic.y + 18.0f, 260.0f, 64.0f};
    ink_fill_panel(r, mini_obj, PAL_SUMI_SCORCH, 0.95f, PAL_SHU, 0.95f, 1301u);
    char qtxt[48];
    if (r->use_ja) {
        snprintf(qtxt, sizeof qtxt, "%s 6 %s", text_get(TEXT_HUD_REMAINING, true),
                 text_get(TEXT_HUD_REMAINING_UNIT, true));
    } else {
        snprintf(qtxt, sizeof qtxt, "6 %s", text_get(TEXT_HUD_REMAINING_UNIT, false));
    }
    render_draw_text_fit(r, qtxt, (Vector2){mini_obj.x + 88.0f, mini_obj.y + 23.0f}, 150.0f, 21.0f,
                         15.0f, PAL_GOFUN, 0.98f, false);
    render_draw_number_fixed(r, "6/12",
                             (Vector2){mini_obj.x + mini_obj.width - 52.0f, mini_obj.y + 15.0f},
                             16.0f, PAL_SUMI_CLEAR, 0.95f);
    draw_rect_solid(
        r, (Rectangle){mini_obj.x + 16.0f, mini_obj.y + 44.0f, mini_obj.width - 32.0f, 9.0f},
        pal_color_alpha(PAL_SUMI_CLEAR, 0.72f));
    draw_rect_solid(
        r,
        (Rectangle){mini_obj.x + 18.0f, mini_obj.y + 46.0f, (mini_obj.width - 36.0f) * 0.5f, 5.0f},
        pal_color_alpha(PAL_SHU, 0.98f));

    render_draw_text_fit(r, text_get(TEXT_GUIDE_D0_TARGET, r->use_ja),
                         (Vector2){pic.x + pic.width * 0.5f, pic.y + 98.0f}, pic.width - 40.0f,
                         14.0f, 11.0f, PAL_BENGARA, 0.96f, false);

    /* Arena middle stage */
    const Rectangle arena = {pic.x + 22.0f, pic.y + 116.0f, pic.width - 44.0f, 206.0f};
    ink_fill_panel(r, arena, PAL_GOFUN, 0.94f, PAL_SUMI_LIGHT, 0.72f, 1302u);

    /* Center: Shrine + glowing vermilion flame HP drops */
    const Vector2 shrine_pos = {arena.x + arena.width * 0.5f, arena.y + 98.0f};
    ink_radial_glow(r, shrine_pos, 64.0f, 52.0f, pal_color_alpha(PAL_SHU_HOT, 0.16f),
                    pal_color_alpha(PAL_SHU_HOT, 0.0f));
    draw_guide_sprite(r, SPR_SHRINE, shrine_pos, 108.0f, 1.0f, 1.0f, 0.0f);
    for (int k = 0; k < 5; ++k) {
        const Vector2 fp = {shrine_pos.x - 36.0f + (float)k * 18.0f, arena.y + 24.0f};
        ink_teardrop(r, fp, (Vector2){0.0f, -1.0f}, 11.0f, 6.5f,
                     pal_color_alpha(k < 4 ? PAL_SHU : PAL_SUMI_CLEAR, 0.94f));
        if (k < 4) {
            ink_teardrop(r, (Vector2){fp.x, fp.y + 1.0f}, (Vector2){0.0f, -1.0f}, 5.5f, 3.2f,
                         pal_color_alpha(PAL_SHU_HOT, 0.96f));
        }
    }
    const Rectangle shr_tag = {shrine_pos.x - 56.0f, arena.y + 150.0f, 112.0f, 24.0f};
    draw_keycap(r, shr_tag, text_get(TEXT_GUIDE_D0_PROTECT, r->use_ja), PAL_SHU, 1303u);

    /* Left: Archer & Right: Umbrella aiming dashed threat arrows inward toward center Shrine */
    const Vector2 archer_pos = {arena.x + 68.0f, arena.y + 96.0f + bob * 0.6f};
    const Vector2 umb_pos = {arena.x + arena.width - 68.0f, arena.y + 102.0f - bob};
    draw_guide_sprite(r, SPR_ARCHER_DRAW, archer_pos, 82.0f, 1.0f, 1.0f, 0.0f);
    draw_guide_sprite(r, SPR_UMBRELLA_HOP, umb_pos, 82.0f, -1.0f, 1.0f, 0.0f);

    const float shift = fmodf(t * 28.0f, 18.0f);
    ink_dashed_stroke(r, (Vector2){archer_pos.x + 42.0f + shift * 0.2f, archer_pos.y + 2.0f},
                      (Vector2){shrine_pos.x - 66.0f, shrine_pos.y - 4.0f}, 12.0f, 9.0f, 3.0f,
                      (InkTone){.color = PAL_SHU, .alpha = 0.85f}, 1304u);
    draw_arrow_head(r, (Vector2){shrine_pos.x - 60.0f, shrine_pos.y - 3.0f}, (Vector2){1.0f, 0.05f},
                    11.0f, 6.0f, pal_color_alpha(PAL_SHU, 0.92f));
    ink_dashed_stroke(r, (Vector2){umb_pos.x - 42.0f - shift * 0.2f, umb_pos.y - 2.0f},
                      (Vector2){shrine_pos.x + 66.0f, shrine_pos.y + 4.0f}, 12.0f, 9.0f, 3.0f,
                      (InkTone){.color = PAL_BENGARA, .alpha = 0.80f}, 1305u);
    draw_arrow_head(r, (Vector2){shrine_pos.x + 60.0f, shrine_pos.y + 3.0f},
                    (Vector2){-1.0f, -0.05f}, 11.0f, 6.0f, pal_color_alpha(PAL_BENGARA, 0.90f));
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D0_THREAT, r->use_ja),
                         (Vector2){arena.x + arena.width * 0.5f, arena.y + 189.0f}, 200.0f, 14.0f,
                         11.0f, PAL_SUMI_HEAVY, 0.95f, false);

    /* Bottom: 3-step progression banner (Wave 1-3 -> Boss -> Blessing/Relic) */
    const float by = pic.y + 340.0f;
    const float bw = 152.0f;
    const float bh = 54.0f;
    const float bx0 = pic.x + 22.0f;
    const float bgap = 30.0f;
    static const TextId k_steps[3] = {TEXT_GUIDE_D0_STEP1, TEXT_GUIDE_D0_STEP2,
                                      TEXT_GUIDE_D0_STEP3};
    static const PaletteId k_borders[3] = {PAL_SUMI_DARK, PAL_BENGARA, PAL_KIN};
    for (int i = 0; i < 3; ++i) {
        const Rectangle sb = {bx0 + (float)i * (bw + bgap), by, bw, bh};
        const bool dark = (i == 2);
        ink_fill_panel(r, sb, dark ? PAL_SUMI_SCORCH : PAL_GOFUN, 0.96f, k_borders[i], 0.94f,
                       1310u + (uint32_t)i);
        render_draw_text_fit(
            r, text_get(k_steps[i], r->use_ja),
            (Vector2){sb.x + sb.width * 0.5f, sb.y + sb.height * 0.5f}, sb.width - 16.0f, 17.0f,
            12.0f, dark ? PAL_KIN_HOT : (i == 1 ? PAL_BENGARA : PAL_SUMI_SCORCH), 0.98f, false);
        if (i < 2) {
            const float ax = sb.x + sb.width + 4.0f;
            const float ay = sb.y + sb.height * 0.5f;
            draw_arrow_head(r, (Vector2){ax + 20.0f, ay}, (Vector2){1.0f, 0.0f}, 11.0f, 6.5f,
                            pal_color_alpha(PAL_SUMI_HEAVY, 0.90f));
            draw_rect_solid(r, (Rectangle){ax, ay - 2.0f, 11.0f, 4.0f},
                            pal_color_alpha(PAL_SUMI_HEAVY, 0.90f));
        }
    }
}

/* Slide 1 (2/5): Controls — 2x2 Visual Control Diagram */
static void draw_slide1_diagram(RenderState *r, Rectangle pic, float t)
{
    const float cw = 250.0f;
    const float ch = 186.0f;
    const float gx = 16.0f;
    const float gy = 16.0f;
    const float x0 = pic.x + 22.0f;
    const float y0 = pic.y + 16.0f;

    /* Cell 0 (Top-Left): Wind Brush stroke */
    const Rectangle c0 = {x0, y0, cw, ch};
    ink_fill_panel(r, c0, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.85f, 1320u);
    const float reveal = 0.25f + 0.75f * fmodf(t * 0.55f, 1.0f);
    Vector2 spts[8];
    for (int k = 0; k < 8; ++k) {
        const float u = (float)k / 7.0f;
        spts[k] = (Vector2){c0.x + 30.0f + u * (cw - 64.0f),
                            c0.y + 72.0f + sinf(u * k_guide_two_pi) * 24.0f};
    }
    ink_stroke(r, spts, 8,
               (BrushProfile){.w_max = 15.0f,
                              .w_min_ratio = 0.24f,
                              .entry = 0.14f,
                              .exit = 0.30f,
                              .dry = 0.30f,
                              .seed = 1321u},
               (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.94f}, reveal);
    const int tip_idx = (int)(reveal * 6.9f);
    ink_teardrop(r, spts[tip_idx], (Vector2){-0.5f, 0.8f}, 14.0f, 9.0f,
                 pal_color_alpha(PAL_SHU, 0.92f));
    draw_keycap(r, (Rectangle){c0.x + 14.0f, c0.y + 120.0f, 106.0f, 26.0f},
                text_get(TEXT_GUIDE_D1_KEY_DRAG, r->use_ja), PAL_SUMI_LIGHT, 1322u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D1_CAP_DRAG, r->use_ja),
                         (Vector2){c0.x + cw * 0.5f, c0.y + 164.0f}, cw - 20.0f, 15.0f, 11.0f,
                         PAL_SUMI_SCORCH, 0.96f, false);

    /* Cell 1 (Top-Right): Cursor Ink Ring + Dry tick + Ink Orbs drifting in */
    const Rectangle c1 = {x0 + cw + gx, y0, cw, ch};
    ink_fill_panel(r, c1, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.85f, 1323u);
    const Vector2 ring_c = {c1.x + cw * 0.5f, c1.y + 66.0f};
    const float rad = 34.0f;
    const float start_a = -k_guide_pi * 0.5f;
    const float ink_ratio = 0.35f + 0.55f * (0.5f + 0.5f * sinf(t * 1.4f));
    ink_arc_stroke(r, ring_c, rad, start_a, start_a + k_guide_two_pi * 0.96f,
                   (BrushProfile){.w_max = 3.2f,
                                  .w_min_ratio = 0.4f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.2f,
                                  .seed = 1324u},
                   (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.55f});
    ink_arc_stroke(r, ring_c, rad, start_a, start_a + k_guide_two_pi * ink_ratio,
                   (BrushProfile){.w_max = 5.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.2f,
                                  .dry = 0.0f,
                                  .seed = 1325u},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.95f});
    const float dry_a = start_a + k_guide_two_pi * 0.20f;
    const Vector2 tick[2] = {
        {ring_c.x + cosf(dry_a) * (rad - 6.0f), ring_c.y + sinf(dry_a) * (rad - 6.0f)},
        {ring_c.x + cosf(dry_a) * (rad + 7.0f), ring_c.y + sinf(dry_a) * (rad + 7.0f)},
    };
    ink_stroke(r, tick, 2,
               (BrushProfile){.w_max = 3.2f,
                              .w_min_ratio = 0.5f,
                              .entry = 0.1f,
                              .exit = 0.1f,
                              .dry = 0.0f,
                              .seed = 1326u},
               (InkTone){.color = PAL_SHU, .alpha = 0.96f}, 1.0f);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D1_DRY, r->use_ja),
                         (Vector2){ring_c.x + 56.0f, ring_c.y - 26.0f}, 68.0f, 13.0f, 10.0f,
                         PAL_SHU, 0.96f, false);
    for (int k = 0; k < 3; ++k) {
        const float ph = fmodf(t * 0.6f + (float)k * 0.33f, 1.0f);
        const float dist = (1.0f - ph) * 62.0f + 10.0f;
        const float ang = 2.4f + (float)k * 0.7f;
        const Vector2 op = {ring_c.x + cosf(ang) * dist, ring_c.y + sinf(ang) * dist};
        ink_radial_glow(r, op, 10.0f, 10.0f, pal_color_alpha(PAL_KIN_HOT, 0.70f),
                        pal_color_alpha(PAL_KIN_HOT, 0.0f));
        ink_teardrop(r, op, (Vector2){0.0f, -1.0f}, 7.0f, 6.0f,
                     pal_color_alpha(PAL_SUMI_SCORCH, 0.94f));
    }
    draw_keycap(r, (Rectangle){c1.x + 14.0f, c1.y + 120.0f, 106.0f, 26.0f},
                text_get(TEXT_GUIDE_D1_KEY_INK, r->use_ja), PAL_KIN, 1327u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D1_CAP_INK, r->use_ja),
                         (Vector2){c1.x + cw * 0.5f, c1.y + 164.0f}, cw - 20.0f, 15.0f, 11.0f,
                         PAL_SUMI_SCORCH, 0.96f, false);

    /* Cell 2 (Bottom-Left): Cyclone Swirl */
    const Rectangle c2 = {x0, y0 + ch + gy, cw, ch};
    ink_fill_panel(r, c2, PAL_GOFUN, 0.96f, PAL_AO, 0.88f, 1328u);
    const Vector2 sw_c = {c2.x + cw * 0.5f, c2.y + 64.0f};
    ink_wash(r, sw_c, 42.0f, 34.0f, 0.0f, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.25f}, 2,
             1329u);
    for (int k = 0; k < 3; ++k) {
        const float a0 = t * 2.6f + (float)k * (k_guide_two_pi / 3.0f);
        ink_arc_stroke(r, sw_c, 16.0f + (float)k * 12.0f, a0, a0 + 2.4f,
                       (BrushProfile){.w_max = 4.4f - (float)k * 0.6f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.22f,
                                      .seed = 1330u + (uint32_t)k},
                       (InkTone){.color = k == 0 ? PAL_AO : PAL_SUMI_DARK, .alpha = 0.92f});
    }
    draw_keycap(r, (Rectangle){c2.x + 14.0f, c2.y + 120.0f, 126.0f, 26.0f},
                text_get(TEXT_GUIDE_D1_KEY_CYC, r->use_ja), PAL_AO, 1334u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D1_CAP_CYC, r->use_ja),
                         (Vector2){c2.x + cw * 0.5f, c2.y + 164.0f}, cw - 20.0f, 15.0f, 11.0f,
                         PAL_SUMI_SCORCH, 0.96f, false);

    /* Cell 3 (Bottom-Right): Gale Burst Crescent Slash */
    const Rectangle c3 = {x0 + cw + gx, y0 + ch + gy, cw, ch};
    ink_fill_panel(r, c3, PAL_GOFUN, 0.96f, PAL_BENGARA, 0.88f, 1335u);
    const float g_shift = fmodf(t * 1.1f, 1.0f) * 36.0f;
    const Vector2 g_c = {c3.x + 96.0f + g_shift, c3.y + 64.0f};
    ink_arc_stroke(r, g_c, 36.0f, -1.15f, 1.15f,
                   (BrushProfile){.w_max = 10.0f,
                                  .w_min_ratio = 0.20f,
                                  .entry = 0.16f,
                                  .exit = 0.32f,
                                  .dry = 0.25f,
                                  .seed = 1336u},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.92f});
    ink_arc_stroke(r, (Vector2){g_c.x + 10.0f, g_c.y}, 30.0f, -0.95f, 0.95f,
                   (BrushProfile){.w_max = 5.0f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.14f,
                                  .exit = 0.28f,
                                  .dry = 0.18f,
                                  .seed = 1337u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.90f});
    for (int k = -1; k <= 1; ++k) {
        const Vector2 wline[2] = {
            {c3.x + 30.0f, c3.y + 64.0f + (float)k * 16.0f},
            {g_c.x - 10.0f, c3.y + 64.0f + (float)k * 16.0f},
        };
        ink_stroke(r, wline, 2,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.3f,
                                  .entry = 0.1f,
                                  .exit = 0.3f,
                                  .dry = 0.4f,
                                  .seed = 1338u + (uint32_t)(k + 1)},
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.72f}, 1.0f);
    }
    draw_keycap(r, (Rectangle){c3.x + 14.0f, c3.y + 120.0f, 88.0f, 26.0f},
                text_get(TEXT_GUIDE_D1_KEY_GALE, r->use_ja), PAL_BENGARA, 1341u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D1_CAP_GALE, r->use_ja),
                         (Vector2){c3.x + cw * 0.5f, c3.y + 164.0f}, cw - 20.0f, 15.0f, 11.0f,
                         PAL_SUMI_SCORCH, 0.96f, false);
}

/* Slide 2 (3/5): How Damage Works — Strip 1 Deflect, Strip 2 Slam & Ink Mark x2 */
static void draw_slide2_diagram(RenderState *r, Rectangle pic, float t)
{
    const float sw = pic.width - 44.0f;
    const float sh = 186.0f;
    const float sx = pic.x + 22.0f;

    /* Top Strip: 1. Deflect */
    const Rectangle top = {sx, pic.y + 16.0f, sw, sh};
    ink_fill_panel(r, top, PAL_GOFUN, 0.96f, PAL_SHU, 0.88f, 1350u);
    draw_keycap(r, (Rectangle){top.x + 12.0f, top.y + 10.0f, 126.0f, 26.0f},
                text_get(TEXT_GUIDE_D2_STRIP1, r->use_ja), PAL_SHU, 1351u);

    const Vector2 arch_p = {top.x + 86.0f, top.y + 114.0f};
    draw_guide_sprite(r, SPR_ARCHER_DRAW, arch_p, 84.0f, 1.0f, 1.0f, 0.0f);

    /* Enemy arrow going right, curved in a bright U-turn by brush stroke back into Archer */
    const Vector2 out_arc[4] = {
        {arch_p.x + 34.0f, arch_p.y - 6.0f},
        {top.x + 240.0f, top.y + 114.0f},
        {top.x + 360.0f, top.y + 108.0f},
        {top.x + 392.0f, top.y + 76.0f},
    };
    ink_dashed_stroke(r, out_arc[0], out_arc[2], 12.0f, 8.0f, 3.0f,
                      (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.72f}, 1352u);

    /* Sweeping brush stroke bending the arrow */
    const Vector2 brush_curve[5] = {
        {top.x + 340.0f, top.y + 150.0f}, {top.x + 412.0f, top.y + 122.0f},
        {top.x + 422.0f, top.y + 66.0f},  {top.x + 362.0f, top.y + 40.0f},
        {top.x + 266.0f, top.y + 52.0f},
    };
    ink_stroke(r, brush_curve, 5,
               (BrushProfile){.w_max = 12.0f,
                              .w_min_ratio = 0.25f,
                              .entry = 0.14f,
                              .exit = 0.30f,
                              .dry = 0.30f,
                              .seed = 1353u},
               (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.45f}, 1.0f);

    /* Bright deflected return U-turn back into Archer */
    const Vector2 ret_arc[5] = {
        {top.x + 360.0f, top.y + 108.0f},     {top.x + 404.0f, top.y + 82.0f},
        {top.x + 356.0f, top.y + 54.0f},      {top.x + 232.0f, top.y + 58.0f},
        {arch_p.x + 38.0f, arch_p.y - 24.0f},
    };
    const float pulse = 0.45f + 0.55f * fmodf(t * 0.7f, 1.0f);
    ink_stroke(r, ret_arc, 5,
               (BrushProfile){.w_max = 6.5f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.12f,
                              .exit = 0.22f,
                              .dry = 0.10f,
                              .seed = 1354u},
               (InkTone){.color = PAL_SHU, .alpha = 0.94f}, pulse);
    draw_arrow_head(r, (Vector2){arch_p.x + 32.0f, arch_p.y - 26.0f}, (Vector2){-0.96f, 0.28f},
                    13.0f, 7.0f, pal_color_alpha(PAL_SHU, 0.96f));
    ink_radial_glow(r, (Vector2){arch_p.x + 24.0f, arch_p.y - 22.0f}, 24.0f, 24.0f,
                    pal_color_alpha(PAL_KIN_HOT, 0.80f), pal_color_alpha(PAL_KIN_HOT, 0.0f));
    ink_fill_panel(r, (Rectangle){top.x + 162.0f, top.y + 14.0f, 122.0f, 28.0f}, PAL_SUMI_SCORCH,
                   0.94f, PAL_KIN, 0.95f, 1355u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D2_DEFLECT, r->use_ja),
                         (Vector2){top.x + 223.0f, top.y + 28.0f}, 108.0f, 16.0f, 12.0f,
                         PAL_KIN_HOT, 0.98f, false);

    /* Bottom Strip: 2. Slam & Ink Mark x2 */
    const Rectangle bot = {sx, pic.y + 216.0f, sw, sh};
    ink_fill_panel(r, bot, PAL_GOFUN, 0.96f, PAL_BENGARA, 0.88f, 1356u);
    draw_keycap(r, (Rectangle){bot.x + 12.0f, bot.y + 10.0f, 188.0f, 26.0f},
                text_get(TEXT_GUIDE_D2_STRIP2, r->use_ja), PAL_BENGARA, 1357u);

    const float slam_ph = fmodf(t * 0.65f, 1.0f);
    const float slide_x = slam_ph < 0.72f ? (slam_ph / 0.72f) : 1.0f;
    const Vector2 umb_p = {bot.x + 132.0f + slide_x * 136.0f, bot.y + 114.0f};
    const Vector2 rock_p = {bot.x + 422.0f, bot.y + 114.0f};

    /* Ink wash soaking area + Ink Mark x2 dark badge */
    ink_wash(r, (Vector2){umb_p.x, umb_p.y + 16.0f}, 54.0f, 22.0f, 0.0f,
             (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.55f}, 3, 1358u);
    draw_guide_sprite(r, SPR_UMBRELLA_HOP, umb_p, 82.0f, 1.0f, 1.0f, slide_x * 0.28f);
    draw_keycap(r, (Rectangle){umb_p.x - 54.0f, bot.y + 44.0f, 108.0f, 24.0f},
                text_get(TEXT_GUIDE_D2_MARK, r->use_ja), PAL_KIN, 1359u);

    /* Gale burst blast pushing Umbrella right */
    ink_arc_stroke(r, (Vector2){bot.x + 64.0f, bot.y + 114.0f}, 32.0f, -1.0f, 1.0f,
                   (BrushProfile){.w_max = 8.0f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.14f,
                                  .exit = 0.28f,
                                  .dry = 0.20f,
                                  .seed = 1360u},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.88f});
    draw_keycap(r, (Rectangle){bot.x + 18.0f, bot.y + 148.0f, 72.0f, 24.0f},
                text_get(TEXT_GUIDE_D1_KEY_GALE, r->use_ja), PAL_SUMI_LIGHT, 1361u);

    /* Rock pillar + crimson impact shockwave rings */
    draw_guide_sprite(r, SPR_TERRAIN_ROCK, rock_p, 94.0f, 1.0f, 1.0f, 0.0f);
    const Vector2 hit_p = {rock_p.x - 36.0f, rock_p.y - 6.0f};
    const float ring_r = 16.0f + fmodf(t * 26.0f, 24.0f);
    ink_arc_stroke(r, hit_p, ring_r, -2.2f, 2.2f,
                   (BrushProfile){.w_max = 4.6f,
                                  .w_min_ratio = 0.30f,
                                  .entry = 0.12f,
                                  .exit = 0.25f,
                                  .dry = 0.15f,
                                  .seed = 1362u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.92f});
    ink_fill_panel(r, (Rectangle){bot.x + 332.0f, bot.y + 12.0f, 164.0f, 28.0f}, PAL_SUMI_SCORCH,
                   0.94f, PAL_SHU, 0.95f, 1363u);
    render_draw_text_fit(r, text_get(TEXT_CALLOUT_SLAM, r->use_ja),
                         (Vector2){bot.x + 414.0f, bot.y + 26.0f}, 148.0f, 16.0f, 12.0f, PAL_GOFUN,
                         0.98f, false);
}

/* Slide 3 (4/5): Elemental Reactions — 2x2 Illustrated Reaction Encyclopedia */
static void draw_slide3_diagram(RenderState *r, Rectangle pic, float t)
{
    const float cw = 250.0f;
    const float ch = 186.0f;
    const float gx = 16.0f;
    const float gy = 16.0f;
    const float x0 = pic.x + 22.0f;
    const float y0 = pic.y + 16.0f;
    const float pulse = 0.5f + 0.5f * sinf(t * 4.5f);

    /* Card 0: Ink + Fire -> Blazing Inkstorm */
    const Rectangle c0 = {x0, y0, cw, ch};
    ink_fill_panel(r, c0, PAL_GOFUN, 0.96f, PAL_SHU, 0.92f, 1370u);
    ink_wash(r, (Vector2){c0.x + 76.0f, c0.y + 92.0f}, 44.0f, 26.0f, -0.1f,
             (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.65f}, 3, 1371u);
    ink_radial_glow(r, (Vector2){c0.x + 158.0f, c0.y + 84.0f}, 44.0f, 36.0f,
                    pal_color_alpha(PAL_SHU_HOT, 0.50f + 0.20f * pulse),
                    pal_color_alpha(PAL_SHU, 0.0f));
    draw_guide_sprite(r, SPR_LANTERN_FIRE, (Vector2){c0.x + 168.0f, c0.y + 82.0f}, 76.0f, 1.0f,
                      1.0f, 0.0f);
    for (int k = 0; k < 4; ++k) {
        const float fx = c0.x + 62.0f + (float)k * 24.0f;
        const float fy = c0.y + 84.0f - fabsf(sinf(t * 4.0f + (float)k)) * 12.0f;
        ink_teardrop(r, (Vector2){fx, fy}, (Vector2){0.1f, -1.0f}, 11.0f, 6.0f,
                     pal_color_alpha(PAL_SHU, 0.92f));
        ink_teardrop(r, (Vector2){fx, fy + 2.0f}, (Vector2){0.1f, -1.0f}, 5.5f, 3.0f,
                     pal_color_alpha(PAL_SHU_HOT, 0.96f));
    }
    draw_keycap(r, (Rectangle){c0.x + 12.0f, c0.y + 10.0f, 96.0f, 24.0f},
                text_get(TEXT_GUIDE_D3_EQ_BLAZE, r->use_ja), PAL_SHU, 1372u);
    render_draw_text_fit(r, text_get(TEXT_RESULT_BLAZING_LABEL, r->use_ja),
                         (Vector2){c0.x + cw * 0.5f, c0.y + 140.0f}, cw - 20.0f, 19.0f, 14.0f,
                         PAL_BENGARA, 0.98f, false);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D3_SUB_BLAZE, r->use_ja),
                         (Vector2){c0.x + cw * 0.5f, c0.y + 166.0f}, cw - 20.0f, 14.0f, 11.0f,
                         PAL_SUMI_HEAVY, 0.95f, false);

    /* Card 1: Ink + Storm -> Tempest Lightning */
    const Rectangle c1 = {x0 + cw + gx, y0, cw, ch};
    ink_fill_panel(r, c1, PAL_GOFUN, 0.96f, PAL_AO, 0.92f, 1373u);
    ink_wash(r, (Vector2){c1.x + 110.0f, c1.y + 92.0f}, 56.0f, 24.0f, 0.05f,
             (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.62f}, 3, 1374u);
    ink_radial_glow(r, (Vector2){c1.x + 178.0f, c1.y + 82.0f}, 42.0f, 34.0f,
                    pal_color_alpha(PAL_AO_HOT, 0.52f + 0.18f * pulse),
                    pal_color_alpha(PAL_AO, 0.0f));
    draw_guide_sprite(r, SPR_LANTERN_STORM, (Vector2){c1.x + 178.0f, c1.y + 82.0f}, 76.0f, 1.0f,
                      1.0f, 0.0f);
    const Vector2 bolt[5] = {
        {c1.x + 150.0f, c1.y + 78.0f}, {c1.x + 122.0f, c1.y + 62.0f + sinf(t * 9.0f) * 3.0f},
        {c1.x + 98.0f, c1.y + 90.0f},  {c1.x + 70.0f, c1.y + 70.0f - sinf(t * 9.0f) * 3.0f},
        {c1.x + 42.0f, c1.y + 92.0f},
    };
    ink_stroke(r, bolt, 5,
               (BrushProfile){.w_max = 4.6f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.08f,
                              .exit = 0.12f,
                              .dry = 0.0f,
                              .seed = 1375u},
               (InkTone){.color = PAL_AO, .alpha = 0.96f}, 1.0f);
    ink_stroke(r, bolt, 5,
               (BrushProfile){.w_max = 2.0f,
                              .w_min_ratio = 0.45f,
                              .entry = 0.08f,
                              .exit = 0.12f,
                              .dry = 0.0f,
                              .seed = 1376u},
               (InkTone){.color = PAL_AO_HOT, .alpha = 0.98f}, 1.0f);
    draw_keycap(r, (Rectangle){c1.x + 12.0f, c1.y + 10.0f, 96.0f, 24.0f},
                text_get(TEXT_GUIDE_D3_EQ_STORM, r->use_ja), PAL_AO, 1377u);
    render_draw_text_fit(r, text_get(TEXT_RESULT_TEMPEST_LABEL, r->use_ja),
                         (Vector2){c1.x + cw * 0.5f, c1.y + 140.0f}, cw - 20.0f, 19.0f, 14.0f,
                         PAL_AO, 0.98f, false);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D3_SUB_STORM, r->use_ja),
                         (Vector2){c1.x + cw * 0.5f, c1.y + 166.0f}, cw - 20.0f, 14.0f, 11.0f,
                         PAL_SUMI_HEAVY, 0.95f, false);

    /* Card 2: Fire + Storm -> Thunderburst */
    const Rectangle c2 = {x0, y0 + ch + gy, cw, ch};
    ink_fill_panel(r, c2, PAL_GOFUN, 0.96f, PAL_BENGARA, 0.92f, 1378u);
    const Vector2 bc = {c2.x + cw * 0.5f, c2.y + 82.0f};
    ink_radial_glow(r, bc, 48.0f, 38.0f, pal_color_alpha(PAL_KIN_HOT, 0.58f),
                    pal_color_alpha(PAL_SHU, 0.0f));
    ink_arc_stroke(r, (Vector2){bc.x - 20.0f, bc.y}, 26.0f + pulse * 4.0f, 0.0f, k_guide_two_pi,
                   (BrushProfile){.w_max = 4.6f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.12f,
                                  .seed = 1379u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.94f});
    ink_arc_stroke(r, (Vector2){bc.x + 20.0f, bc.y}, 26.0f + pulse * 4.0f, 0.0f, k_guide_two_pi,
                   (BrushProfile){.w_max = 4.6f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.12f,
                                  .seed = 1380u},
                   (InkTone){.color = PAL_AO, .alpha = 0.94f});
    for (int k = 0; k < 6; ++k) {
        const float a = (float)k * (k_guide_two_pi / 6.0f) + t * 0.8f;
        const Vector2 sp = {bc.x + cosf(a) * 32.0f, bc.y + sinf(a) * 24.0f};
        ink_teardrop(r, sp, (Vector2){cosf(a), sinf(a)}, 10.0f, 5.0f,
                     pal_color_alpha((k % 2 == 0) ? PAL_SHU_HOT : PAL_AO_HOT, 0.94f));
    }
    draw_keycap(r, (Rectangle){c2.x + 12.0f, c2.y + 10.0f, 96.0f, 24.0f},
                text_get(TEXT_GUIDE_D3_EQ_BURST, r->use_ja), PAL_SHU, 1381u);
    render_draw_text_fit(r, text_get(TEXT_RESULT_THUNDERBURST_LABEL, r->use_ja),
                         (Vector2){c2.x + cw * 0.5f, c2.y + 140.0f}, cw - 20.0f, 19.0f, 14.0f,
                         PAL_SHU, 0.98f, false);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D3_SUB_BURST, r->use_ja),
                         (Vector2){c2.x + cw * 0.5f, c2.y + 166.0f}, cw - 20.0f, 14.0f, 11.0f,
                         PAL_SUMI_HEAVY, 0.95f, false);

    /* Card 3: Ink + Fire + Storm -> Cataclysm (Dark Lacquer Hero Card) */
    const Rectangle c3 = {x0 + cw + gx, y0 + ch + gy, cw, ch};
    ink_fill_panel(r, c3, PAL_SUMI_SCORCH, 0.97f, PAL_KIN, 0.96f, 1382u);
    const Vector2 cc = {c3.x + cw * 0.5f, c3.y + 82.0f};
    ink_radial_glow(r, cc, 62.0f, 46.0f, pal_color_alpha(PAL_KIN_HOT, 0.42f + 0.16f * pulse),
                    pal_color_alpha(PAL_KIN, 0.0f));
    ink_arc_stroke(r, cc, 34.0f + pulse * 4.0f, 0.15f, k_guide_two_pi - 0.15f,
                   (BrushProfile){.w_max = 6.2f,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.14f,
                                  .exit = 0.25f,
                                  .dry = 0.20f,
                                  .seed = 1383u},
                   (InkTone){.color = PAL_KIN_HOT, .alpha = 0.96f});
    seal_stamp(r, TEXT_SEAL_SANGEN, cc, 40.0f, SEAL_SHUBUN, 0.85f);
    draw_keycap(r, (Rectangle){c3.x + 12.0f, c3.y + 10.0f, 128.0f, 24.0f},
                text_get(TEXT_GUIDE_D3_EQ_CATA, r->use_ja), PAL_KIN, 1384u);
    render_draw_text_fit(r, text_get(TEXT_RESULT_CATACLYSM_LABEL, r->use_ja),
                         (Vector2){c3.x + cw * 0.5f, c3.y + 140.0f}, cw - 20.0f, 19.0f, 14.0f,
                         PAL_KIN_HOT, 0.98f, false);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D3_SUB_CATA, r->use_ja),
                         (Vector2){c3.x + cw * 0.5f, c3.y + 166.0f}, cw - 20.0f, 14.0f, 11.0f,
                         PAL_GOFUN, 0.95f, false);
}

/* Slide 4 (5/5): Chains, Blessings & Bokuan Mastery */
static void draw_slide4_diagram(RenderState *r, Rectangle pic, float t)
{
    const float inner_w = pic.width - 44.0f;
    const float x0 = pic.x + 22.0f;
    const float pulse = 0.5f + 0.5f * sinf(t * 3.8f);

    /* Top block: Dark lacquer Chain x2+ -> Golden Ink Surge */
    const Rectangle top = {x0, pic.y + 16.0f, inner_w, 134.0f};
    ink_fill_panel(r, top, PAL_SUMI_SCORCH, 0.96f, PAL_KIN, 0.96f, 1390u);

    char chain_txt[48];
    snprintf(chain_txt, sizeof chain_txt, "%s%s", text_get(TEXT_CALLOUT_CHAIN, r->use_ja),
             r->use_ja ? "2\xE3\x80\x9C" : "2+");
    const Rectangle ch_box = {top.x + 22.0f, top.y + 22.0f, 186.0f, 48.0f};
    ink_fill_panel(r, ch_box, PAL_SUMI_DARK, 0.95f, PAL_SHU, 0.94f, 1391u);
    render_draw_text_fit(r, chain_txt,
                         (Vector2){ch_box.x + ch_box.width * 0.5f, ch_box.y + ch_box.height * 0.5f},
                         ch_box.width - 18.0f, 22.0f, 15.0f, PAL_GOFUN, 0.98f, false);

    draw_rect_solid(r, (Rectangle){ch_box.x + ch_box.width + 12.0f, top.y + 44.0f, 28.0f, 4.0f},
                    pal_color_alpha(PAL_KIN_HOT, 0.95f));
    draw_arrow_head(r, (Vector2){ch_box.x + ch_box.width + 48.0f, top.y + 46.0f},
                    (Vector2){1.0f, 0.0f}, 12.0f, 7.0f, pal_color_alpha(PAL_KIN_HOT, 0.98f));

    const Rectangle surge_box = {ch_box.x + ch_box.width + 60.0f, top.y + 22.0f, 224.0f, 48.0f};
    ink_radial_glow(
        r, (Vector2){surge_box.x + surge_box.width * 0.5f, surge_box.y + surge_box.height * 0.5f},
        118.0f, 32.0f, pal_color_alpha(PAL_KIN_HOT, 0.24f + 0.14f * pulse),
        pal_color_alpha(PAL_KIN, 0.0f));
    ink_fill_panel(r, surge_box, PAL_SUMI_DARK, 0.96f, PAL_KIN_HOT, 0.96f, 1392u);
    render_draw_text_fit(
        r, text_get(TEXT_CALLOUT_SURGE, r->use_ja),
        (Vector2){surge_box.x + surge_box.width * 0.5f, surge_box.y + surge_box.height * 0.5f},
        surge_box.width - 18.0f, 22.0f, 15.0f, PAL_KIN_HOT, 0.99f, false);

    /* Golden glowing surge gauge bar + caption */
    const float fill_ratio = 0.55f + 0.40f * pulse;
    draw_rect_solid(r, (Rectangle){top.x + 24.0f, top.y + 84.0f, inner_w - 48.0f, 10.0f},
                    pal_color_alpha(PAL_SUMI_HEAVY, 0.85f));
    draw_rect_solid(r,
                    (Rectangle){top.x + 26.0f, top.y + 86.0f, (inner_w - 52.0f) * fill_ratio, 6.0f},
                    pal_color_alpha(PAL_KIN_HOT, 0.98f));
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D4_SURGE_SUB, r->use_ja),
                         (Vector2){top.x + inner_w * 0.5f, top.y + 114.0f}, inner_w - 32.0f, 15.0f,
                         11.0f, PAL_GOFUN, 0.95f, false);

    /* Bottom-Left: 3 Mini Axis Blessing Cards */
    const Rectangle bl = {x0, pic.y + 166.0f, 274.0f, 236.0f};
    ink_fill_panel(r, bl, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.86f, 1393u);
    render_draw_text_fit(r, text_get(TEXT_GUIDE_D4_BLESS_HDR, r->use_ja),
                         (Vector2){bl.x + bl.width * 0.5f, bl.y + 22.0f}, bl.width - 20.0f, 16.0f,
                         12.0f, PAL_SUMI_SCORCH, 0.96f, false);

    static const TextId k_axes[3] = {TEXT_AXIS_BLAZE, TEXT_AXIS_THUNDER, TEXT_GUIDE_D4_BLESS_AXIS3};
    static const PaletteId k_ax_pal[3] = {PAL_SHU, PAL_AO, PAL_SUMI_DARK};
    static const TextId k_sample[3] = {TEXT_UPG_EMBER_TIP_NAME, TEXT_UPG_CHAIN_LIGHTNING_NAME,
                                       TEXT_UPG_DOUBLE_GALE_NAME};
    for (int i = 0; i < 3; ++i) {
        const Rectangle mc = {bl.x + 14.0f + (float)i * 84.0f, bl.y + 44.0f, 78.0f, 174.0f};
        ink_fill_panel(r, mc, PAL_PAPER_ACT1, 0.96f, k_ax_pal[i], 0.92f, 1394u + (uint32_t)i);
        draw_rect_solid(r, (Rectangle){mc.x + 2.0f, mc.y + 2.0f, mc.width - 4.0f, 26.0f},
                        pal_color_alpha(k_ax_pal[i], 0.92f));
        render_draw_text_fit(r, text_get(k_axes[i], r->use_ja),
                             (Vector2){mc.x + mc.width * 0.5f, mc.y + 15.0f}, mc.width - 8.0f,
                             13.0f, 9.0f, PAL_GOFUN, 0.98f, false);
        ink_teardrop(r, (Vector2){mc.x + mc.width * 0.5f, mc.y + 66.0f}, (Vector2){0.0f, -1.0f},
                     14.0f, 10.0f, pal_color_alpha(k_ax_pal[i], 0.92f));
        render_draw_text_wrapped(r, text_get(k_sample[i], r->use_ja),
                                 (Rectangle){mc.x + 6.0f, mc.y + 94.0f, mc.width - 12.0f, 44.0f},
                                 13.0f, 10.0f, PAL_SUMI_SCORCH, 0.96f, false, true);
        render_draw_text_fit(r, "Lv 1->3", (Vector2){mc.x + mc.width * 0.5f, mc.y + 154.0f},
                             mc.width - 10.0f, 12.0f, 10.0f, PAL_BENGARA, 0.95f, false);
    }

    /* Bottom-Right: Ink Crystal + Bokuan Permanent Upgrade block */
    const Rectangle br = {x0 + 290.0f, pic.y + 166.0f, inner_w - 290.0f, 236.0f};
    ink_fill_panel(r, br, PAL_GOFUN, 0.96f, PAL_KIN, 0.94f, 1398u);
    const Vector2 gem_c = {br.x + br.width * 0.5f, br.y + 82.0f};
    ink_radial_glow(r, gem_c, 52.0f, 44.0f, pal_color_alpha(PAL_KIN_HOT, 0.50f + 0.18f * pulse),
                    pal_color_alpha(PAL_KIN, 0.0f));
    const Vector2 crystal[6] = {
        {gem_c.x, gem_c.y - 34.0f},         {gem_c.x + 24.0f, gem_c.y - 10.0f},
        {gem_c.x + 18.0f, gem_c.y + 24.0f}, {gem_c.x, gem_c.y + 34.0f},
        {gem_c.x - 18.0f, gem_c.y + 24.0f}, {gem_c.x - 24.0f, gem_c.y - 10.0f},
    };
    ink_fill_poly(r, crystal, 6, pal_color_alpha(PAL_SUMI_SCORCH, 0.95f));
    ink_outline_tone(r, crystal, 6, true, 3.6f, (Vector2){0.7f, 0.7f},
                     (InkTone){.color = PAL_KIN_HOT, .alpha = 0.98f}, 1399u);
    ink_teardrop(r, gem_c, (Vector2){0.0f, -1.0f}, 16.0f, 10.0f,
                 pal_color_alpha(PAL_KIN_HOT, 0.94f));

    const Rectangle bk_badge = {br.x + 42.0f, br.y + 132.0f, br.width - 84.0f, 34.0f};
    ink_fill_panel(r, bk_badge, PAL_SUMI_SCORCH, 0.96f, PAL_KIN, 0.95f, 1400u);
    render_draw_text_fit(
        r, text_get(TEXT_BOKUAN_TITLE, r->use_ja),
        (Vector2){bk_badge.x + bk_badge.width * 0.5f, bk_badge.y + bk_badge.height * 0.5f},
        bk_badge.width - 16.0f, 18.0f, 13.0f, PAL_KIN_HOT, 0.98f, false);

    render_draw_text_wrapped(r, text_get(TEXT_GUIDE_D4_BOKUAN_SUB, r->use_ja),
                             (Rectangle){br.x + 14.0f, br.y + 174.0f, br.width - 28.0f, 48.0f},
                             15.0f, 11.0f, PAL_SUMI_DARK, 0.96f, false, true);
}

static void draw_nav_button(RenderState *r, Rectangle rc, const char *label, bool focused,
                            bool disabled, uint32_t seed)
{
    const PaletteId fill = disabled ? PAL_PAPER_ACT1 : (focused ? PAL_PAPER_ACT1 : PAL_GOFUN);
    const PaletteId border = disabled ? PAL_SUMI_CLEAR : (focused ? PAL_SHU : PAL_SUMI_DARK);
    const PaletteId text_pal = disabled ? PAL_SUMI_LIGHT : (focused ? PAL_SHU : PAL_SUMI_SCORCH);
    ink_fill_panel(r, rc, fill, disabled ? 0.78f : 0.96f, border, disabled ? 0.70f : 0.94f, seed);
    render_draw_text_fit(r, label, (Vector2){rc.x + rc.width * 0.5f, rc.y + rc.height * 0.5f},
                         rc.width - 24.0f, 19.0f, 13.0f, text_pal, disabled ? 0.72f : 0.98f, false);
}

void render_draw_guide_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }
    int32_t page = ui->guide_page;
    if (page < 0) {
        page = 0;
    } else if (page >= GUIDE_SLIDE_COUNT) {
        page = GUIDE_SLIDE_COUNT - 1;
    }
    const GuideSlideText *slide = &k_slides[page];

    /* Soft paper wash over lobby canvas */
    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(PAL_PAPER_ACT1, 0.72f));

    GuideLayout layout;
    render_guide_layout(&layout);
    const Rectangle scroll = layout.panel_rect;
    ink_fill_panel(r, scroll, PAL_GOFUN, 0.97f, PAL_SUMI_DARK, 0.94f, 1280u);

    /* Scroll rods matching Result screen */
    const Rectangle left_rod = {scroll.x - 14.0f, scroll.y - 10.0f, 16.0f, scroll.height + 20.0f};
    const Rectangle right_rod = {scroll.x + scroll.width - 2.0f, scroll.y - 10.0f, 16.0f,
                                 scroll.height + 20.0f};
    ink_fill_panel(r, left_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 1281u);
    ink_fill_panel(r, right_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 1282u);

    /* Top Header: dark lacquer page indicator + 32px brush title + Close button */
    const Rectangle pg_pill = {116.0f, 70.0f, 84.0f, 34.0f};
    ink_fill_panel(r, pg_pill, PAL_SUMI_SCORCH, 0.96f, PAL_KIN, 0.95f, 1283u);
    char pg_buf[32];
    snprintf(pg_buf, sizeof pg_buf, "%d / %d", page + 1, GUIDE_SLIDE_COUNT);
    render_draw_text_centered(
        r, pg_buf, (Vector2){pg_pill.x + pg_pill.width * 0.5f, pg_pill.y + pg_pill.height * 0.5f},
        18.0f, PAL_KIN_HOT, 0.98f, false);

    render_draw_text_fit(r, text_get(slide->title, r->use_ja), (Vector2){618.0f, 87.0f}, 804.0f,
                         32.0f, 20.0f, PAL_SUMI_SCORCH, 0.98f, true);

    const bool close_hov = (ui->hover_choice == GUIDE_HOVER_CLOSE);
    draw_nav_button(r, layout.close_rect, text_get(TEXT_GUIDE_CLOSE, r->use_ja), close_hov, false,
                    1284u);

    const Vector2 hrule[2] = {{116.0f, 122.0f}, {1164.0f, 122.0f}};
    ink_stroke(r, hrule, 2,
               (BrushProfile){.w_max = 2.6f,
                              .w_min_ratio = 0.28f,
                              .entry = 0.10f,
                              .exit = 0.20f,
                              .dry = 0.35f,
                              .seed = 1285u},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.78f}, 1.0f);

    /* Left Illustrated Picture Canvas (560x420) */
    const Rectangle pic = {116.0f, 136.0f, 560.0f, 420.0f};
    ink_fill_panel(r, pic, PAL_PAPER_ACT1, 0.96f, PAL_SUMI_DARK, 0.88f, 1286u);
    if (page == 0) {
        draw_slide0_diagram(r, pic, r->vfx_time);
    } else if (page == 1) {
        draw_slide1_diagram(r, pic, r->vfx_time);
    } else if (page == 2) {
        draw_slide2_diagram(r, pic, r->vfx_time);
    } else if (page == 3) {
        draw_slide3_diagram(r, pic, r->vfx_time);
    } else {
        draw_slide4_diagram(r, pic, r->vfx_time);
    }

    /* Right Explanation Column (468x420): 3 framed bullet cards + bottom TIP strip */
    const float rx = 696.0f;
    const float rw = 468.0f;
    const float card_h = 104.0f;
    const float card_gap = 12.0f;
    for (int i = 0; i < 3; ++i) {
        const Rectangle card = {rx, 136.0f + (float)i * (card_h + card_gap), rw, card_h};
        ink_fill_panel(r, card, PAL_GOFUN, 0.96f, PAL_SUMI_LIGHT, 0.82f, 1290u + (uint32_t)i);

        const Rectangle num_badge = {card.x + 14.0f, card.y + 14.0f, 28.0f, 28.0f};
        ink_fill_panel(r, num_badge, PAL_SUMI_SCORCH, 0.96f, PAL_KIN, 0.94f, 1294u + (uint32_t)i);
        char num_s[16];
        snprintf(num_s, sizeof num_s, "%d", i + 1);
        render_draw_text_centered(
            r, num_s,
            (Vector2){num_badge.x + num_badge.width * 0.5f, num_badge.y + num_badge.height * 0.5f},
            16.0f, PAL_KIN_HOT, 0.98f, false);

        render_draw_text_wrapped(r, text_get(slide->head[i], r->use_ja),
                                 (Rectangle){card.x + 52.0f, card.y + 12.0f, rw - 66.0f, 32.0f},
                                 18.0f, 13.0f, slide->accent[i], 0.98f, false, false);
        render_draw_text_wrapped(r, text_get(slide->body[i], r->use_ja),
                                 (Rectangle){card.x + 16.0f, card.y + 46.0f, rw - 32.0f, 50.0f},
                                 15.0f, 12.0f, PAL_SUMI_DARK, 0.95f, false, false);
    }

    /* Bottom TIP accent strip */
    const Rectangle tip_box = {rx, 486.0f, rw, 70.0f};
    ink_fill_panel(r, tip_box, PAL_PAPER_ACT2, 0.96f, PAL_BENGARA, 0.92f, 1298u);
    const Rectangle tip_tag = {tip_box.x + 12.0f, tip_box.y + 19.0f, 54.0f, 32.0f};
    ink_fill_panel(r, tip_tag, PAL_BENGARA, 0.96f, PAL_SUMI_SCORCH, 0.85f, 1299u);
    render_draw_text_fit(
        r, text_get(TEXT_GUIDE_TIP_TAG, r->use_ja),
        (Vector2){tip_tag.x + tip_tag.width * 0.5f, tip_tag.y + tip_tag.height * 0.5f},
        tip_tag.width - 8.0f, 15.0f, 11.0f, PAL_GOFUN, 0.98f, false);
    render_draw_text_wrapped(r, text_get(slide->tip, r->use_ja),
                             (Rectangle){tip_box.x + 76.0f, tip_box.y + 8.0f, rw - 90.0f, 54.0f},
                             15.0f, 11.0f, PAL_SUMI_SCORCH, 0.97f, false, false);

    /* Bottom Navigation Bar */
    const bool prev_disabled = (page <= 0);
    const bool prev_hov = (!prev_disabled && ui->hover_choice == GUIDE_HOVER_PREV);
    draw_nav_button(r, layout.prev_rect, text_get(TEXT_GUIDE_PREV, r->use_ja), prev_hov,
                    prev_disabled, 1410u);

    const bool is_last = (page >= GUIDE_SLIDE_COUNT - 1);
    const bool next_hov = (ui->hover_choice == GUIDE_HOVER_NEXT);
    const char *next_lbl = text_get(is_last ? TEXT_GUIDE_FINISH : TEXT_GUIDE_NEXT, r->use_ja);
    draw_nav_button(r, layout.next_rect, next_lbl, next_hov, false, 1411u);

    for (int i = 0; i < GUIDE_SLIDE_COUNT; ++i) {
        const bool active = (i == page);
        const bool hov = (ui->hover_choice == i);
        const Rectangle dot = layout.dots[i];
        ink_fill_panel(r, dot, active ? PAL_SUMI_SCORCH : PAL_GOFUN, 0.96f,
                       active ? PAL_KIN : (hov ? PAL_SHU : PAL_SUMI_DARK), 0.94f,
                       1412u + (uint32_t)i);
        char d_s[16];
        snprintf(d_s, sizeof d_s, "%d", i + 1);
        render_draw_text_centered(
            r, d_s, (Vector2){dot.x + dot.width * 0.5f, dot.y + dot.height * 0.5f}, 18.0f,
            active ? PAL_KIN_HOT : (hov ? PAL_SHU : PAL_SUMI_SCORCH), 0.98f, false);
    }

    render_draw_text_fit(r, text_get(TEXT_GUIDE_NAV_HINT, r->use_ja),
                         (Vector2){(float)INK_SCREEN_W * 0.5f, 646.0f}, 960.0f, 14.0f, 11.0f,
                         PAL_SUMI_HEAVY, 0.92f, false);
}
