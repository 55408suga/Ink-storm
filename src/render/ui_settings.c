#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <stdio.h>
#include <string.h>

void render_settings_layout(SettingsLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    const float panel_w = 720.0f;
    const float x = ((float)INK_SCREEN_W - panel_w) * 0.5f;
    const float row_h = 54.0f;
    const float gap = 12.0f;
    const float y0 = 176.0f;

    for (int i = 0; i < 6; ++i) {
        const float y = y0 + (float)i * (row_h + gap);
        out->rows[i] = (Rectangle){x + 28.0f, y, panel_w - 56.0f, row_h};
        out->minus_rects[i] = (Rectangle){x + 412.0f, y + 8.0f, 44.0f, 38.0f};
        out->plus_rects[i] = (Rectangle){x + 628.0f, y + 8.0f, 44.0f, 38.0f};
    }
    out->back_rect = (Rectangle){((float)INK_SCREEN_W - 240.0f) * 0.5f, 594.0f, 240.0f, 54.0f};
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
    const Rectangle panel = {cx - 384.0f, 76.0f, 768.0f, 596.0f};
    ink_fill_panel(r, panel, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.92f, 1200u);

    render_draw_text_centered(r, text_get(TEXT_SETTINGS_TITLE, r->use_ja), (Vector2){cx, 118.0f},
                              44.0f, PAL_SUMI_SCORCH, 0.98f, true);
    render_draw_text_centered(r, text_get(TEXT_SETTINGS_HINT, r->use_ja), (Vector2){cx, 152.0f},
                              18.0f, PAL_SUMI_HEAVY, 0.88f, false);

    SettingsLayout layout;
    render_settings_layout(&layout);
    const SettingsState *st = &ui->meta_view.settings;
    static const TextId k_labels[6] = {
        TEXT_SETTINGS_MASTER, TEXT_SETTINGS_BGM,   TEXT_SETTINGS_SE,
        TEXT_SETTINGS_POST,   TEXT_SETTINGS_SHAKE, TEXT_SETTINGS_HINTS,
    };

    const int32_t active =
        (ui->hover_choice >= 0 && ui->hover_choice <= 6) ? ui->hover_choice : ui->settings_cursor;

    for (int i = 0; i < 6; ++i) {
        const bool sel = (active == i);
        const Rectangle row = layout.rows[i];
        ink_fill_panel(r, row, PAL_PAPER_ACT1, sel ? 0.92f : 0.62f, sel ? PAL_SHU : PAL_SUMI_CLEAR,
                       sel ? 0.92f : 0.55f, 1210u + (uint32_t)i);

        render_draw_text_id(r, k_labels[i], (Vector2){row.x + 20.0f, row.y + 16.0f}, 20.0f,
                            sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);

        /* [-] and [+] controls */
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
            snprintf(
                vbuf, sizeof vbuf, "%s",
                text_get(st->post_shader_on ? TEXT_SETTINGS_ON : TEXT_SETTINGS_OFF, r->use_ja));
        } else if (i == 4) {
            snprintf(vbuf, sizeof vbuf, "%u%%", (unsigned)st->shake_scale);
        } else {
            snprintf(vbuf, sizeof vbuf, "%s",
                     text_get(st->hints_on ? TEXT_SETTINGS_ON : TEXT_SETTINGS_OFF, r->use_ja));
        }

        const float val_cx =
            (layout.minus_rects[i].x + layout.minus_rects[i].width + layout.plus_rects[i].x) * 0.5f;
        render_draw_text_centered(r, vbuf, (Vector2){val_cx, row.y + row.height * 0.5f}, 20.0f,
                                  PAL_SUMI_SCORCH, 0.96f, false);
    }

    const bool back_sel = (active == 6);
    ink_fill_panel(r, layout.back_rect, PAL_GOFUN, 0.95f, back_sel ? PAL_SHU : PAL_SUMI_DARK, 0.92f,
                   1250u);
    render_draw_text_centered(r, text_get(TEXT_SETTINGS_BACK, r->use_ja),
                              (Vector2){layout.back_rect.x + layout.back_rect.width * 0.5f,
                                        layout.back_rect.y + layout.back_rect.height * 0.5f},
                              20.0f, back_sel ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
}
