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
    ink_fill_panel(r, build_panel, PAL_GOFUN, 0.94f, PAL_SUMI_DARK, 0.90f, 1100u);
    render_draw_text_id(r, TEXT_PAUSE_BUILD_HDR,
                        (Vector2){build_panel.x + 24.0f, build_panel.y + 20.0f}, 22.0f,
                        PAL_SUMI_SCORCH, 0.98f, false);

    float y = build_panel.y + 62.0f;
    if (g->run.upgrade_count <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){build_panel.x + 28.0f, y}, 18.0f,
                            PAL_SUMI_LIGHT, 0.92f, false);
        y += 28.0f;
    } else {
        const int32_t shown = g->run.upgrade_count < 12 ? g->run.upgrade_count : 12;
        for (int32_t i = 0; i < shown; ++i) {
            char buf[96];
            const OwnedUpgrade *ou = &g->run.upgrades[i];
            const int col = i % 2;
            const int row = i / 2;
            snprintf(buf, sizeof buf, "%s %s%u", text_get(text_upgrade_name(ou->id), r->use_ja),
                     text_get(TEXT_REWARD_LV, r->use_ja), (unsigned)ou->level);
            render_draw_text(
                r, buf,
                (Vector2){build_panel.x + 24.0f + (float)col * 224.0f, y + (float)row * 26.0f},
                18.0f, pal_axis_color(game_upgrade_axis(ou->id)), 0.95f, false);
        }
        y += (float)((shown + 1) / 2) * 26.0f + 22.0f;
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
    y += 18.0f;

    render_draw_text_id(r, TEXT_RESULT_RELICS, (Vector2){build_panel.x + 24.0f, y}, 20.0f,
                        PAL_SUMI_DARK, 0.96f, false);
    y += 30.0f;
    if (g->run.relic_count <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){build_panel.x + 28.0f, y}, 18.0f,
                            PAL_SUMI_LIGHT, 0.92f, false);
    } else {
        const int32_t shown_rel = g->run.relic_count < 6 ? g->run.relic_count : 6;
        for (int32_t i = 0; i < shown_rel; ++i) {
            const RelicId rid = g->run.relics[i];
            render_draw_text_id(r, text_relic_name(rid),
                                (Vector2){build_panel.x + 28.0f, y + (float)i * 26.0f}, 19.0f,
                                PAL_KIN, 0.96f, false);
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
