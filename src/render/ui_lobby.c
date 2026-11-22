#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void render_lobby_layout(LobbyLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    const float x = 112.0f;
    const float w = 296.0f;
    const float h = 58.0f;
    const float gap = 16.0f;
    const float y0 = 236.0f;
    for (int i = 0; i < 4; ++i) {
        out->items[i] = (Rectangle){x, y0 + (float)i * (h + gap), w, h};
    }
}

void render_brush_select_layout(BrushSelectLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    const float card_w = 344.0f;
    const float card_h = 420.0f;
    const float gap = 32.0f;
    const float total_w = 3.0f * card_w + 2.0f * gap;
    const float start_x = ((float)INK_SCREEN_W - total_w) * 0.5f;
    const float card_y = 148.0f;
    for (int i = 0; i < BRUSH_STYLE_COUNT; ++i) {
        out->cards[i] = (Rectangle){start_x + (float)i * (card_w + gap), card_y, card_w, card_h};
    }
    out->back_rect = (Rectangle){((float)INK_SCREEN_W - 240.0f) * 0.5f, 606.0f, 240.0f, 52.0f};
}

void render_draw_lobby_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    /* Calligraphic title "墨嵐" & vermilion seal on right-center */
    const char *title_str = text_get(TEXT_GAME_TITLE, r->use_ja);
    const float title_px = r->use_ja ? 88.0f : 72.0f;
    const Vector2 title_m = render_measure_text(r, title_str, title_px, true);
    const Vector2 title_center = {800.0f, 258.0f};
    const float backing_w = fmaxf(500.0f, title_m.x + 180.0f);
    const Rectangle title_backing = {title_center.x - backing_w * 0.5f + 24.0f, 168.0f, backing_w,
                                     198.0f};
    ink_fill_panel(r, title_backing, PAL_GOFUN, 0.62f, PAL_SUMI_CLEAR, 0.0f, 900u);
    render_draw_text_centered(r, title_str, title_center, title_px, PAL_SUMI_SCORCH, 0.98f, true);
    const float seal_x =
        fminf(title_center.x + title_m.x * 0.5f + 46.0f, (float)INK_SCREEN_W - 60.0f);
    seal_stamp(r, TEXT_SEAL_INKSTORM, (Vector2){seal_x, 286.0f}, 58.0f, SEAL_HAKUBUN, 0.85f);

    const Vector2 rule_pts[2] = {{610.0f, 326.0f}, {970.0f, 326.0f}};
    ink_stroke(r, rule_pts, 2,
               (BrushProfile){.w_max = 3.6f,
                              .w_min_ratio = 0.25f,
                              .entry = 0.14f,
                              .exit = 0.35f,
                              .dry = 0.40f,
                              .seed = 902u},
               (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.82f}, 1.0f);

    render_draw_text_centered(r, text_get(TEXT_LOBBY_SUBTITLE, r->use_ja),
                              (Vector2){810.0f, 348.0f}, 20.0f, PAL_SUMI_HEAVY, 0.94f, false);

    /* Left-side menu buttons */
    LobbyLayout layout;
    render_lobby_layout(&layout);
    static const TextId k_menu_ids[4] = {
        TEXT_LOBBY_SORTIE,
        TEXT_LOBBY_BOKUAN,
        TEXT_LOBBY_SETTINGS,
        TEXT_LOBBY_QUIT,
    };
    const int32_t active =
        ui->hover_choice >= 0 && ui->hover_choice < 4 ? ui->hover_choice : ui->lobby_cursor;

    for (int i = 0; i < 4; ++i) {
        const bool sel = (active == i);
        Rectangle rect = layout.items[i];
        if (sel) {
            rect.x += 8.0f;
        }
        ink_fill_panel(r, rect, PAL_GOFUN, sel ? 0.96f : 0.86f, sel ? PAL_SHU : PAL_SUMI_DARK,
                       0.92f, 910u + (uint32_t)i);
        render_draw_text_centered(
            r, text_get(k_menu_ids[i], r->use_ja),
            (Vector2){rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f}, 24.0f,
            sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
        if (sel) {
            ink_teardrop(r, (Vector2){rect.x + 26.0f, rect.y + rect.height * 0.5f},
                         (Vector2){1.0f, 0.0f}, 9.0f, 6.0f, pal_color_alpha(PAL_SHU, 0.92f));
        }
    }

    /* Bottom meta record strip */
    const Rectangle footer = {64.0f, 608.0f, (float)INK_SCREEN_W - 128.0f, 58.0f};
    ink_fill_panel(r, footer, PAL_GOFUN, 0.88f, PAL_SUMI_LIGHT, 0.72f, 920u);

    const MetaState *m = &ui->meta_view;
    char buf[64];
    const float fy = footer.y + 18.0f;

    render_draw_text_id(r, TEXT_LOBBY_CRYSTALS, (Vector2){96.0f, fy}, 20.0f, PAL_SUMI_HEAVY, 0.95f,
                        false);
    snprintf(buf, sizeof buf, "%d", m->ink_crystals);
    render_draw_number_fixed(r, buf, (Vector2){204.0f, fy}, 20.0f, PAL_KIN, 0.98f);

    render_draw_text_id(r, TEXT_LOBBY_BEST_ACT, (Vector2){356.0f, fy}, 20.0f, PAL_SUMI_HEAVY, 0.95f,
                        false);
    snprintf(buf, sizeof buf, "%d", m->best_act);
    render_draw_number_fixed(r, buf, (Vector2){470.0f, fy}, 20.0f, PAL_SUMI_SCORCH, 0.96f);

    render_draw_text_id(r, TEXT_LOBBY_BEST_SCORE, (Vector2){566.0f, fy}, 20.0f, PAL_SUMI_HEAVY,
                        0.95f, false);
    snprintf(buf, sizeof buf, "%07d", m->best_score);
    render_draw_number_fixed(r, buf, (Vector2){696.0f, fy}, 20.0f, PAL_SUMI_SCORCH, 0.96f);

    render_draw_text_id(r, TEXT_LOBBY_CLEARS, (Vector2){868.0f, fy}, 20.0f, PAL_SUMI_HEAVY, 0.95f,
                        false);
    snprintf(buf, sizeof buf, "%d", m->clears);
    render_draw_number_fixed(r, buf, (Vector2){1002.0f, fy}, 20.0f, PAL_SHU, 0.96f);

    /* Subtle interactive canvas hint */
    render_draw_text_centered(r, text_get(TEXT_LOBBY_CANVAS_HINT, r->use_ja),
                              (Vector2){820.0f, 586.0f}, 18.0f, PAL_SUMI_HEAVY, 0.78f, false);
}

