#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <inttypes.h>
#include <math.h>
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
        out->endless_rect = (Rectangle){x0, 582.0f, bw, 54.0f};
        out->lobby_rect = (Rectangle){x0 + bw + gap, 582.0f, bw, 54.0f};
        out->retry_rect = (Rectangle){x0 + (bw + gap) * 2.0f, 582.0f, bw, 54.0f};
    } else {
        out->has_endless = false;
        out->lobby_rect = (Rectangle){cx - 260.0f, 582.0f, 236.0f, 54.0f};
        out->retry_rect = (Rectangle){cx + 24.0f, 582.0f, 236.0f, 54.0f};
    }
}

static void draw_flat_rect(RenderState *r, Rectangle rc, Color c)
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

static void draw_section_rule(RenderState *r, float x0, float x1, float y, uint32_t seed)
{
    const Vector2 pts[2] = {{x0, y}, {x1, y}};
    ink_stroke(r, pts, 2,
               (BrushProfile){.w_max = 2.0f,
                              .w_min_ratio = 0.30f,
                              .entry = 0.10f,
                              .exit = 0.25f,
                              .dry = 0.35f,
                              .seed = seed},
               (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.72f}, 1.0f);
}

static void draw_number_centered(const RenderState *r, const char *digits, Vector2 center,
                                 float size, PaletteId pal, float alpha)
{
    const float w = render_measure_number_fixed(r, digits, size);
    render_draw_number_fixed(r, digits, (Vector2){center.x - w * 0.5f, center.y - size * 0.5f},
                             size, pal, alpha);
}

static void format_stage_reached(const RenderState *r, const GameState *g, char *out, size_t cap)
{
    const char *prefix = text_get(TEXT_RESULT_REACHED, r->use_ja);
    const int32_t stage_act = g->run.act_index >= 0 ? (g->run.act_index % GAME_MAIN_ACT_COUNT) : 0;
    const bool is_boss = g->run.wave_index >= GAME_BOSS_WAVE_INDEX;

    if (g->run.act_index >= GAME_MAIN_ACT_COUNT) {
        const int32_t loop_n = g->run.act_index / GAME_MAIN_ACT_COUNT;
        if (r->use_ja) {
            if (is_boss) {
                snprintf(out, cap, "%s：%s%d%s %s（%s%d）", prefix,
                         text_get(TEXT_ENDLESS_ACT_PREFIX, true), g->run.act_index + 1,
                         text_get(TEXT_ENDLESS_ACT_SUFFIX, true), text_get(TEXT_HUD_BOSS, true),
                         text_get(TEXT_ENDLESS_LOOP_LABEL, true), loop_n);
            } else {
                snprintf(out, cap, "%s：%s%d%s %s %d/3（%s%d）", prefix,
                         text_get(TEXT_ENDLESS_ACT_PREFIX, true), g->run.act_index + 1,
                         text_get(TEXT_ENDLESS_ACT_SUFFIX, true), text_get(TEXT_HUD_WAVE, true),
                         g->run.wave_index + 1, text_get(TEXT_ENDLESS_LOOP_LABEL, true), loop_n);
            }
        } else {
            if (is_boss) {
                snprintf(out, cap, "%s: Act %d %s (Loop %d)", prefix, g->run.act_index + 1,
                         text_get(TEXT_HUD_BOSS, false), loop_n);
            } else {
                snprintf(out, cap, "%s: Act %d %s %d/3 (Loop %d)", prefix, g->run.act_index + 1,
                         text_get(TEXT_HUD_WAVE, false), g->run.wave_index + 1, loop_n);
            }
        }
        return;
    }

    const TextId short_id = text_act_short(stage_act);
    const TextId sub_id = text_act_subtitle(stage_act);
    const char *act_s = text_get(short_id != TEXT_NONE ? short_id : TEXT_ACT1_SHORT, r->use_ja);
    const char *sub_s = text_get(sub_id != TEXT_NONE ? sub_id : TEXT_ACT1_SUBTITLE, r->use_ja);
    if (r->use_ja) {
        if (is_boss) {
            snprintf(out, cap, "%s：%s・%s（%s）", prefix, act_s, sub_s,
                     text_get(TEXT_HUD_BOSS, true));
        } else {
            snprintf(out, cap, "%s：%s・%s %s %d/3", prefix, act_s, sub_s,
                     text_get(TEXT_HUD_WAVE, true), g->run.wave_index + 1);
        }
    } else {
        if (is_boss) {
            snprintf(out, cap, "%s: %s - %s (%s)", prefix, act_s, sub_s,
                     text_get(TEXT_HUD_BOSS, false));
        } else {
            snprintf(out, cap, "%s: %s - %s %s %d/3", prefix, act_s, sub_s,
                     text_get(TEXT_HUD_WAVE, false), g->run.wave_index + 1);
        }
    }
}

