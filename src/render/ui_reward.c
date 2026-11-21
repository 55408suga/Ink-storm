#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <stdio.h>
#include <string.h>

void render_reward_layout(const GameState *g, RewardLayout *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (g == NULL) {
        return;
    }

    const RewardOffer *rw = &g->run.reward;
    int32_t count = rw->choice_count;
    if (count < 0) {
        count = 0;
    }
    if (count > GAME_MAX_REWARD_CHOICES) {
        count = GAME_MAX_REWARD_CHOICES;
    }
    if (rw->kind == REWARD_KIND_FALLBACK) {
        count = 0;
    }

    const float card_w = 264.0f;
    const float card_h = 356.0f;
    const float gap = 36.0f;
    const float total_w = count > 0 ? ((float)count * card_w + (float)(count - 1) * gap) : 0.0f;
    const float start_x = ((float)INK_SCREEN_W - total_w) * 0.5f;
    const float card_y = 148.0f;

    out->card_count = count;
    for (int32_t i = 0; i < count; ++i) {
        out->cards[i] = (Rectangle){start_x + (float)i * (card_w + gap), card_y, card_w, card_h};
    }

    if (rw->kind == REWARD_KIND_FALLBACK) {
        out->has_repair = true;
        out->repair_rect =
            (Rectangle){((float)INK_SCREEN_W - 280.0f) * 0.5f, 270.0f, 280.0f, 112.0f};
        out->has_reroll = false;
    } else {
        out->has_repair = true;
        out->has_reroll = true;
        out->reroll_rect = (Rectangle){(float)INK_SCREEN_W * 0.5f - 260.0f, 544.0f, 236.0f, 68.0f};
        out->repair_rect = (Rectangle){(float)INK_SCREEN_W * 0.5f + 24.0f, 544.0f, 236.0f, 68.0f};
    }
}

static void draw_axis_emblem(RenderState *r, Vector2 center, UpgradeAxis axis)
{
    const PaletteId pal = pal_axis_color(axis);
    ink_wash(r, center, 22.0f, 22.0f, 0.2f, (InkTone){.color = pal, .alpha = 0.26f}, 2,
             410u + (uint32_t)axis);
    if (axis == AXIS_GALE) {
        ink_arc_stroke(r, center, 13.0f, 0.2f, 5.4f,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.3f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.2f,
                                      .seed = 415u},
                       (InkTone){.color = pal, .alpha = 0.90f});
    } else {
        ink_teardrop(r, center, (Vector2){0.0f, -1.0f}, 14.0f, 11.0f, pal_color_alpha(pal, 0.92f));
    }
}

static void draw_mini_shrine_icon(RenderState *r, Vector2 c, bool enabled)
{
    const PaletteId ink = enabled ? PAL_SUMI_DARK : PAL_SUMI_LIGHT;
    const Vector2 roof[3] = {
        {c.x - 16.0f, c.y - 3.0f}, {c.x, c.y - 13.0f}, {c.x + 16.0f, c.y - 3.0f}};
    ink_stroke(r, roof, 3,
               (BrushProfile){.w_max = 3.6f,
                              .w_min_ratio = 0.3f,
                              .entry = 0.15f,
                              .exit = 0.2f,
                              .dry = 0.15f,
                              .seed = 440u},
               (InkTone){.color = ink, .alpha = 0.95f}, 1.0f);
    ink_teardrop(r, (Vector2){c.x, c.y + 4.0f}, (Vector2){0.0f, -1.0f}, 7.0f, 4.5f,
                 pal_color_alpha(enabled ? PAL_SHU : PAL_SUMI_CLEAR, 0.92f));
}

