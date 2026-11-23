#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void render_result_layout(bool show_endless, ResultLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    const float cx = (float)INK_SCREEN_W * 0.5f;
    if (show_endless) {
        const float bw = 246.0f;
        const float gap = 26.0f;
        const float total_w = bw * 3.0f + gap * 2.0f;
        const float x0 = cx - total_w * 0.5f;
        out->has_endless = true;
        out->endless_rect = (Rectangle){x0, 580.0f, bw, 54.0f};
        out->lobby_rect = (Rectangle){x0 + bw + gap, 580.0f, bw, 54.0f};
        out->retry_rect = (Rectangle){x0 + (bw + gap) * 2.0f, 580.0f, bw, 54.0f};
    } else {
        out->has_endless = false;
        out->lobby_rect = (Rectangle){cx - 260.0f, 580.0f, 236.0f, 54.0f};
        out->retry_rect = (Rectangle){cx + 24.0f, 580.0f, 236.0f, 54.0f};
    }
}

static void draw_ledger_row(const RenderState *r, float x, float y, float w, TextId label,
                            const char *val_str, PaletteId val_pal)
{
    render_draw_text_id(r, label, (Vector2){x, y}, 20.0f, PAL_SUMI_HEAVY, 0.95f, false);
    const Vector2 m = render_measure_text(r, val_str, 20.0f, false);
    render_draw_number_fixed(r, val_str, (Vector2){x + w - m.x, y}, 20.0f, val_pal, 0.96f);
}

static void draw_ledger_text_row(const RenderState *r, float x, float y, float w, TextId label,
                                 const char *val_str, PaletteId val_pal)
{
    render_draw_text_id(r, label, (Vector2){x, y}, 20.0f, PAL_SUMI_HEAVY, 0.95f, false);
    const Vector2 m = render_measure_text(r, val_str, 20.0f, false);
    render_draw_text(r, val_str, (Vector2){x + w - m.x, y}, 20.0f, val_pal, 0.96f, false);
}

