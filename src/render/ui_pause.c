#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <stdio.h>
#include <string.h>

void render_pause_layout(PauseLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    const float x = 668.0f;
    const float w = 312.0f;
    const float h = 58.0f;
    const float gap = 16.0f;
    const float y0 = 232.0f;
    for (int i = 0; i < 4; ++i) {
        out->items[i] = (Rectangle){x, y0 + (float)i * (h + gap), w, h};
    }

    const float cx = (float)INK_SCREEN_W * 0.5f;
    out->confirm_yes = (Rectangle){cx - 190.0f, 388.0f, 168.0f, 52.0f};
    out->confirm_no = (Rectangle){cx + 22.0f, 388.0f, 168.0f, 52.0f};
}

static void draw_pause_cell_text(const RenderState *r, const char *text, Vector2 pos, float max_w,
                                 float size, float min_size, PaletteId color, float alpha)
{
    if (text == NULL || text[0] == '\0') {
        return;
    }
    float s = size;
    while (s > min_size && render_measure_text(r, text, s, false).x > max_w) {
        s -= 1.0f;
    }
    render_draw_text(r, text, pos, s, color, alpha, false);
}

static void format_more_badge(bool use_ja, int32_t hidden, char *out, size_t cap)
{
    const char *word = text_get(TEXT_RESULT_MORE_ITEMS, use_ja);
    if (use_ja) {
        snprintf(out, cap, "%s +%d", word, hidden);
    } else {
        snprintf(out, cap, "+%d %s", hidden, word);
    }
}