void render_draw_reward_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || g->phase != RUN_PHASE_REWARD) {
        return;
    }

    const RewardOffer *rw = &g->run.reward;
    RewardLayout layout;
    render_reward_layout(g, &layout);

    /* Thin washi veil over paused combat scene */
    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(pal_paper_for_act(g->run.act_index), 0.72f));

    const TextId heading =
        rw->kind == REWARD_KIND_FALLBACK ? TEXT_REWARD_FALLBACK_TITLE : TEXT_REWARD_TITLE;
    render_draw_text_centered(r, text_get(heading, r->use_ja),
                              (Vector2){(float)INK_SCREEN_W * 0.5f, 82.0f}, 36.0f, PAL_SUMI_SCORCH,
                              0.96f, true);
    render_draw_text_centered(r, text_get(TEXT_REWARD_HINT, r->use_ja),
                              (Vector2){(float)INK_SCREEN_W * 0.5f, 118.0f}, 18.0f, PAL_SUMI_HEAVY,
                              0.88f, false);

    const float anim = r->reward_reroll_t > 0.0f
                           ? (1.0f - r->reward_reroll_t / 0.40f)
                           : (r->reward_anim_t > 0.05f ? r->reward_anim_t : 1.0f);
    const int32_t hover = ui != NULL ? ui->hover_choice : UI_HOVER_NONE;

    for (int32_t i = 0; i < layout.card_count; ++i) {
        Rectangle card = layout.cards[i];
        const bool is_hover = (hover == i);
        if (is_hover) {
            card.y -= 8.0f;
        }
        const float h_vis = card.height * ink_clampf(anim, 0.25f, 1.0f);
        const Rectangle vis = {card.x, card.y, card.width, h_vis};

        const UpgradeId uid = (UpgradeId)rw->choice_ids[i];
        const UpgradeAxis axis = game_upgrade_axis(uid);
        const PaletteId border = is_hover ? pal_axis_color(axis) : PAL_SUMI_DARK;
        ink_fill_panel(r, vis, PAL_GOFUN, 0.95f, border, 0.94f, 450u + (uint32_t)i);

        const float cx = vis.x + vis.width * 0.5f;
        draw_axis_emblem(r, (Vector2){cx, vis.y + 42.0f}, axis);

        /* Key number & axis name */
        char hdr[64];
        snprintf(hdr, sizeof hdr, "[%d] %s", i + 1, text_get(text_axis_name(axis), r->use_ja));
        render_draw_text_centered(r, hdr, (Vector2){cx, vis.y + 82.0f}, 18.0f, pal_axis_color(axis),
                                  0.94f, false);

        /* Upgrade name */
        const char *name = text_get(text_upgrade_name(uid), r->use_ja);
        render_draw_text_centered(r, name, (Vector2){cx, vis.y + 124.0f}, 26.0f, PAL_SUMI_SCORCH,
                                  0.98f, false);

        /* Current Lv -> Next Lv */
        const uint8_t cur_lv = game_upgrade_level(g, uid);
        const uint8_t next_lv = rw->choice_next_level[i] > 0 ? rw->choice_next_level[i] : 1;
        char lv_buf[48];
        snprintf(lv_buf, sizeof lv_buf, "%s %u -> %u", text_get(TEXT_REWARD_LV, r->use_ja),
                 (unsigned)cur_lv, (unsigned)next_lv);
        render_draw_text_centered(r, lv_buf, (Vector2){cx, vis.y + 168.0f}, 20.0f, PAL_SUMI_HEAVY,
                                  0.92f, false);

        /* Divider brush line */
        const Vector2 div_pts[2] = {{vis.x + 28.0f, vis.y + 196.0f},
                                    {vis.x + vis.width - 28.0f, vis.y + 196.0f}};
        ink_stroke(r, div_pts, 2,
                   (BrushProfile){.w_max = 2.4f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.15f,
                                  .exit = 0.25f,
                                  .dry = 0.3f,
                                  .seed = 460u + (uint32_t)i},
                   (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.70f}, 1.0f);

        /* Effect description */
        const char *eff = text_get(text_upgrade_effect(uid, (int)next_lv), r->use_ja);
        render_draw_text_centered(r, eff, (Vector2){cx, vis.y + 248.0f}, 18.0f, PAL_SUMI_DARK,
                                  0.94f, false);

        if (is_hover) {
            seal_stamp(r, TEXT_SEAL_DONE,
                       (Vector2){vis.x + vis.width - 32.0f, vis.y + vis.height - 32.0f}, 34.0f,
                       SEAL_SHUBUN, 0.5f);
        }
    }

    if (layout.has_reroll) {
        const bool can_reroll = rw->kind == REWARD_KIND_UPGRADE && g->run.rerolls_left > 0;
        const bool hov = hover == UI_HOVER_REROLL && can_reroll;
        ink_fill_panel(r, layout.reroll_rect, can_reroll ? PAL_GOFUN : PAL_SUMI_CLEAR,
                       can_reroll ? 0.94f : 0.45f,
                       hov ? PAL_SHU : (can_reroll ? PAL_SUMI_DARK : PAL_SUMI_LIGHT), 0.90f, 480u);
        char rb[64];
        snprintf(rb, sizeof rb, "%s (%d)", text_get(TEXT_REWARD_REROLL, r->use_ja),
                 g->run.rerolls_left);
        render_draw_text_centered(
            r, rb,
            (Vector2){layout.reroll_rect.x + layout.reroll_rect.width * 0.5f,
                      layout.reroll_rect.y + layout.reroll_rect.height * 0.5f},
            20.0f, can_reroll ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, 0.95f, false);
    }

    if (layout.has_repair) {
        const bool can_rep = rw->repair_available;
        const bool hov = hover == UI_HOVER_REPAIR && can_rep;
        ink_fill_panel(r, layout.repair_rect, can_rep ? PAL_GOFUN : PAL_SUMI_CLEAR,
                       can_rep ? 0.94f : 0.45f,
                       hov ? PAL_SHU : (can_rep ? PAL_SUMI_DARK : PAL_SUMI_LIGHT), 0.90f, 481u);
        draw_mini_shrine_icon(r,
                              (Vector2){layout.repair_rect.x + 30.0f,
                                        layout.repair_rect.y + layout.repair_rect.height * 0.5f},
                              can_rep);
        render_draw_text_centered(
            r, text_get(TEXT_REWARD_REPAIR, r->use_ja),
            (Vector2){layout.repair_rect.x + layout.repair_rect.width * 0.5f + 14.0f,
                      layout.repair_rect.y + layout.repair_rect.height * 0.5f},
            19.0f, can_rep ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, 0.95f, false);
    }
}