static void draw_kpi_row(RenderState *r, const GameState *g, const UiView *ui,
                         const MetaSettleBreakdown *st, bool has_breakdown)
{
    const float y = 154.0f;
    const float w = 240.0f;
    const float h = 84.0f;
    const float gap = 16.0f;
    const float x0 = 136.0f;
    char buf[64];

    /* Card 1: Score */
    const Rectangle c0 = {x0, y, w, h};
    ink_fill_panel(r, c0, PAL_PAPER_ACT1, 0.96f, PAL_SUMI_DARK, 0.88f, 512u);
    render_draw_text_centered(r, text_get(TEXT_RESULT_SCORE, r->use_ja),
                              (Vector2){c0.x + w * 0.5f, c0.y + 20.0f}, 15.0f, PAL_SUMI_HEAVY,
                              0.95f, false);
    snprintf(buf, sizeof buf, "%d", g->run.score);
    draw_number_centered(r, buf, (Vector2){c0.x + w * 0.5f, c0.y + 54.0f}, 30.0f, PAL_SUMI_SCORCH,
                         0.98f);

    /* Card 2: Kills */
    const Rectangle c1 = {x0 + (w + gap), y, w, h};
    ink_fill_panel(r, c1, PAL_PAPER_ACT1, 0.96f, PAL_SUMI_DARK, 0.88f, 513u);
    render_draw_text_centered(r, text_get(TEXT_RESULT_KILLS, r->use_ja),
                              (Vector2){c1.x + w * 0.5f, c1.y + 20.0f}, 15.0f, PAL_SUMI_HEAVY,
                              0.95f, false);
    snprintf(buf, sizeof buf, "%d", g->run.kills);
    draw_number_centered(r, buf, (Vector2){c1.x + w * 0.5f, c1.y + 54.0f}, 30.0f, PAL_SUMI_SCORCH,
                         0.98f);

    /* Card 3: Max Chain */
    const Rectangle c2 = {x0 + (w + gap) * 2.0f, y, w, h};
    ink_fill_panel(r, c2, PAL_PAPER_ACT2, 0.96f, PAL_BENGARA, 0.92f, 514u);
    render_draw_text_centered(r, text_get(TEXT_RESULT_MAX_CHAIN, r->use_ja),
                              (Vector2){c2.x + w * 0.5f, c2.y + 20.0f}, 15.0f, PAL_BENGARA, 0.96f,
                              false);
    if (r->use_ja) {
        snprintf(buf, sizeof buf, "×%d", g->run.max_chain);
    } else {
        snprintf(buf, sizeof buf, "x%d", g->run.max_chain);
    }
    render_draw_text_centered(r, buf, (Vector2){c2.x + w * 0.5f, c2.y + 54.0f}, 30.0f, PAL_BENGARA,
                              0.98f, false);

    /* Card 4: Crystals (Hero Highlight on dark lacquer) */
    const Rectangle c3 = {x0 + (w + gap) * 3.0f, y, w, h};
    ink_fill_panel(r, c3, PAL_SUMI_SCORCH, 0.96f, PAL_KIN, 0.96f, 515u);
    const TextId cry_hdr = (ui != NULL && ui->settle_preview && !ui->settled)
                               ? TEXT_RESULT_CRYSTALS_PREVIEW_SHORT
                               : TEXT_RESULT_CRYSTALS;
    render_draw_text_fit(r, text_get(cry_hdr, r->use_ja), (Vector2){c3.x + w * 0.5f, c3.y + 16.0f},
                         w - 20.0f, 14.0f, 11.0f, PAL_GOFUN, 0.94f, false);
    snprintf(buf, sizeof buf, "+%d", st->total_crystals);
    draw_number_centered(r, buf, (Vector2){c3.x + w * 0.5f, c3.y + 44.0f}, 30.0f, PAL_KIN_HOT,
                         0.99f);
    if (has_breakdown) {
        snprintf(buf, sizeof buf, "%s %d", text_get(TEXT_RESULT_CRYSTAL_OWNED, r->use_ja),
                 ui->crystals_after);
        render_draw_text_fit(r, buf, (Vector2){c3.x + w * 0.5f, c3.y + 70.0f}, w - 20.0f, 14.0f,
                             11.0f, PAL_GOFUN, 0.90f, false);
    }
}