static void draw_brush_trial_demo(RenderState *r, Rectangle box, BrushStyleId style, float t)
{
    ink_fill_panel(r, box, PAL_PAPER_ACT1, 0.92f, PAL_SUMI_CLEAR, 0.65f, 940u + (uint32_t)style);

    const float cx = box.x + box.width * 0.5f;
    const float cy = box.y + box.height * 0.5f;
    const float phase = fmodf(t * 0.55f + (float)style * 0.33f, 1.0f);
    const float reveal = ink_clampf(phase * 1.45f, 0.12f, 1.0f);

    Vector2 pts[10];
    for (int k = 0; k < 10; ++k) {
        const float u = (float)k / 9.0f;
        const float x = box.x + 28.0f + u * (box.width - 56.0f);
        const float amp = style == BRUSH_STYLE_QUILL ? 26.0f : 20.0f;
        const float y = cy + sinf(u * 6.283f + (float)style * 0.8f) * amp;
        pts[k] = (Vector2){x, y};
    }

    if (style == BRUSH_STYLE_HEAVY) {
        ink_stroke(r, pts, 10,
                   (BrushProfile){.w_max = 16.0f,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.14f,
                                  .exit = 0.30f,
                                  .dry = 0.35f,
                                  .seed = 950u},
                   (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.94f}, reveal);
    } else if (style == BRUSH_STYLE_EMBER) {
        ink_stroke(r, pts, 10,
                   (BrushProfile){.w_max = 10.5f,
                                  .w_min_ratio = 0.24f,
                                  .entry = 0.14f,
                                  .exit = 0.32f,
                                  .dry = 0.28f,
                                  .seed = 951u},
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f}, reveal);
        ink_stroke(r, pts, 10,
                   (BrushProfile){.w_max = 5.0f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.16f,
                                  .exit = 0.35f,
                                  .dry = 0.30f,
                                  .seed = 952u},
                   (InkTone){.color = PAL_SHU, .alpha = 0.88f}, reveal);
        const int tip = (int)(reveal * 8.0f);
        for (int e = 0; e < 3; ++e) {
            const float ang = t * 4.0f + (float)e * 2.1f;
            const Vector2 ep = {pts[tip].x + cosf(ang) * 12.0f,
                                pts[tip].y + sinf(ang) * 10.0f - 4.0f};
            ink_teardrop(r, ep, (Vector2){0.2f, -1.0f}, 7.0f, 4.0f,
                         pal_color_alpha(PAL_SHU_HOT, 0.92f));
        }
    } else {
        ink_stroke(r, pts, 10,
                   (BrushProfile){.w_max = 5.4f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.10f,
                                  .exit = 0.45f,
                                  .dry = 0.22f,
                                  .seed = 953u},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.90f}, reveal);
        const int tip = (int)(reveal * 8.0f);
        const Vector2 z[3] = {
            {pts[tip].x - 10.0f, pts[tip].y - 8.0f},
            {pts[tip].x + 2.0f, pts[tip].y - 16.0f},
            {pts[tip].x + 12.0f, pts[tip].y - 4.0f},
        };
        ink_stroke(r, z, 3,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.2f,
                                  .dry = 0.0f,
                                  .seed = 954u},
                   (InkTone){.color = PAL_AO_HOT, .alpha = 0.95f}, 1.0f);
        ink_arc_stroke(r, (Vector2){cx, cy}, 26.0f, t * 2.5f, t * 2.5f + 3.4f,
                       (BrushProfile){.w_max = 2.6f,
                                      .w_min_ratio = 0.3f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.2f,
                                      .seed = 955u},
                       (InkTone){.color = PAL_AO, .alpha = 0.72f});
    }
}

