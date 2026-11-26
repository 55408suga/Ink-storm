#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void render_settings_layout(SettingsLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    const float panel_w = 840.0f;
    const float x = ((float)INK_SCREEN_W - panel_w) * 0.5f;
    const float row_h = 42.0f;
    const float gap = 6.0f;
    const float y0 = 122.0f;

    for (int i = 0; i < SETTINGS_ROW_COUNT; ++i) {
        const float y = y0 + (float)i * (row_h + gap);
        out->rows[i] = (Rectangle){x + 28.0f, y, panel_w - 56.0f, row_h};
        out->minus_rects[i] = (Rectangle){x + 514.0f, y + 4.0f, 42.0f, 34.0f};
        out->plus_rects[i] = (Rectangle){x + 746.0f, y + 4.0f, 42.0f, 34.0f};
    }

    const float util_y = y0 + (float)SETTINGS_ROW_COUNT * (row_h + gap) + 6.0f;
    const float util_w = (panel_w - 56.0f - 24.0f) * 0.5f;
    out->reset_hints_rect = (Rectangle){x + 28.0f, util_y, util_w, 44.0f};
    out->reset_defaults_rect = (Rectangle){x + 28.0f + util_w + 24.0f, util_y, util_w, 44.0f};
    out->back_rect =
        (Rectangle){((float)INK_SCREEN_W - 230.0f) * 0.5f, util_y + 56.0f, 230.0f, 46.0f};
}

static TextId lang_label_id(uint8_t ui_lang)
{
    if (ui_lang == (uint8_t)UI_LANG_JA) {
        return TEXT_SETTINGS_LANG_JA;
    }
    if (ui_lang == (uint8_t)UI_LANG_EN) {
        return TEXT_SETTINGS_LANG_EN;
    }
    return TEXT_SETTINGS_LANG_AUTO;
}