static void draw_reaction_tile(RenderState *r, Rectangle rc, TextId label, int32_t count,
                               PaletteId accent_top, PaletteId accent_bot, PaletteId label_pal,
                               bool dark, uint32_t seed)
{
    ink_fill_panel(r, rc, dark ? PAL_SUMI_SCORCH : PAL_GOFUN, 0.96f,
                   dark ? PAL_KIN : PAL_SUMI_LIGHT, dark ? 0.96f : 0.80f, seed);
    const float strip_w = 6.0f;
    if (accent_top == accent_bot) {
        draw_flat_rect(r, (Rectangle){rc.x + 2.0f, rc.y + 2.0f, strip_w, rc.height - 4.0f},
                       pal_color_alpha(accent_top, 0.95f));
    } else {
        const float half = (rc.height - 4.0f) * 0.5f;
        draw_flat_rect(r, (Rectangle){rc.x + 2.0f, rc.y + 2.0f, strip_w, half},
                       pal_color_alpha(accent_top, 0.95f));
        draw_flat_rect(r, (Rectangle){rc.x + 2.0f, rc.y + 2.0f + half, strip_w, half},
                       pal_color_alpha(accent_bot, 0.95f));
    }

    char cnt_buf[24];
    snprintf(cnt_buf, sizeof cnt_buf, "%d", count);
    const float cw = render_measure_number_fixed(r, cnt_buf, 24.0f);
    render_draw_number_fixed(
        r, cnt_buf, (Vector2){rc.x + rc.width - 14.0f - cw, rc.y + (rc.height - 24.0f) * 0.5f},
        24.0f, dark ? PAL_GOFUN : PAL_SUMI_SCORCH, 0.98f);

    const float label_x = rc.x + 16.0f;
    const float label_max_w = fmaxf(40.0f, rc.width - 36.0f - cw);
    render_draw_text_wrapped(r, text_get(label, r->use_ja),
                             (Rectangle){label_x, rc.y + 8.0f, label_max_w, rc.height - 16.0f},
                             17.0f, 12.0f, label_pal, 0.97f, false, false);
}

static void draw_breakdown_chip(RenderState *r, Rectangle rc, TextId label, int32_t value,
                                PaletteId border_pal, PaletteId val_pal, uint32_t seed)
{
    ink_fill_panel(r, rc, PAL_GOFUN, 0.96f, border_pal, 0.84f, seed);
    char val_buf[24];
    snprintf(val_buf, sizeof val_buf, "+%d", value);
    const float vw = render_measure_number_fixed(r, val_buf, 15.0f);
    render_draw_number_fixed(
        r, val_buf, (Vector2){rc.x + rc.width - 10.0f - vw, rc.y + (rc.height - 15.0f) * 0.5f},
        15.0f, val_pal, 0.97f);
    const float lbl_w = fmaxf(28.0f, rc.width - 22.0f - vw);
    render_draw_text_wrapped(r, text_get(label, r->use_ja),
                             (Rectangle){rc.x + 9.0f, rc.y + 4.0f, lbl_w, rc.height - 8.0f}, 14.0f,
                             11.0f, PAL_SUMI_DARK, 0.94f, false, false);
}