void render_draw_brush_select_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(PAL_PAPER_ACT1, 0.65f));

    const float cx = (float)INK_SCREEN_W * 0.5f;
    render_draw_text_centered(r, text_get(TEXT_BRUSH_SELECT_TITLE, r->use_ja), (Vector2){cx, 72.0f},
                              46.0f, PAL_SUMI_SCORCH, 0.98f, true);
    render_draw_text_centered(r, text_get(TEXT_BRUSH_SELECT_HINT, r->use_ja), (Vector2){cx, 118.0f},
                              18.0f, PAL_SUMI_HEAVY, 0.90f, false);

    BrushSelectLayout layout;
    render_brush_select_layout(&layout);
    const int32_t active = ui->hover_choice >= 0 && ui->hover_choice < BRUSH_STYLE_COUNT
                               ? ui->hover_choice
                               : (int32_t)ui->selected_brush;

    for (int i = 0; i < BRUSH_STYLE_COUNT; ++i) {
        const BrushStyleId style = (BrushStyleId)i;
        const bool sel = (active == i);
        Rectangle card = layout.cards[i];
        if (sel) {
            card.y -= 8.0f;
        }
        const PaletteId border =
            sel ? (style == BRUSH_STYLE_EMBER ? PAL_SHU
                                              : (style == BRUSH_STYLE_QUILL ? PAL_AO : PAL_KIN))
                : PAL_SUMI_DARK;
        ink_fill_panel(r, card, PAL_GOFUN, 0.96f, border, 0.94f, 960u + (uint32_t)i);

        const float ccx = card.x + card.width * 0.5f;
        char hdr[48];
        snprintf(hdr, sizeof hdr, "[%d] %s", i + 1, text_get(text_brush_name(style), r->use_ja));
        render_draw_text_centered(r, hdr, (Vector2){ccx, card.y + 38.0f}, 28.0f, PAL_SUMI_SCORCH,
                                  0.98f, false);

        const Rectangle demo_box = {card.x + 22.0f, card.y + 68.0f, card.width - 44.0f, 112.0f};
        draw_brush_trial_demo(r, demo_box, style, r->vfx_time);

        render_draw_text_centered(r, text_get(text_brush_desc(style), r->use_ja),
                                  (Vector2){ccx, card.y + 216.0f}, 18.0f, PAL_SUMI_DARK, 0.94f,
                                  false);

        const Vector2 div_pts[2] = {{card.x + 26.0f, card.y + 248.0f},
                                    {card.x + card.width - 26.0f, card.y + 248.0f}};
        ink_stroke(r, div_pts, 2,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.15f,
                                  .exit = 0.25f,
                                  .dry = 0.3f,
                                  .seed = 970u + (uint32_t)i},
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.70f}, 1.0f);

        render_draw_text_centered(r, text_get(text_brush_stats(style), r->use_ja),
                                  (Vector2){ccx, card.y + 286.0f}, 18.0f, PAL_SUMI_HEAVY, 0.94f,
                                  false);

        char rec_buf[96];
        snprintf(rec_buf, sizeof rec_buf, "%s%s", text_get(TEXT_BRUSH_REC_LABEL, r->use_ja),
                 text_get(text_brush_recommended(style), r->use_ja));
        render_draw_text_centered(r, rec_buf, (Vector2){ccx, card.y + 344.0f}, 18.0f, border, 0.96f,
                                  false);

        if (sel) {
            seal_stamp(r, TEXT_SEAL_DONE,
                       (Vector2){card.x + card.width - 34.0f, card.y + card.height - 34.0f}, 36.0f,
                       SEAL_SHUBUN, 0.7f);
        }
    }

    const bool back_hov = (ui->hover_choice == BRUSH_STYLE_COUNT);
    ink_fill_panel(r, layout.back_rect, PAL_GOFUN, 0.94f, back_hov ? PAL_SHU : PAL_SUMI_DARK, 0.92f,
                   980u);
    render_draw_text_centered(r, text_get(TEXT_BOKUAN_BACK, r->use_ja),
                              (Vector2){layout.back_rect.x + layout.back_rect.width * 0.5f,
                                        layout.back_rect.y + layout.back_rect.height * 0.5f},
                              20.0f, back_hov ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
}