void render_draw_result_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL ||
        (g->phase != RUN_PHASE_VICTORY && g->phase != RUN_PHASE_GAME_OVER)) {
        return;
    }

    const float open = ink_clampf(r->victory_scroll_t, 0.05f, 1.0f);
    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(pal_paper_for_act(g->run.act_index), 0.78f));

    const float scroll_w = 1040.0f * open;
    const float scroll_x = ((float)INK_SCREEN_W - 1040.0f) * 0.5f + (1040.0f - scroll_w);
    const Rectangle scroll = {scroll_x, 60.0f, scroll_w, 600.0f};
    ink_fill_panel(r, scroll, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.92f, 500u);

    const Rectangle left_rod = {scroll.x - 14.0f, scroll.y - 10.0f, 16.0f, scroll.height + 20.0f};
    const Rectangle right_rod = {scroll.x + scroll.width - 2.0f, scroll.y - 10.0f, 16.0f,
                                 scroll.height + 20.0f};
    ink_fill_panel(r, left_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 501u);
    ink_fill_panel(r, right_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 502u);

    if (open < 0.75f) {
        return;
    }

    const bool win = (g->phase == RUN_PHASE_VICTORY);
    const float cx = (float)INK_SCREEN_W * 0.5f;
    render_draw_text_centered(
        r, text_get(win ? TEXT_RESULT_VICTORY : TEXT_RESULT_DEFEAT, r->use_ja),
        (Vector2){cx, 110.0f}, 42.0f, win ? PAL_SUMI_SCORCH : PAL_BENGARA, 0.98f, true);

    seal_stamp(r, win ? TEXT_SEAL_SLAY : TEXT_SEAL_PURIFY,
               (Vector2){scroll.x + scroll.width - 78.0f, 114.0f}, 58.0f, SEAL_HAKUBUN, open);

    /* Left column: Run statistics */
    const float lx = scroll.x + 56.0f;
    const float lw = 420.0f;
    float y = 162.0f;
    char buf[96];

    if (g->run.act_index >= GAME_MAIN_ACT_COUNT) {
        const int32_t loop_n = g->run.act_index / GAME_MAIN_ACT_COUNT;
        if (r->use_ja) {
            if (g->run.wave_index >= GAME_BOSS_WAVE_INDEX) {
                snprintf(buf, sizeof buf, "%s%d%s %s (%s%d)",
                         text_get(TEXT_ENDLESS_ACT_PREFIX, true), g->run.act_index + 1,
                         text_get(TEXT_ENDLESS_ACT_SUFFIX, true), text_get(TEXT_HUD_BOSS, true),
                         text_get(TEXT_ENDLESS_LOOP_LABEL, true), loop_n);
            } else {
                snprintf(buf, sizeof buf, "%s%d%s %d/3 (%s%d)",
                         text_get(TEXT_ENDLESS_ACT_PREFIX, true), g->run.act_index + 1,
                         text_get(TEXT_ENDLESS_ACT_SUFFIX, true), g->run.wave_index + 1,
                         text_get(TEXT_ENDLESS_LOOP_LABEL, true), loop_n);
            }
        } else {
            snprintf(buf, sizeof buf, "Act %d-%d (Loop %d)", g->run.act_index + 1,
                     g->run.wave_index + 1, loop_n);
        }
        draw_ledger_text_row(r, lx, y, lw, TEXT_RESULT_REACHED, buf, PAL_SUMI_SCORCH);
    } else {
        snprintf(buf, sizeof buf, "%d-%d", g->run.act_index + 1, g->run.wave_index + 1);
        draw_ledger_row(r, lx, y, lw, TEXT_RESULT_REACHED, buf, PAL_SUMI_SCORCH);
    }
    y += 30.0f;

    snprintf(buf, sizeof buf, "%d", g->run.score);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_SCORE, buf, PAL_SUMI_SCORCH);
    y += 30.0f;

    snprintf(buf, sizeof buf, "%d", g->run.kills);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_KILLS, buf, PAL_SUMI_SCORCH);
    y += 30.0f;

    snprintf(buf, sizeof buf, "%d", g->run.max_chain);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_MAX_CHAIN, buf, PAL_KIN);
    y += 36.0f;

    render_draw_text_id(r, TEXT_RESULT_REACTIONS, (Vector2){lx, y}, 20.0f, PAL_SUMI_DARK, 0.96f,
                        false);
    y += 28.0f;
    snprintf(buf, sizeof buf, "%d", g->run.ignitions);
    draw_ledger_row(r, lx + 16.0f, y, lw - 16.0f, TEXT_RESULT_BLAZING_LABEL, buf, PAL_SHU);
    y += 26.0f;
    snprintf(buf, sizeof buf, "%d", g->run.storm_zaps);
    draw_ledger_row(r, lx + 16.0f, y, lw - 16.0f, TEXT_RESULT_TEMPEST_LABEL, buf, PAL_AO);
    y += 26.0f;
    snprintf(buf, sizeof buf, "%d", g->run.thunderbursts);
    draw_ledger_row(r, lx + 16.0f, y, lw - 16.0f, TEXT_RESULT_THUNDERBURST_LABEL, buf, PAL_SHU);
    y += 26.0f;
    snprintf(buf, sizeof buf, "%d", g->run.cataclysms);
    draw_ledger_row(r, lx + 16.0f, y, lw - 16.0f, TEXT_RESULT_CATACLYSM_LABEL, buf, PAL_KIN);
    y += 34.0f;

    snprintf(buf, sizeof buf, "0x%016" PRIx64, g->config.seed);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_SEED, buf, PAL_SUMI_HEAVY);

    const Vector2 vdiv[2] = {{cx, 160.0f}, {cx, 558.0f}};
    ink_stroke(r, vdiv, 2,
               (BrushProfile){.w_max = 2.6f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.1f,
                              .exit = 0.2f,
                              .dry = 0.3f,
                              .seed = 510u},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.72f}, 1.0f);

    /* Right column: Ink Crystal settlement / preview + up to 9 upgrades (2 cols) + relics */
    const float rx = cx + 40.0f;
    const float rw = 436.0f;
    float ry = 160.0f;
    const bool has_breakdown = (ui != NULL && (ui->settled || ui->settle_preview));
    const TextId cry_hdr =
        (ui != NULL && ui->settle_preview && !ui->settled) ? TEXT_RESULT_CRYSTALS_PREVIEW
                                                           : TEXT_RESULT_CRYSTALS;
    render_draw_text_id(r, cry_hdr, (Vector2){rx, ry}, 20.0f, PAL_SUMI_SCORCH, 0.96f, false);
    ry += 26.0f;

    const MetaSettleBreakdown st = has_breakdown ? ui->settle : (MetaSettleBreakdown){0};
    const int extra_rows = (st.from_endless > 0 ? 1 : 0) + (st.from_bonus > 0 ? 1 : 0);
    const float row_step = extra_rows > 0 ? 22.0f : 24.0f;

    snprintf(buf, sizeof buf, "+%d", st.from_acts);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_ACTS, buf, PAL_SUMI_DARK);
    ry += row_step;
    snprintf(buf, sizeof buf, "+%d", st.from_kills);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_KILLS, buf, PAL_SUMI_DARK);
    ry += row_step;
    snprintf(buf, sizeof buf, "+%d", st.from_chain);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_CHAIN, buf, PAL_SUMI_DARK);
    ry += row_step;
    snprintf(buf, sizeof buf, "+%d", st.from_reactions);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_REACTIONS, buf, PAL_SUMI_DARK);
    ry += row_step;

    if (st.from_endless > 0) {
        snprintf(buf, sizeof buf, "+%d", st.from_endless);
        draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_ENDLESS, buf, PAL_KIN);
        ry += row_step;
    }
    if (st.from_bonus > 0) {
        snprintf(buf, sizeof buf, "+%d", st.from_bonus);
        draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_BONUS, buf, PAL_SHU);
        ry += row_step;
    }

    snprintf(buf, sizeof buf, "+%d", st.total_crystals);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_TOTAL, buf, PAL_KIN);
    ry += row_step;
    if (has_breakdown) {
        snprintf(buf, sizeof buf, "%d", ui->crystals_after);
        draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_OWNED, buf, PAL_SUMI_SCORCH);
    }
    ry += 26.0f;

    /* Owned upgrades (up to 9, 2 columns compact) */
    render_draw_text_id(r, TEXT_RESULT_UPGRADES, (Vector2){rx, ry}, 18.0f, PAL_SUMI_DARK, 0.96f,
                        false);
    ry += 22.0f;
    const int32_t shown_upgs = g->run.upgrade_count < 9 ? g->run.upgrade_count : 9;
    const float upg_step = extra_rows > 0 ? 20.0f : 22.0f;
    for (int32_t i = 0; i < shown_upgs; ++i) {
        const OwnedUpgrade *ou = &g->run.upgrades[i];
        const int col = i % 2;
        const int row = i / 2;
        snprintf(buf, sizeof buf, "%s %s%u", text_get(text_upgrade_name(ou->id), r->use_ja),
                 text_get(TEXT_REWARD_LV, r->use_ja), (unsigned)ou->level);
        render_draw_text(r, buf, (Vector2){rx + (float)col * 218.0f, ry + (float)row * upg_step},
                         18.0f, pal_axis_color(game_upgrade_axis(ou->id)), 0.94f, false);
    }
    const int upg_rows = (shown_upgs + 1) / 2;
    ry += (float)(upg_rows > 0 ? upg_rows : 1) * upg_step + 8.0f;

    /* Owned relics */
    render_draw_text_id(r, TEXT_RESULT_RELICS, (Vector2){rx, ry}, 18.0f, PAL_SUMI_DARK, 0.96f,
                        false);
    ry += 22.0f;
    if (g->run.relic_count <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){rx + 8.0f, ry}, 18.0f, PAL_SUMI_LIGHT,
                            0.90f, false);
    } else {
        const int32_t shown_rel = g->run.relic_count < 4 ? g->run.relic_count : 4;
        for (int32_t i = 0; i < shown_rel; ++i) {
            const int col = i % 2;
            const int row = i / 2;
            render_draw_text_id(r, text_relic_name(g->run.relics[i]),
                                (Vector2){rx + (float)col * 218.0f, ry + (float)row * 20.0f}, 18.0f,
                                PAL_KIN, 0.96f, false);
        }
    }

    /* Action buttons: [Enter Endless] (on main campaign Victory) + Return to Lobby + Play Again */
    const bool show_endless =
        (g->phase == RUN_PHASE_VICTORY && !g->run.run_settled && g->run.endless_loop == 0);
    ResultLayout layout;
    render_result_layout(show_endless, &layout);
    const int32_t hov = ui != NULL ? ui->hover_choice : UI_HOVER_NONE;

    if (layout.has_endless) {
        const bool ehov = (hov == 2);
        ink_fill_panel(r, layout.endless_rect, ehov ? PAL_PAPER_ACT1 : PAL_GOFUN, 0.98f, PAL_SHU,
                       0.98f, 519u);
        render_draw_text_centered(
            r, text_get(TEXT_RESULT_ENDLESS, r->use_ja),
            (Vector2){layout.endless_rect.x + layout.endless_rect.width * 0.5f,
                      layout.endless_rect.y + layout.endless_rect.height * 0.5f},
            20.0f, PAL_SHU, 0.98f, false);
    }

    ink_fill_panel(r, layout.lobby_rect, PAL_GOFUN, 0.96f, hov == 0 ? PAL_SHU : PAL_SUMI_DARK,
                   0.94f, 520u);
    render_draw_text_centered(r, text_get(TEXT_RESULT_TO_LOBBY, r->use_ja),
                              (Vector2){layout.lobby_rect.x + layout.lobby_rect.width * 0.5f,
                                        layout.lobby_rect.y + layout.lobby_rect.height * 0.5f},
                              20.0f, hov == 0 ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);

    ink_fill_panel(r, layout.retry_rect, PAL_GOFUN, 0.96f, hov == 1 ? PAL_SHU : PAL_SUMI_DARK,
                   0.94f, 521u);
    render_draw_text_centered(r, text_get(TEXT_RESULT_PLAY_AGAIN, r->use_ja),
                              (Vector2){layout.retry_rect.x + layout.retry_rect.width * 0.5f,
                                        layout.retry_rect.y + layout.retry_rect.height * 0.5f},
                              20.0f, hov == 1 ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f, false);
}