static void draw_left_panel(RenderState *r, const GameState *g, const MetaSettleBreakdown *st)
{
    const Rectangle panel = {136.0f, 256.0f, 490.0f, 282.0f};
    ink_fill_panel(r, panel, PAL_PAPER_ACT1, 0.92f, PAL_SUMI_LIGHT, 0.85f, 520u);

    /* Section 1: Elemental Reactions (2x2 tiles) */
    render_draw_text_id(r, TEXT_RESULT_REACTIONS_HDR, (Vector2){154.0f, 265.0f}, 16.0f,
                        PAL_SUMI_DARK, 0.96f, false);
    draw_section_rule(r, 154.0f, 608.0f, 287.0f, 521u);

    const float tw = 220.0f;
    const float th = 48.0f;
    const float tgx = 14.0f;
    const float tgy = 10.0f;
    const float tx0 = 154.0f;
    const float ty0 = 295.0f;
    draw_reaction_tile(r, (Rectangle){tx0, ty0, tw, th}, TEXT_RESULT_BLAZING_LABEL,
                       g->run.ignitions, PAL_SHU, PAL_SHU, PAL_BENGARA, false, 522u);
    draw_reaction_tile(r, (Rectangle){tx0 + tw + tgx, ty0, tw, th}, TEXT_RESULT_TEMPEST_LABEL,
                       g->run.storm_zaps, PAL_AO, PAL_AO, PAL_AO, false, 523u);
    draw_reaction_tile(r, (Rectangle){tx0, ty0 + th + tgy, tw, th}, TEXT_RESULT_THUNDERBURST_LABEL,
                       g->run.thunderbursts, PAL_SHU, PAL_AO, PAL_SHU, false, 524u);
    draw_reaction_tile(r, (Rectangle){tx0 + tw + tgx, ty0 + th + tgy, tw, th},
                       TEXT_RESULT_CATACLYSM_LABEL, g->run.cataclysms, PAL_KIN, PAL_KIN,
                       PAL_KIN_HOT, true, 525u);

    /* Section 2: Crystal Breakdown (3x2 micro-chip grid) */
    render_draw_text_id(r, TEXT_RESULT_BREAKDOWN_HDR, (Vector2){154.0f, 413.0f}, 16.0f,
                        PAL_SUMI_DARK, 0.96f, false);
    draw_section_rule(r, 154.0f, 608.0f, 435.0f, 526u);

    typedef struct {
        TextId label;
        int32_t value;
        PaletteId border;
        PaletteId val_pal;
    } BreakdownChip;

    BreakdownChip chips[6];
    int32_t chip_n = 0;
    chips[chip_n++] =
        (BreakdownChip){TEXT_RESULT_CRYSTAL_ACTS, st->from_acts, PAL_SUMI_LIGHT, PAL_SUMI_SCORCH};
    chips[chip_n++] =
        (BreakdownChip){TEXT_RESULT_CRYSTAL_KILLS, st->from_kills, PAL_SUMI_LIGHT, PAL_SUMI_SCORCH};
    chips[chip_n++] =
        (BreakdownChip){TEXT_RESULT_CRYSTAL_CHAIN, st->from_chain, PAL_SUMI_LIGHT, PAL_SUMI_SCORCH};
    chips[chip_n++] = (BreakdownChip){TEXT_RESULT_CRYSTAL_REACTIONS, st->from_reactions,
                                      PAL_SUMI_LIGHT, PAL_SUMI_SCORCH};
    if (st->from_endless > 0) {
        chips[chip_n++] = (BreakdownChip){TEXT_RESULT_CRYSTAL_ENDLESS, st->from_endless,
                                          PAL_BENGARA, PAL_BENGARA};
    }
    if (st->from_bonus > 0) {
        chips[chip_n++] =
            (BreakdownChip){TEXT_RESULT_CRYSTAL_BONUS_SHORT, st->from_bonus, PAL_SHU, PAL_SHU};
    }

    const float cw = 146.0f;
    const float ch = 36.0f;
    const float cgx = 8.0f;
    const float cgy = 8.0f;
    const float cx0 = 154.0f;
    const float cy0 = 444.0f;
    for (int32_t i = 0; i < chip_n; ++i) {
        const int32_t col = i % 3;
        const int32_t row = i / 3;
        const Rectangle rc = {cx0 + (float)col * (cw + cgx), cy0 + (float)row * (ch + cgy), cw, ch};
        draw_breakdown_chip(r, rc, chips[i].label, chips[i].value, chips[i].border,
                            chips[i].val_pal, 530u + (uint32_t)i);
    }
}