void render_draw_pause_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    /* Translucent washi veil over paused combat */
    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(pal_paper_for_act(g->run.act_index), 0.78f));

    /* Left panel: current run's owned upgrades & relics */
    const Rectangle build_panel = {112.0f, 136.0f, 476.0f, 476.0f};
    const float panel_bottom = build_panel.y + build_panel.height - 14.0f;
    ink_fill_panel(r, build_panel, PAL_GOFUN, 0.94f, PAL_SUMI_DARK, 0.90f, 1100u);
    render_draw_text_id(r, TEXT_PAUSE_BUILD_HDR,
                        (Vector2){build_panel.x + 24.0f, build_panel.y + 18.0f}, 22.0f,
                        PAL_SUMI_SCORCH, 0.98f, false);

    const int32_t rel_n = g->run.relic_count;
    const int32_t rel_cols = rel_n > 4 ? 2 : 1;
    const float rel_row_h = rel_n > 8 ? 20.0f : 24.0f;
    const float rel_font = rel_n > 8 ? 15.0f : 18.0f;
    const int32_t rel_rows_need = rel_n > 0 ? (rel_n + rel_cols - 1) / rel_cols : 1;
    const int32_t rel_rows_res = rel_rows_need < 8 ? rel_rows_need : 8;
    const float rel_reserved_h = 46.0f + (float)rel_rows_res * rel_row_h;

    float y = build_panel.y + 56.0f;
    const int32_t upg_n = g->run.upgrade_count;
    if (upg_n <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){build_panel.x + 28.0f, y}, 18.0f,
                            PAL_SUMI_LIGHT, 0.92f, false);
        y += 28.0f;
    } else {
        const float upg_row_h = upg_n > 18 ? 18.0f : (upg_n > 12 ? 20.0f : 25.0f);
        const float upg_font = upg_n > 18 ? 14.0f : (upg_n > 12 ? 15.0f : 17.5f);
        const float upg_avail_h = panel_bottom - y - rel_reserved_h;
        int32_t max_rows = (int32_t)(upg_avail_h / upg_row_h);
        if (max_rows < 2) {
            max_rows = 2;
        }
        const int32_t max_slots = max_rows * 2;
        const bool upg_overflow = upg_n > max_slots;
        const int32_t shown = upg_overflow ? (max_slots - 1) : upg_n;
        for (int32_t i = 0; i < shown; ++i) {
            char buf[96];
            const OwnedUpgrade *ou = &g->run.upgrades[i];
            const int col = i % 2;
            const int row = i / 2;
            snprintf(buf, sizeof buf, "%s %s%u", text_get(text_upgrade_name(ou->id), r->use_ja),
                     text_get(TEXT_REWARD_LV, r->use_ja), (unsigned)ou->level);
            draw_pause_cell_text(
                r, buf,
                (Vector2){build_panel.x + 24.0f + (float)col * 222.0f, y + (float)row * upg_row_h},
                214.0f, upg_font, 12.0f, pal_axis_color(game_upgrade_axis(ou->id)), 0.95f);
        }
        if (upg_overflow) {
            char more_buf[32];
            format_more_badge(r->use_ja, upg_n - shown, more_buf, sizeof more_buf);
            const int col = shown % 2;
            const int row = shown / 2;
            draw_pause_cell_text(
                r, more_buf,
                (Vector2){build_panel.x + 24.0f + (float)col * 222.0f, y + (float)row * upg_row_h},
                214.0f, upg_font, 12.0f, PAL_SHU, 0.96f);
        }
        const int32_t used_slots = upg_overflow ? max_slots : shown;
        y += (float)((used_slots + 1) / 2) * upg_row_h + 10.0f;
    }

    const Vector2 div[2] = {{build_panel.x + 24.0f, y},
                            {build_panel.x + build_panel.width - 24.0f, y}};
    ink_stroke(r, div, 2,
               (BrushProfile){.w_max = 2.4f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.12f,
                              .exit = 0.25f,
                              .dry = 0.30f,
                              .seed = 1105u},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.70f}, 1.0f);
    y += 12.0f;

    render_draw_text_id(r, TEXT_RESULT_RELICS, (Vector2){build_panel.x + 24.0f, y}, 19.0f,
                        PAL_SUMI_DARK, 0.96f, false);
    y += 24.0f;
    if (rel_n <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){build_panel.x + 28.0f, y}, 18.0f,
                            PAL_SUMI_LIGHT, 0.92f, false);
    } else {
        int32_t max_rel_rows = (int32_t)((panel_bottom - y) / rel_row_h);
        if (max_rel_rows < 1) {
            max_rel_rows = 1;
        }
        const int32_t rel_cap = max_rel_rows * rel_cols;
        const bool rel_overflow = rel_n > rel_cap;
        const int32_t shown_rel = rel_overflow ? (rel_cap - 1) : rel_n;
        const float rel_col_w = rel_cols == 2 ? 222.0f : 428.0f;
        const float rel_max_w = rel_cols == 2 ? 214.0f : 420.0f;
        for (int32_t i = 0; i < shown_rel; ++i) {
            const RelicId rid = g->run.relics[i];
            const int col = i % rel_cols;
            const int row = i / rel_cols;
            draw_pause_cell_text(r, text_get(text_relic_name(rid), r->use_ja),
                                 (Vector2){build_panel.x + 24.0f + (float)col * rel_col_w,
                                           y + (float)row * rel_row_h},
                                 rel_max_w, rel_font, 12.0f, PAL_KIN, 0.96f);
        }
        if (rel_overflow) {
            char more_buf[32];
            format_more_badge(r->use_ja, rel_n - shown_rel, more_buf, sizeof more_buf);
            const int col = shown_rel % rel_cols;
            const int row = shown_rel / rel_cols;
            draw_pause_cell_text(r, more_buf,
                                 (Vector2){build_panel.x + 24.0f + (float)col * rel_col_w,
                                           y + (float)row * rel_row_h},
                                 rel_max_w, rel_font, 12.0f, PAL_SHU, 0.96f);
        }
    }

    /* Right panel: Pause menu */
    const Rectangle menu_panel = {626.0f, 136.0f, 396.0f, 476.0f};
    ink_fill_panel(r, menu_panel, PAL_GOFUN, 0.94f, PAL_SUMI_DARK, 0.90f, 1110u);
    render_draw_text_centered(r, text_get(TEXT_PAUSE_TITLE, r->use_ja),
                              (Vector2){menu_panel.x + menu_panel.width * 0.5f, 184.0f}, 44.0f,
                              PAL_SUMI_SCORCH, 0.98f, true);

    PauseLayout layout;
    render_pause_layout(&layout);
    static const TextId k_items[4] = {
        TEXT_PAUSE_RESUME,
        TEXT_PAUSE_RESTART,
        TEXT_PAUSE_SETTINGS,
        TEXT_PAUSE_LOBBY,
    };
    const int32_t active =
        (ui->pause_confirm == PAUSE_CONFIRM_NONE && ui->hover_choice >= 0 && ui->hover_choice < 4)
            ? ui->hover_choice
            : ui->pause_cursor;

    for (int i = 0; i < 4; ++i) {
        const bool sel = (active == i) && (ui->pause_confirm == PAUSE_CONFIRM_NONE);
        ink_fill_panel(r, layout.items[i], PAL_GOFUN, 0.96f, sel ? PAL_SHU : PAL_SUMI_DARK, 0.92f,
                       1120u + (uint32_t)i);
        render_draw_text_centered(r, text_get(k_items[i], r->use_ja),
                                  (Vector2){layout.items[i].x + layout.items[i].width * 0.5f,
                                            layout.items[i].y + layout.items[i].height * 0.5f},
                                  22.0f, sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
    }

    /* Confirmation dialog when Restart or Return to Lobby is chosen */
    if (ui->pause_confirm != PAUSE_CONFIRM_NONE) {
        ink_fill_poly(r, full, 4, pal_color_alpha(PAL_SUMI_SCORCH, 0.42f));
        const float cx = (float)INK_SCREEN_W * 0.5f;
        const Rectangle dlg = {cx - 330.0f, 260.0f, 660.0f, 214.0f};
        ink_fill_panel(r, dlg, PAL_GOFUN, 0.98f, PAL_BENGARA, 0.95f, 1140u);
        render_draw_text_centered(r, text_get(TEXT_PAUSE_CONFIRM_MSG, r->use_ja),
                                  (Vector2){cx, 326.0f}, 20.0f, PAL_SUMI_SCORCH, 0.98f, false);

        const int32_t c_active = (ui->hover_choice == 0 || ui->hover_choice == 1)
                                     ? ui->hover_choice
                                     : ui->confirm_cursor;
        const bool yes_sel = (c_active == 0);
        const bool no_sel = (c_active == 1);

        ink_fill_panel(r, layout.confirm_yes, PAL_GOFUN, 0.96f, yes_sel ? PAL_SHU : PAL_SUMI_DARK,
                       0.94f, 1141u);
        render_draw_text_centered(
            r, text_get(TEXT_PAUSE_CONFIRM_YES, r->use_ja),
            (Vector2){layout.confirm_yes.x + layout.confirm_yes.width * 0.5f,
                      layout.confirm_yes.y + layout.confirm_yes.height * 0.5f},
            22.0f, yes_sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.98f, false);

        ink_fill_panel(r, layout.confirm_no, PAL_GOFUN, 0.96f, no_sel ? PAL_SHU : PAL_SUMI_DARK,
                       0.94f, 1142u);
        render_draw_text_centered(r, text_get(TEXT_PAUSE_CONFIRM_NO, r->use_ja),
                                  (Vector2){layout.confirm_no.x + layout.confirm_no.width * 0.5f,
                                            layout.confirm_no.y + layout.confirm_no.height * 0.5f},
                                  22.0f, no_sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.98f, false);
    }
}