void render_draw_settings_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL) {
        return;
    }

    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(PAL_PAPER_ACT1, 0.78f));

    const float cx = (float)INK_SCREEN_W * 0.5f;
    const Rectangle panel = {cx - 420.0f, 36.0f, 840.0f, 650.0f};
    ink_fill_panel(r, panel, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.92f, 1200u);

    render_draw_text_centered(r, text_get(TEXT_SETTINGS_TITLE, r->use_ja), (Vector2){cx, 72.0f},
                              42.0f, PAL_SUMI_SCORCH, 0.98f, true);
    render_draw_text_centered(r, text_get(TEXT_SETTINGS_HINT, r->use_ja), (Vector2){cx, 102.0f},
                              18.0f, PAL_SUMI_HEAVY, 0.88f, false);

    SettingsLayout layout;
    render_settings_layout(&layout);
    const SettingsState *st = &ui->meta_view.settings;
    static const TextId k_labels[SETTINGS_ROW_COUNT] = {
        TEXT_SETTINGS_MASTER, TEXT_SETTINGS_BGM,          TEXT_SETTINGS_SE,
        TEXT_SETTINGS_POST,   TEXT_SETTINGS_SHAKE,        TEXT_SETTINGS_HINTS,
        TEXT_SETTINGS_LANG,   TEXT_SETTINGS_REDUCE_FLASH, TEXT_SETTINGS_CURSOR_SCALE,
    };

    const int32_t max_idx = SETTINGS_ROW_COUNT + 2; /* 0..8 rows, 9 hints, 10 defaults, 11 back */
    const int32_t active = (ui->hover_choice >= 0 && ui->hover_choice <= max_idx)
                               ? ui->hover_choice
                               : ui->settings_cursor;

    for (int i = 0; i < SETTINGS_ROW_COUNT; ++i) {
        const bool sel = (active == i);
        const Rectangle row = layout.rows[i];
        ink_fill_panel(r, row, PAL_PAPER_ACT1, sel ? 0.92f : 0.62f, sel ? PAL_SHU : PAL_SUMI_CLEAR,
                       sel ? 0.92f : 0.55f, 1210u + (uint32_t)i);

        render_draw_text_id(r, k_labels[i], (Vector2){row.x + 20.0f, row.y + 11.0f}, 20.0f,
                            sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);

        /* [<] and [>] controls */
        ink_fill_panel(r, layout.minus_rects[i], PAL_GOFUN, 0.94f, sel ? PAL_SHU : PAL_SUMI_DARK,
                       0.88f, 1220u + (uint32_t)i);
        render_draw_text_centered(
            r, "<",
            (Vector2){layout.minus_rects[i].x + layout.minus_rects[i].width * 0.5f,
                      layout.minus_rects[i].y + layout.minus_rects[i].height * 0.5f},
            20.0f, PAL_SUMI_SCORCH, 0.95f, false);

        ink_fill_panel(r, layout.plus_rects[i], PAL_GOFUN, 0.94f, sel ? PAL_SHU : PAL_SUMI_DARK,
                       0.88f, 1230u + (uint32_t)i);
        render_draw_text_centered(
            r, ">",
            (Vector2){layout.plus_rects[i].x + layout.plus_rects[i].width * 0.5f,
                      layout.plus_rects[i].y + layout.plus_rects[i].height * 0.5f},
            20.0f, PAL_SUMI_SCORCH, 0.95f, false);

        char vbuf[32];
        if (i == 0) {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->master_vol);
        } else if (i == 1) {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->bgm_vol);
        } else if (i == 2) {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->se_vol);
        } else if (i == 3) {
            const TextId post_id = !render_post_available(r) ? TEXT_SETTINGS_UNAVAILABLE
                                   : st->post_shader_on      ? TEXT_SETTINGS_ON
                                                             : TEXT_SETTINGS_OFF;
            snprintf(vbuf, sizeof vbuf, "%s", text_get(post_id, r->use_ja));
        } else if (i == 4) {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->shake_scale);
        } else if (i == 5) {
            snprintf(vbuf, sizeof vbuf, "%s",
                     text_get(st->hints_on ? TEXT_SETTINGS_ON : TEXT_SETTINGS_OFF, r->use_ja));
        } else if (i == 6) {
            snprintf(vbuf, sizeof vbuf, "%s", text_get(lang_label_id(st->ui_lang), r->use_ja));
        } else if (i == 7) {
            snprintf(vbuf, sizeof vbuf, "%s",
                     text_get(st->reduce_flash ? TEXT_SETTINGS_ON : TEXT_SETTINGS_OFF, r->use_ja));
        } else {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->cursor_scale);
        }

        const float val_l = layout.minus_rects[i].x + layout.minus_rects[i].width;
        const float val_cx = (val_l + layout.plus_rects[i].x) * 0.5f;
        render_draw_text_fit(r, vbuf, (Vector2){val_cx, row.y + row.height * 0.5f},
                             layout.plus_rects[i].x - val_l - 8.0f, 20.0f, 13.0f, PAL_SUMI_SCORCH,
                             0.96f, false);
    }

    /* Utility buttons: Reset First-Time Hints (9) & Reset Settings to Defaults (10) */
    const bool hints_sel = (active == 9);
    ink_fill_panel(r, layout.reset_hints_rect, PAL_PAPER_ACT1, hints_sel ? 0.94f : 0.72f,
                   hints_sel ? PAL_SHU : PAL_SUMI_DARK, 0.90f, 1245u);
    render_draw_text_fit(
        r, text_get(TEXT_SETTINGS_RESET_HINTS, r->use_ja),
        (Vector2){layout.reset_hints_rect.x + layout.reset_hints_rect.width * 0.5f,
                  layout.reset_hints_rect.y + layout.reset_hints_rect.height * 0.5f},
        layout.reset_hints_rect.width - 20.0f, 19.0f, 13.0f, hints_sel ? PAL_SHU : PAL_SUMI_SCORCH,
        0.96f, false);

    const bool def_sel = (active == 10);
    ink_fill_panel(r, layout.reset_defaults_rect, PAL_PAPER_ACT1, def_sel ? 0.94f : 0.72f,
                   def_sel ? PAL_SHU : PAL_SUMI_DARK, 0.90f, 1246u);
    render_draw_text_fit(
        r, text_get(TEXT_SETTINGS_RESET_DEFAULTS, r->use_ja),
        (Vector2){layout.reset_defaults_rect.x + layout.reset_defaults_rect.width * 0.5f,
                  layout.reset_defaults_rect.y + layout.reset_defaults_rect.height * 0.5f},
        layout.reset_defaults_rect.width - 20.0f, 19.0f, 13.0f, def_sel ? PAL_SHU : PAL_SUMI_SCORCH,
        0.96f, false);

    /* Back button (11) */
    const bool back_sel = (active == 11);
    ink_fill_panel(r, layout.back_rect, PAL_GOFUN, 0.95f, back_sel ? PAL_SHU : PAL_SUMI_DARK, 0.92f,
                   1250u);
    render_draw_text_centered(r, text_get(TEXT_SETTINGS_BACK, r->use_ja),
                              (Vector2){layout.back_rect.x + layout.back_rect.width * 0.5f,
                                        layout.back_rect.y + layout.back_rect.height * 0.5f},
                              20.0f, back_sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);

    /* Action confirmation notice beside Back button */
    if (ui->settings_notice_t > 0.0f &&
        (ui->settings_notice_kind == 1 || ui->settings_notice_kind == 2)) {
        const TextId nid = ui->settings_notice_kind == 1 ? TEXT_SETTINGS_NOTICE_HINTS
                                                         : TEXT_SETTINGS_NOTICE_DEFAULTS;
        const float na = ink_clampf(ui->settings_notice_t * 2.0f, 0.0f, 1.0f);
        const float nx = layout.back_rect.x + layout.back_rect.width + 16.0f;
        const float panel_r = cx + 420.0f - 16.0f;
        const Rectangle notice = {nx, layout.back_rect.y, fmaxf(40.0f, panel_r - nx),
                                  layout.back_rect.height};
        render_draw_text_wrapped(r, text_get(nid, r->use_ja), notice, 18.0f, 12.0f, PAL_SHU,
                                 0.96f * na, false, false);
    }
}