static void draw_right_panel(RenderState *r, const GameState *g)
{
    const Rectangle panel = {646.0f, 256.0f, 498.0f, 282.0f};
    ink_fill_panel(r, panel, PAL_PAPER_ACT1, 0.92f, PAL_SUMI_LIGHT, 0.85f, 540u);

    /* Section 1: Upgrades (2x4 framed pill chips) */
    render_draw_text_id(r, TEXT_RESULT_UPGRADES, (Vector2){664.0f, 265.0f}, 16.0f, PAL_SUMI_DARK,
                        0.96f, false);
    draw_section_rule(r, 664.0f, 1126.0f, 287.0f, 541u);

    const float uw = 224.0f;
    const float uh = 30.0f;
    const float ugx = 14.0f;
    const float ugy = 7.0f;
    const float ux0 = 664.0f;
    const float uy0 = 294.0f;

    if (g->run.upgrade_count <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){ux0 + 6.0f, uy0 + 6.0f}, 16.0f,
                            PAL_SUMI_HEAVY, 0.85f, false);
    } else {
        const bool has_overflow = g->run.upgrade_count > 8;
        const int32_t direct_n = has_overflow ? 7 : g->run.upgrade_count;
        char buf[96];
        for (int32_t i = 0; i < direct_n; ++i) {
            const OwnedUpgrade *ou = &g->run.upgrades[i];
            const int32_t col = i % 2;
            const int32_t row = i / 2;
            const Rectangle rc = {ux0 + (float)col * (uw + ugx), uy0 + (float)row * (uh + ugy), uw,
                                  uh};
            ink_fill_panel(r, rc, PAL_GOFUN, 0.96f, PAL_SUMI_LIGHT, 0.78f, 550u + (uint32_t)i);
            draw_flat_rect(r, (Rectangle){rc.x + 2.0f, rc.y + 2.0f, 6.0f, rc.height - 4.0f},
                           pal_color_alpha(pal_axis_color(game_upgrade_axis(ou->id)), 0.95f));
            snprintf(buf, sizeof buf, "%s %s%u", text_get(text_upgrade_name(ou->id), r->use_ja),
                     text_get(TEXT_REWARD_LV, r->use_ja), (unsigned)ou->level);
            render_draw_text_wrapped(
                r, buf, (Rectangle){rc.x + 14.0f, rc.y + 3.0f, rc.width - 20.0f, rc.height - 6.0f},
                15.0f, 11.0f, PAL_SUMI_SCORCH, 0.97f, false, false);
        }
        if (has_overflow) {
            const int32_t extra = g->run.upgrade_count - 7;
            const Rectangle rc = {ux0 + (uw + ugx), uy0 + 3.0f * (uh + ugy), uw, uh};
            ink_fill_panel(r, rc, PAL_PAPER_ACT2, 0.96f, PAL_SUMI_DARK, 0.85f, 558u);
            if (r->use_ja) {
                snprintf(buf, sizeof buf, "%s +%d", text_get(TEXT_RESULT_MORE_ITEMS, true), extra);
            } else {
                snprintf(buf, sizeof buf, "+%d %s", extra, text_get(TEXT_RESULT_MORE_ITEMS, false));
            }
            render_draw_text_fit(r, buf, (Vector2){rc.x + rc.width * 0.5f, rc.y + rc.height * 0.5f},
                                 rc.width - 18.0f, 15.0f, 11.0f, PAL_SUMI_SCORCH, 0.96f, false);
        }
    }

    /* Section 2: Great Relics (2x2 gold-accented chips) */
    render_draw_text_id(r, TEXT_RESULT_RELICS, (Vector2){664.0f, 444.0f}, 16.0f, PAL_SUMI_DARK,
                        0.96f, false);
    draw_section_rule(r, 664.0f, 1126.0f, 465.0f, 560u);

    const float rw = 224.0f;
    const float rh = 26.0f;
    const float rgx = 14.0f;
    const float rgy = 6.0f;
    const float rx0 = 664.0f;
    const float ry0 = 472.0f;

    if (g->run.relic_count <= 0) {
        render_draw_text_id(r, TEXT_PAUSE_NONE, (Vector2){rx0 + 6.0f, ry0 + 4.0f}, 16.0f,
                            PAL_SUMI_HEAVY, 0.85f, false);
    } else {
        const bool has_overflow = g->run.relic_count > 4;
        const int32_t direct_n = has_overflow ? 3 : g->run.relic_count;
        for (int32_t i = 0; i < direct_n; ++i) {
            const int32_t col = i % 2;
            const int32_t row = i / 2;
            const Rectangle rc = {rx0 + (float)col * (rw + rgx), ry0 + (float)row * (rh + rgy), rw,
                                  rh};
            ink_fill_panel(r, rc, PAL_GOFUN, 0.96f, PAL_KIN, 0.92f, 562u + (uint32_t)i);
            const Vector2 gem = {rc.x + 14.0f, rc.y + rc.height * 0.5f};
            ink_teardrop(r, gem, (Vector2){0.0f, -1.0f}, 7.0f, 5.4f,
                         pal_color_alpha(PAL_KIN, 0.96f));
            ink_teardrop(r, gem, (Vector2){0.0f, -1.0f}, 3.6f, 2.8f,
                         pal_color_alpha(PAL_KIN_HOT, 0.98f));
            render_draw_text_wrapped(
                r, text_get(text_relic_name(g->run.relics[i]), r->use_ja),
                (Rectangle){rc.x + 26.0f, rc.y + 2.0f, rc.width - 32.0f, rc.height - 4.0f}, 15.0f,
                11.0f, PAL_SUMI_SCORCH, 0.97f, false, false);
        }
        if (has_overflow) {
            char buf[32];
            const int32_t extra = g->run.relic_count - 3;
            const Rectangle rc = {rx0 + (rw + rgx), ry0 + (rh + rgy), rw, rh};
            ink_fill_panel(r, rc, PAL_PAPER_ACT2, 0.96f, PAL_KIN, 0.90f, 566u);
            if (r->use_ja) {
                snprintf(buf, sizeof buf, "%s +%d", text_get(TEXT_RESULT_MORE_ITEMS, true), extra);
            } else {
                snprintf(buf, sizeof buf, "+%d %s", extra, text_get(TEXT_RESULT_MORE_ITEMS, false));
            }
            render_draw_text_fit(r, buf, (Vector2){rc.x + rc.width * 0.5f, rc.y + rc.height * 0.5f},
                                 rc.width - 18.0f, 14.0f, 11.0f, PAL_SUMI_SCORCH, 0.96f, false);
        }
    }
}

