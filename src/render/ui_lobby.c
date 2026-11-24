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
    out->title_rect = (Rectangle){548.0f, 168.0f, 556.0f, 198.0f};
    out->footer_rect = (Rectangle){64.0f, 608.0f, (float)INK_SCREEN_W - 128.0f, 58.0f};
    out->brush_rect = (Rectangle){x, y0 + 4.0f * (h + gap) + 4.0f, w, 52.0f};
    out->hint_rect = (Rectangle){500.0f, 566.0f, 640.0f, 32.0f};
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

static void draw_brush_switcher(RenderState *r, Rectangle box, BrushStyleId style)
{
    const PaletteId accent = style == BRUSH_STYLE_EMBER   ? PAL_SHU
                             : style == BRUSH_STYLE_QUILL ? PAL_AO
                                                          : PAL_SUMI_SCORCH;
    ink_fill_panel(r, box, PAL_GOFUN, 0.88f, accent, 0.85f, 930u);
    const float cy = box.y + box.height * 0.5f;
    ink_teardrop(r, (Vector2){box.x + 18.0f, cy}, (Vector2){-1.0f, 0.0f}, 9.0f, 6.0f,
                 pal_color_alpha(PAL_SUMI_DARK, 0.85f));
    ink_teardrop(r, (Vector2){box.x + box.width - 18.0f, cy}, (Vector2){1.0f, 0.0f}, 9.0f, 6.0f,
                 pal_color_alpha(PAL_SUMI_DARK, 0.85f));
    render_draw_text_fit(r, text_get(TEXT_LOBBY_TEST_BRUSH, r->use_ja),
                         (Vector2){box.x + box.width * 0.5f, box.y + 15.0f}, box.width - 72.0f,
                         15.0f, 12.0f, PAL_SUMI_HEAVY, 0.88f, false);
    render_draw_text_fit(r, text_get(text_brush_name(style), r->use_ja),
                         (Vector2){box.x + box.width * 0.5f, box.y + 35.0f}, box.width - 72.0f,
                         22.0f, 15.0f, accent, 0.98f, false);
}

static void draw_footer_records(RenderState *r, Rectangle footer, const MetaState *m)
{
    ink_fill_panel(r, footer, PAL_GOFUN, 0.88f, PAL_SUMI_LIGHT, 0.72f, 920u);
    char vals[4][32];
    snprintf(vals[0], sizeof vals[0], "%d", m->ink_crystals);
    snprintf(vals[1], sizeof vals[1], "%d", m->best_act);
    snprintf(vals[2], sizeof vals[2], "%07d", m->best_score);
    snprintf(vals[3], sizeof vals[3], "%d", m->clears);
    static const TextId k_labels[4] = {TEXT_LOBBY_CRYSTALS, TEXT_LOBBY_BEST_ACT,
                                       TEXT_LOBBY_BEST_SCORE, TEXT_LOBBY_CLEARS};
    static const PaletteId k_val_pal[4] = {PAL_KIN, PAL_SUMI_SCORCH, PAL_SUMI_SCORCH, PAL_SHU};
    const float slot_w = (footer.width - 32.0f) / 4.0f;
    const float fy = footer.y + 18.0f;
    for (int i = 0; i < 4; ++i) {
        const char *label = text_get(k_labels[i], r->use_ja);
        const float label_w = render_measure_text(r, label, 20.0f, false).x;
        const float val_w = render_measure_number_fixed(r, vals[i], 20.0f);
        const float total = label_w + 12.0f + val_w;
        const float x = footer.x + 16.0f + slot_w * (float)i + fmaxf(0.0f, (slot_w - total) * 0.5f);
        render_draw_text(r, label, (Vector2){x, fy}, 20.0f, PAL_SUMI_HEAVY, 0.95f, false);
        render_draw_number_fixed(r, vals[i], (Vector2){x + label_w + 12.0f, fy}, 20.0f,
                                 k_val_pal[i], 0.97f);
    }
}

void render_draw_lobby_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    LobbyLayout layout;
    render_lobby_layout(&layout);

    /* Calligraphic title "墨嵐" & vermilion seal on right-center */
    const char *title_str = text_get(TEXT_GAME_TITLE, r->use_ja);
    const float title_px = r->use_ja ? 88.0f : 72.0f;
    const Vector2 title_m = render_measure_text(r, title_str, title_px, true);
    const Vector2 title_center = {800.0f, 258.0f};
    ink_fill_panel(r, layout.title_rect, PAL_GOFUN, 0.62f, PAL_SUMI_CLEAR, 0.0f, 900u);
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

    draw_brush_switcher(r, layout.brush_rect, ui->selected_brush);
    draw_footer_records(r, layout.footer_rect, &ui->meta_view);

    /* Canvas hint sits on its own washi pill so test strokes never drown it out. */
    const char *hint = text_get(TEXT_LOBBY_CANVAS_HINT, r->use_ja);
    const float hint_w =
        fminf(layout.hint_rect.width, render_measure_text(r, hint, 18.0f, false).x + 32.0f);
    const Rectangle pill = {layout.hint_rect.x + (layout.hint_rect.width - hint_w) * 0.5f,
                            layout.hint_rect.y, hint_w, layout.hint_rect.height};
    ink_fill_panel(r, pill, PAL_GOFUN, 0.84f, PAL_SUMI_LIGHT, 0.45f, 925u);
    render_draw_text_fit(r, hint,
                         (Vector2){pill.x + pill.width * 0.5f, pill.y + pill.height * 0.5f},
                         pill.width - 20.0f, 18.0f, 13.0f, PAL_SUMI_HEAVY, 0.92f, false);

    if (ui->lobby_canvas_hover) {
        render_draw_brush_cursor(r, g, ui);
    }
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
        render_draw_text_fit(r, hdr, (Vector2){ccx, card.y + 38.0f}, card.width - 40.0f, 28.0f,
                             20.0f, PAL_SUMI_SCORCH, 0.98f, false);

        const Rectangle demo_box = {card.x + 22.0f, card.y + 68.0f, card.width - 44.0f, 112.0f};
        draw_brush_trial_demo(r, demo_box, style, r->vfx_time);

        render_draw_text_wrapped(
            r, text_get(text_brush_desc(style), r->use_ja),
            (Rectangle){card.x + 22.0f, card.y + 190.0f, card.width - 44.0f, 52.0f}, 18.0f, 14.0f,
            PAL_SUMI_DARK, 0.94f, false, true);

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

        render_draw_text_wrapped(
            r, text_get(text_brush_stats(style), r->use_ja),
            (Rectangle){card.x + 22.0f, card.y + 258.0f, card.width - 44.0f, 62.0f}, 18.0f, 14.0f,
            PAL_SUMI_HEAVY, 0.94f, false, true);

        char rec_buf[96];
        snprintf(rec_buf, sizeof rec_buf, "%s%s", text_get(TEXT_BRUSH_REC_LABEL, r->use_ja),
                 text_get(text_brush_recommended(style), r->use_ja));
        render_draw_text_fit(r, rec_buf, (Vector2){ccx, card.y + 344.0f}, card.width - 40.0f, 18.0f,
                             13.0f, border, 0.96f, false);

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
