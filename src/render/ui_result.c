#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <inttypes.h>
#include <stdio.h>

static void draw_ledger_row(const RenderState *r, float x, float y, float w, TextId label,
                            const char *val_str, PaletteId val_pal)
{
    render_draw_text_id(r, label, (Vector2){x, y}, 20.0f, PAL_SUMI_HEAVY, 0.95f, false);
    const Vector2 m = render_measure_text(r, val_str, 20.0f, false);
    render_draw_number_fixed(r, val_str, (Vector2){x + w - m.x, y}, 20.0f, val_pal, 0.96f);
}

void render_draw_result_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL ||
        (g->phase != RUN_PHASE_VICTORY && g->phase != RUN_PHASE_GAME_OVER)) {
        return;
    }

    const float open = r->victory_scroll_t > 0.1f ? r->victory_scroll_t : 1.0f;
    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(pal_paper_for_act(g->run.act_index), 0.78f));

    /* Scroll unrolling right-to-left */
    const float scroll_w = 1040.0f * open;
    const float scroll_x = ((float)INK_SCREEN_W - 1040.0f) * 0.5f + (1040.0f - scroll_w);
    const Rectangle scroll = {scroll_x, 68.0f, scroll_w, 584.0f};
    ink_fill_panel(r, scroll, PAL_GOFUN, 0.96f, PAL_SUMI_DARK, 0.92f, 500u);

    /* Scroll rollers on left & right */
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
        (Vector2){cx, 118.0f}, 42.0f, win ? PAL_SUMI_SCORCH : PAL_BENGARA, 0.98f, true);

    seal_stamp(r, win ? TEXT_SEAL_SLAY : TEXT_SEAL_PURIFY,
               (Vector2){scroll.x + scroll.width - 78.0f, 122.0f}, 58.0f, SEAL_HAKUBUN, open);

    /* Left column: Run statistics */
    const float lx = scroll.x + 56.0f;
    const float lw = 420.0f;
    float y = 176.0f;
    char buf[96];

    snprintf(buf, sizeof buf, "%d-%d", g->run.act_index + 1, g->run.wave_index + 1);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_REACHED, buf, PAL_SUMI_SCORCH);
    y += 32.0f;

    snprintf(buf, sizeof buf, "%d", g->run.score);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_SCORE, buf, PAL_SUMI_SCORCH);
    y += 32.0f;

    snprintf(buf, sizeof buf, "%d", g->run.kills);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_KILLS, buf, PAL_SUMI_SCORCH);
    y += 32.0f;

    snprintf(buf, sizeof buf, "%d", g->run.max_chain);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_MAX_CHAIN, buf, PAL_KIN);
    y += 38.0f;

    /* Reactions breakdown */
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

    /* Seed */
    snprintf(buf, sizeof buf, "0x%016" PRIx64, g->config.seed);
    draw_ledger_row(r, lx, y, lw, TEXT_RESULT_SEED, buf, PAL_SUMI_HEAVY);

    /* Vertical ledger divider */
    const Vector2 vdiv[2] = {{cx, 172.0f}, {cx, 540.0f}};
    ink_stroke(r, vdiv, 2,
               (BrushProfile){.w_max = 2.6f,
                              .w_min_ratio = 0.35f,
                              .entry = 0.1f,
                              .exit = 0.2f,
                              .dry = 0.3f,
                              .seed = 510u},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.72f}, 1.0f);

    /* Right column: Ink Crystal settlement ledger & owned upgrades */
    const float rx = cx + 44.0f;
    const float rw = 420.0f;
    float ry = 176.0f;
    render_draw_text_id(r, TEXT_RESULT_CRYSTALS, (Vector2){rx, ry}, 22.0f, PAL_SUMI_SCORCH, 0.96f,
                        false);
    ry += 34.0f;

    const MetaSettleBreakdown st =
        (ui != NULL && ui->settled) ? ui->settle : (MetaSettleBreakdown){0};
    snprintf(buf, sizeof buf, "+%d", st.from_acts);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_ACTS, buf, PAL_SUMI_DARK);
    ry += 28.0f;
    snprintf(buf, sizeof buf, "+%d", st.from_kills);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_KILLS, buf, PAL_SUMI_DARK);
    ry += 28.0f;
    snprintf(buf, sizeof buf, "+%d", st.from_chain);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_CHAIN, buf, PAL_SUMI_DARK);
    ry += 28.0f;
    snprintf(buf, sizeof buf, "+%d", st.from_reactions);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_REACTIONS, buf, PAL_SUMI_DARK);
    ry += 32.0f;

    snprintf(buf, sizeof buf, "+%d", st.total_crystals);
    draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_TOTAL, buf, PAL_KIN);
    ry += 30.0f;
    if (ui != NULL && ui->settled) {
        snprintf(buf, sizeof buf, "%d", ui->crystals_after);
        draw_ledger_row(r, rx, ry, rw, TEXT_RESULT_CRYSTAL_OWNED, buf, PAL_SUMI_SCORCH);
    }
    ry += 42.0f;

    /* Owned upgrades list */
    render_draw_text_id(r, TEXT_RESULT_UPGRADES, (Vector2){rx, ry}, 20.0f, PAL_SUMI_DARK, 0.96f,
                        false);
    ry += 28.0f;
    const int32_t shown_upgs = g->run.upgrade_count < 6 ? g->run.upgrade_count : 6;
    for (int32_t i = 0; i < shown_upgs; ++i) {
        const OwnedUpgrade *ou = &g->run.upgrades[i];
        snprintf(buf, sizeof buf, "%s %s%u", text_get(text_upgrade_name(ou->id), r->use_ja),
                 text_get(TEXT_REWARD_LV, r->use_ja), (unsigned)ou->level);
        render_draw_text(r, buf, (Vector2){rx + 12.0f, ry}, 18.0f,
                         pal_axis_color(game_upgrade_axis(ou->id)), 0.94f, false);
        ry += 24.0f;
    }

    /* Retry prompt */
    render_draw_text_centered(r, text_get(TEXT_RESULT_RETRY, r->use_ja),
                              (Vector2){cx, scroll.y + scroll.height - 44.0f}, 24.0f, PAL_SHU,
                              0.96f, false);
}