static void draw_result_button(RenderState *r, Rectangle rect, TextId label, bool focused,
                               PaletteId idle_border, uint32_t seed)
{
    ink_fill_panel(r, rect, focused ? PAL_PAPER_ACT1 : PAL_GOFUN, 0.98f,
                   focused ? PAL_SHU : idle_border, 0.94f, seed);
    if (focused) {
        ink_teardrop(r, (Vector2){rect.x + 22.0f, rect.y + rect.height * 0.5f},
                     (Vector2){1.0f, 0.0f}, 9.0f, 6.0f, pal_color_alpha(PAL_SHU, 0.92f));
    }
    render_draw_text_fit(r, text_get(label, r->use_ja),
                         (Vector2){rect.x + rect.width * 0.5f + (focused ? 8.0f : 0.0f),
                                   rect.y + rect.height * 0.5f},
                         rect.width - 52.0f, 20.0f, 14.0f, focused ? PAL_SHU : PAL_SUMI_SCORCH,
                         0.98f, false);
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

    /* The scroll unrolls from the center and every card is anchored to the fully open sheet,
     * clipped to the visible paper, so content never shifts mid-unfurl. */
    const float scroll_full_w = 1080.0f;
    const float scroll_h = 616.0f;
    const float scroll_y = 52.0f;
    const float scroll_w = scroll_full_w * open;
    const float scroll_x = ((float)INK_SCREEN_W - scroll_w) * 0.5f;
    const Rectangle scroll = {scroll_x, scroll_y, scroll_w, scroll_h};
    const Rectangle sheet = {((float)INK_SCREEN_W - scroll_full_w) * 0.5f, scroll_y, scroll_full_w,
                             scroll_h};
    ink_fill_panel(r, scroll, PAL_GOFUN, 0.97f, PAL_SUMI_DARK, 0.92f, 500u);

    const Rectangle left_rod = {scroll.x - 14.0f, scroll.y - 10.0f, 16.0f, scroll.height + 20.0f};
    const Rectangle right_rod = {scroll.x + scroll.width - 2.0f, scroll.y - 10.0f, 16.0f,
                                 scroll.height + 20.0f};
    ink_fill_panel(r, left_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 501u);
    ink_fill_panel(r, right_rod, PAL_SUMI_HEAVY, 0.96f, PAL_SUMI_SCORCH, 0.92f, 502u);

    if (open < 0.75f) {
        return;
    }
    render_reset_blend(r);
    BeginScissorMode((int)scroll.x, (int)scroll.y, (int)scroll.width, (int)scroll.height);

    const bool win = (g->phase == RUN_PHASE_VICTORY);
    const float cx = (float)INK_SCREEN_W * 0.5f;

    /* Header: Brush title, Stage Reached pill, top-right Hanko seal */
    render_draw_text_centered(
        r, text_get(win ? TEXT_RESULT_VICTORY : TEXT_RESULT_DEFEAT, r->use_ja),
        (Vector2){cx, 90.0f}, 40.0f, win ? PAL_SUMI_SCORCH : PAL_BENGARA, 0.98f, true);

    char stage_buf[128];
    format_stage_reached(r, g, stage_buf, sizeof stage_buf);
    const float pill_w =
        ink_clampf(render_measure_text(r, stage_buf, 16.0f, false).x + 44.0f, 260.0f, 680.0f);
    const Rectangle stage_pill = {cx - pill_w * 0.5f, 114.0f, pill_w, 28.0f};
    ink_fill_panel(r, stage_pill, PAL_PAPER_ACT1, 0.96f, PAL_SUMI_DARK, 0.86f, 505u);
    render_draw_text_fit(r, stage_buf, (Vector2){cx, stage_pill.y + stage_pill.height * 0.5f},
                         stage_pill.width - 24.0f, 16.0f, 12.0f, PAL_SUMI_SCORCH, 0.97f, false);

    seal_stamp(r, win ? TEXT_SEAL_SLAY : TEXT_SEAL_PURIFY,
               (Vector2){sheet.x + sheet.width - 66.0f, 100.0f}, 54.0f, SEAL_HAKUBUN, open);

    /* Row 1: 4 Hero KPI Cards */
    const bool has_breakdown = (ui != NULL && (ui->settled || ui->settle_preview));
    const MetaSettleBreakdown st = has_breakdown ? ui->settle : (MetaSettleBreakdown){0};
    draw_kpi_row(r, g, ui, &st, has_breakdown);

    /* Row 2: Structured Left & Right Panels */
    draw_left_panel(r, g, &st);
    draw_right_panel(r, g);

    /* Footer: subtle seed line + action buttons */
    char seed_buf[64];
    snprintf(seed_buf, sizeof seed_buf, "%s: 0x%016" PRIx64, text_get(TEXT_RESULT_SEED, r->use_ja),
             g->config.seed);
    render_draw_text(r, seed_buf, (Vector2){136.0f, 548.0f}, 14.0f, PAL_SUMI_HEAVY, 0.90f, false);

    const bool show_endless =
        (g->phase == RUN_PHASE_VICTORY && !g->run.run_settled && g->run.endless_loop == 0);
    ResultLayout layout;
    render_result_layout(show_endless, &layout);
    int32_t hov = UI_HOVER_NONE;
    if (ui != NULL) {
        hov = ui->hover_choice >= 0 ? ui->hover_choice : ui->result_cursor;
    }
    if (!layout.has_endless && hov == 2) {
        hov = 0;
    }

    if (layout.has_endless) {
        draw_result_button(r, layout.endless_rect, TEXT_RESULT_ENDLESS, hov == 2, PAL_BENGARA,
                           570u);
    }
    draw_result_button(r, layout.lobby_rect, TEXT_RESULT_TO_LOBBY, hov == 0, PAL_SUMI_DARK, 571u);
    draw_result_button(r, layout.retry_rect, TEXT_RESULT_PLAY_AGAIN, hov == 1, PAL_SUMI_DARK, 572u);

    render_reset_blend(r);
    EndScissorMode();
}
