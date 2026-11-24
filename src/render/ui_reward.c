#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
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

    const float card_w = 276.0f;
    const float card_h = 356.0f;
    const float gap = 32.0f;
    const float total_w = count > 0 ? ((float)count * card_w + (float)(count - 1) * gap) : 0.0f;
    const float start_x = ((float)INK_SCREEN_W - total_w) * 0.5f;
    const float card_y = 152.0f;

    out->card_count = count;
    for (int32_t i = 0; i < count; ++i) {
        out->cards[i] = (Rectangle){start_x + (float)i * (card_w + gap), card_y, card_w, card_h};
    }

    if (rw->kind == REWARD_KIND_FALLBACK) {
        out->has_repair = true;
        out->has_reroll = false;
        if (rw->crystal_reward > 0) {
            out->has_crystal = true;
            const float cw = 320.0f;
            const float ch = 286.0f;
            const float cgap = 48.0f;
            const float cx = (float)INK_SCREEN_W * 0.5f;
            out->repair_rect = (Rectangle){cx - cw - cgap * 0.5f, 196.0f, cw, ch};
            out->crystal_rect = (Rectangle){cx + cgap * 0.5f, 196.0f, cw, ch};
        } else {
            out->has_crystal = false;
            out->repair_rect =
                (Rectangle){((float)INK_SCREEN_W - 280.0f) * 0.5f, 270.0f, 280.0f, 112.0f};
        }
    } else if (rw->kind == REWARD_KIND_RELIC) {
        out->has_repair = false;
        out->has_crystal = false;
        out->has_reroll = true;
        out->reroll_rect =
            (Rectangle){((float)INK_SCREEN_W - 236.0f) * 0.5f, 548.0f, 236.0f, 68.0f};
    } else {
        out->has_repair = true;
        out->has_crystal = false;
        out->has_reroll = true;
        out->reroll_rect = (Rectangle){(float)INK_SCREEN_W * 0.5f - 260.0f, 548.0f, 236.0f, 68.0f};
        out->repair_rect = (Rectangle){(float)INK_SCREEN_W * 0.5f + 24.0f, 548.0f, 236.0f, 68.0f};
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

static void draw_relic_emblem(RenderState *r, Vector2 c, RelicId rid)
{
    ink_wash(r, c, 34.0f, 28.0f, 0.12f, (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.42f}, 2,
             420u + (uint32_t)rid);
    switch (rid) {
    case RELIC_VERMILION_INKSTONE: {
        const Vector2 stone[4] = {{c.x - 18.0f, c.y - 12.0f},
                                  {c.x + 18.0f, c.y - 10.0f},
                                  {c.x + 15.0f, c.y + 14.0f},
                                  {c.x - 16.0f, c.y + 12.0f}};
        ink_outline(r, stone, 4, true, 3.2f, (Vector2){0.7f, 0.7f}, 430u);
        ink_wash(r, c, 10.0f, 7.0f, 0.0f, (InkTone){.color = PAL_SHU, .alpha = 0.85f}, 1, 431u);
        break;
    }
    case RELIC_THUNDER_DRUM:
        ink_arc_stroke(r, c, 16.0f, 0.0f, 6.2f,
                       (BrushProfile){.w_max = 3.6f,
                                      .w_min_ratio = 0.3f,
                                      .entry = 0.1f,
                                      .exit = 0.2f,
                                      .dry = 0.1f,
                                      .seed = 432u},
                       (InkTone){.color = PAL_AO, .alpha = 0.92f});
        ink_teardrop(r, c, (Vector2){0.0f, -1.0f}, 9.0f, 7.0f, pal_color_alpha(PAL_AO_HOT, 0.92f));
        break;
    case RELIC_TENGU_FAN:
        for (int k = -1; k <= 1; ++k) {
            const float a = -1.57f + (float)k * 0.48f;
            const Vector2 f[2] = {{c.x, c.y + 14.0f},
                                  {c.x + cosf(a) * 24.0f, c.y + 14.0f + sinf(a) * 24.0f}};
            ink_stroke(r, f, 2,
                       (BrushProfile){.w_max = 4.5f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.1f,
                                      .exit = 0.45f,
                                      .dry = 0.2f,
                                      .seed = 433u + (uint32_t)(k + 1)},
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f}, 1.0f);
        }
        break;
    case RELIC_PACT_OF_FIRE:
        ink_teardrop(r, c, (Vector2){0.0f, -1.0f}, 18.0f, 13.0f, pal_color_alpha(PAL_SHU, 0.92f));
        ink_teardrop(r, (Vector2){c.x, c.y + 2.0f}, (Vector2){0.0f, -1.0f}, 10.0f, 7.0f,
                     pal_color_alpha(PAL_SHU_HOT, 0.95f));
        break;
    case RELIC_SEAL_OF_TRINITY:
        /* Three-element crest ringed by gold arc */
        ink_arc_stroke(r, c, 19.0f, 0.15f, 5.75f,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.28f,
                                      .entry = 0.12f,
                                      .exit = 0.30f,
                                      .dry = 0.20f,
                                      .seed = 437u},
                       (InkTone){.color = PAL_KIN, .alpha = 0.94f});
        ink_teardrop(r, (Vector2){c.x, c.y - 8.5f}, (Vector2){0.0f, -1.0f}, 8.5f, 6.2f,
                     pal_color_alpha(PAL_SHU, 0.95f));
        ink_teardrop(r, (Vector2){c.x - 8.5f, c.y + 6.5f}, (Vector2){-0.7f, 0.7f}, 8.5f, 6.2f,
                     pal_color_alpha(PAL_AO, 0.95f));
        ink_teardrop(r, (Vector2){c.x + 8.5f, c.y + 6.5f}, (Vector2){0.7f, 0.7f}, 8.5f, 6.2f,
                     pal_color_alpha(PAL_SUMI_SCORCH, 0.95f));
        break;
    case RELIC_INKSTORM_SECRET:
        /* Deep swirling ink enso + gold core flick */
        ink_arc_stroke(r, c, 18.0f, 0.2f, 5.4f,
                       (BrushProfile){.w_max = 5.2f,
                                      .w_min_ratio = 0.22f,
                                      .entry = 0.14f,
                                      .exit = 0.35f,
                                      .dry = 0.35f,
                                      .seed = 438u},
                       (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f});
        ink_arc_stroke(r, c, 10.5f, 1.4f, 6.1f,
                       (BrushProfile){.w_max = 3.4f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.12f,
                                      .exit = 0.35f,
                                      .dry = 0.20f,
                                      .seed = 439u},
                       (InkTone){.color = PAL_KIN, .alpha = 0.95f});
        break;
    case RELIC_THUNDERFIRE_GEM: {
        /* Crimson-and-azure dual orb with lightning crack */
        ink_teardrop(r, (Vector2){c.x - 4.0f, c.y}, (Vector2){-0.2f, -1.0f}, 16.0f, 11.0f,
                     pal_color_alpha(PAL_SHU, 0.92f));
        ink_teardrop(r, (Vector2){c.x + 5.0f, c.y + 1.0f}, (Vector2){0.2f, -1.0f}, 15.0f, 10.5f,
                     pal_color_alpha(PAL_AO, 0.92f));
        const Vector2 zap[4] = {{c.x - 2.0f, c.y - 12.0f},
                                {c.x + 3.0f, c.y - 2.0f},
                                {c.x - 3.0f, c.y + 3.0f},
                                {c.x + 2.0f, c.y + 12.0f}};
        ink_stroke(r, zap, 4,
                   (BrushProfile){.w_max = 2.8f,
                                  .w_min_ratio = 0.32f,
                                  .entry = 0.10f,
                                  .exit = 0.25f,
                                  .dry = 0.0f,
                                  .seed = 441u},
                   (InkTone){.color = PAL_KIN_HOT, .alpha = 0.96f}, 1.0f);
        break;
    }
    case RELIC_ONI_TILE: {
        /* Stout horned roof tile silhouette */
        const Vector2 tile[6] = {
            {c.x - 16.0f, c.y - 10.0f}, {c.x, c.y - 15.0f}, {c.x + 16.0f, c.y - 10.0f},
            {c.x + 18.0f, c.y + 12.0f}, {c.x, c.y + 15.0f}, {c.x - 18.0f, c.y + 12.0f},
        };
        ink_fill_poly(r, tile, 6, pal_color_alpha(PAL_SUMI_HEAVY, 0.85f));
        ink_outline(r, tile, 6, true, 3.6f, (Vector2){0.7f, 0.7f}, 442u);
        const Vector2 lh[2] = {{c.x - 10.0f, c.y - 11.0f}, {c.x - 16.0f, c.y - 22.0f}};
        const Vector2 rh[2] = {{c.x + 10.0f, c.y - 11.0f}, {c.x + 16.0f, c.y - 22.0f}};
        const BrushProfile hp = {.w_max = 3.4f,
                                 .w_min_ratio = 0.22f,
                                 .entry = 0.12f,
                                 .exit = 0.55f,
                                 .dry = 0.15f,
                                 .seed = 443u};
        ink_stroke(r, lh, 2, hp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f}, 1.0f);
        ink_stroke(r, rh, 2, hp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f}, 1.0f);
        ink_teardrop(r, (Vector2){c.x - 6.0f, c.y - 1.0f}, (Vector2){1.0f, 0.0f}, 5.5f, 3.5f,
                     pal_color_alpha(PAL_GOFUN, 0.95f));
        ink_teardrop(r, (Vector2){c.x + 6.0f, c.y - 1.0f}, (Vector2){-1.0f, 0.0f}, 5.5f, 3.5f,
                     pal_color_alpha(PAL_GOFUN, 0.95f));
        break;
    }
    case RELIC_ENDLESS_BRUSH: {
        /* Continuous figure-eight calligraphic brush ribbon */
        const Vector2 loop[9] = {
            {c.x, c.y}, {c.x - 11.0f, c.y - 11.0f}, {c.x - 20.0f, c.y}, {c.x - 11.0f, c.y + 11.0f},
            {c.x, c.y}, {c.x + 11.0f, c.y - 11.0f}, {c.x + 20.0f, c.y}, {c.x + 11.0f, c.y + 11.0f},
            {c.x, c.y},
        };
        ink_stroke(r, loop, 9,
                   (BrushProfile){.w_max = 4.2f,
                                  .w_min_ratio = 0.24f,
                                  .entry = 0.12f,
                                  .exit = 0.30f,
                                  .dry = 0.30f,
                                  .seed = 444u},
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.95f}, 1.0f);
        ink_teardrop(r, (Vector2){c.x + 16.0f, c.y - 2.0f}, (Vector2){0.5f, -0.8f}, 7.0f, 4.5f,
                     pal_color_alpha(PAL_KIN, 0.94f));
        break;
    }
    case RELIC_NIGHT_PARADE_SCROLL: {
        /* Unfurled painted scroll with twin rollers and vermilion seal */
        const Rectangle sc = {c.x - 18.0f, c.y - 11.0f, 36.0f, 22.0f};
        ink_fill_panel(r, sc, PAL_PAPER_ACT1, 0.95f, PAL_SUMI_DARK, 0.92f, 448u);
        const Vector2 lrod[2] = {{c.x - 19.0f, c.y - 14.0f}, {c.x - 19.0f, c.y + 14.0f}};
        const Vector2 rrod[2] = {{c.x + 19.0f, c.y - 14.0f}, {c.x + 19.0f, c.y + 14.0f}};
        const BrushProfile rp = {.w_max = 4.4f,
                                 .w_min_ratio = 0.40f,
                                 .entry = 0.10f,
                                 .exit = 0.10f,
                                 .dry = 0.10f,
                                 .seed = 449u};
        ink_stroke(r, lrod, 2, rp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f}, 1.0f);
        ink_stroke(r, rrod, 2, rp, (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.96f}, 1.0f);
        ink_teardrop(r, (Vector2){c.x + 7.0f, c.y + 2.0f}, (Vector2){0.0f, -1.0f}, 6.5f, 5.5f,
                     pal_color_alpha(PAL_SHU, 0.94f));
        break;
    }
    default:
        ink_arc_stroke(r, c, 15.0f, 0.3f, 5.5f,
                       (BrushProfile){.w_max = 3.8f,
                                      .w_min_ratio = 0.28f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.25f,
                                      .seed = 435u + (uint32_t)rid},
                       (InkTone){.color = PAL_KIN, .alpha = 0.92f});
        ink_wash(r, c, 9.0f, 9.0f, 0.0f, (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.65f}, 1,
                 436u + (uint32_t)rid);
        break;
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

static void draw_crystal_emblem(RenderState *r, Vector2 c)
{
    ink_wash(r, c, 32.0f, 26.0f, 0.12f, (InkTone){.color = PAL_KIN, .alpha = 0.28f}, 2, 490u);
    const Vector2 gem[5] = {
        {c.x, c.y - 22.0f},         {c.x + 15.0f, c.y - 4.0f}, {c.x + 10.0f, c.y + 18.0f},
        {c.x - 11.0f, c.y + 17.0f}, {c.x - 15.0f, c.y - 5.0f},
    };
    ink_fill_poly(r, gem, 5, pal_color_alpha(PAL_SUMI_SCORCH, 0.92f));
    ink_outline_tone(r, gem, 5, true, 3.2f, (Vector2){0.7f, 0.7f},
                     (InkTone){.color = PAL_KIN, .alpha = 0.96f}, 491u);
    ink_teardrop(r, (Vector2){c.x - 4.0f, c.y - 6.0f}, (Vector2){0.6f, -0.8f}, 8.5f, 5.0f,
                 pal_color_alpha(PAL_KIN_HOT, 0.96f));
}

void render_draw_reward_ui(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || g->phase != RUN_PHASE_REWARD) {
        return;
    }

    const RewardOffer *rw = &g->run.reward;
    RewardLayout layout;
    render_reward_layout(g, &layout);

    const Vector2 full[4] = {{0.0f, 0.0f},
                             {(float)INK_SCREEN_W, 0.0f},
                             {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                             {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, full, 4, pal_color_alpha(pal_paper_for_act(g->run.act_index), 0.72f));

    /* Scroll backdrop for Relic reward */
    if (rw->kind == REWARD_KIND_RELIC) {
        const Rectangle scroll = {138.0f, 130.0f, (float)INK_SCREEN_W - 276.0f, 404.0f};
        ink_fill_panel(r, scroll, PAL_PAPER_ACT1, 0.90f, PAL_KIN, 0.85f, 445u);
        const Rectangle lrod = {scroll.x - 14.0f, scroll.y - 8.0f, 16.0f, scroll.height + 16.0f};
        const Rectangle rrod = {scroll.x + scroll.width - 2.0f, scroll.y - 8.0f, 16.0f,
                                scroll.height + 16.0f};
        ink_fill_panel(r, lrod, PAL_SUMI_HEAVY, 0.95f, PAL_SUMI_SCORCH, 0.90f, 446u);
        ink_fill_panel(r, rrod, PAL_SUMI_HEAVY, 0.95f, PAL_SUMI_SCORCH, 0.90f, 447u);
    }

    TextId heading = TEXT_REWARD_TITLE;
    if (rw->kind == REWARD_KIND_FALLBACK) {
        heading = TEXT_REWARD_FALLBACK_TITLE;
    } else if (rw->kind == REWARD_KIND_RELIC) {
        heading = TEXT_REWARD_RELIC_TITLE;
    }
    /* Title brush font strictly >= 40px (42px) */
    render_draw_text_centered(r, text_get(heading, r->use_ja),
                              (Vector2){(float)INK_SCREEN_W * 0.5f, 76.0f}, 42.0f, PAL_SUMI_SCORCH,
                              0.96f, true);
    TextId hint_id = TEXT_REWARD_HINT;
    if (rw->kind == REWARD_KIND_FALLBACK) {
        hint_id = layout.has_crystal ? TEXT_REWARD_FALLBACK_HINT : TEXT_REWARD_FALLBACK_SINGLE_HINT;
    }
    render_draw_text_centered(r, text_get(hint_id, r->use_ja),
                              (Vector2){(float)INK_SCREEN_W * 0.5f, 116.0f}, 18.0f, PAL_SUMI_HEAVY,
                              0.88f, false);

    const float anim = r->reward_reroll_t > 0.0f ? (1.0f - r->reward_reroll_t / 0.40f)
                                                 : ink_clampf(r->reward_anim_t, 0.0f, 1.0f);
    const float text_a = ink_clampf((anim - 0.40f) / 0.45f, 0.0f, 1.0f);
    const int32_t hover = ui != NULL ? ui->hover_choice : UI_HOVER_NONE;

    for (int32_t i = 0; i < layout.card_count; ++i) {
        Rectangle card = layout.cards[i];
        const bool is_hover = (hover == i);
        if (is_hover) {
            card.y -= 8.0f;
        }
        const float h_vis = card.height * ink_clampf(anim, 0.25f, 1.0f);
        const Rectangle vis = {card.x, card.y, card.width, h_vis};
        const float cx = vis.x + vis.width * 0.5f;

        if (rw->kind == REWARD_KIND_RELIC) {
            const RelicId rid = (RelicId)rw->choice_ids[i];
            const PaletteId border = is_hover ? PAL_SHU : PAL_KIN;
            ink_fill_panel(r, vis, PAL_GOFUN, 0.96f, border, 0.94f, 450u + (uint32_t)i);
            if (text_a <= 0.01f) {
                continue;
            }
            draw_relic_emblem(r, (Vector2){cx, vis.y + 62.0f}, rid);

            char hdr[32];
            snprintf(hdr, sizeof hdr, "[%d]", i + 1);
            render_draw_text_centered(r, hdr, (Vector2){cx, vis.y + 108.0f}, 18.0f, PAL_KIN,
                                      0.94f * text_a, false);
            render_draw_text_fit(r, text_get(text_relic_name(rid), r->use_ja),
                                 (Vector2){cx, vis.y + 146.0f}, vis.width - 32.0f, 26.0f, 18.0f,
                                 PAL_SUMI_SCORCH, 0.98f * text_a, false);

            const Vector2 div_pts[2] = {{vis.x + 28.0f, vis.y + 180.0f},
                                        {vis.x + vis.width - 28.0f, vis.y + 180.0f}};
            ink_stroke(r, div_pts, 2,
                       (BrushProfile){.w_max = 2.4f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.25f,
                                      .dry = 0.3f,
                                      .seed = 460u + (uint32_t)i},
                       (InkTone){.color = PAL_KIN, .alpha = 0.72f * text_a}, 1.0f);

            render_draw_text_wrapped(
                r, text_get(text_relic_effect(rid), r->use_ja),
                (Rectangle){vis.x + 18.0f, vis.y + 192.0f, vis.width - 36.0f, 64.0f}, 18.0f, 13.0f,
                PAL_SUMI_DARK, 0.94f * text_a, false, true);

            const TextId tr_id = text_relic_tradeoff(rid);
            if (tr_id != TEXT_NONE) {
                char tr_buf[160];
                snprintf(tr_buf, sizeof tr_buf, "%s%s", text_get(TEXT_REWARD_TRADEOFF, r->use_ja),
                         text_get(tr_id, r->use_ja));
                render_draw_text_wrapped(
                    r, tr_buf, (Rectangle){vis.x + 18.0f, vis.y + 260.0f, vis.width - 36.0f, 52.0f},
                    18.0f, 13.0f, PAL_SHU, 0.96f * text_a, false, true);
            }
        } else {
            const UpgradeId uid = (UpgradeId)rw->choice_ids[i];
            const UpgradeAxis axis = game_upgrade_axis(uid);
            const PaletteId border = is_hover ? pal_axis_color(axis) : PAL_SUMI_DARK;
            ink_fill_panel(r, vis, PAL_GOFUN, 0.95f, border, 0.94f, 450u + (uint32_t)i);

            if (text_a <= 0.01f) {
                continue;
            }

            draw_axis_emblem(r, (Vector2){cx, vis.y + 42.0f}, axis);

            char hdr[64];
            snprintf(hdr, sizeof hdr, "[%d] %s", i + 1, text_get(text_axis_name(axis), r->use_ja));
            render_draw_text_centered(r, hdr, (Vector2){cx, vis.y + 82.0f}, 18.0f,
                                      pal_axis_color(axis), 0.94f * text_a, false);

            const char *name = text_get(text_upgrade_name(uid), r->use_ja);
            render_draw_text_fit(r, name, (Vector2){cx, vis.y + 124.0f}, vis.width - 32.0f, 26.0f,
                                 18.0f, PAL_SUMI_SCORCH, 0.98f * text_a, false);

            const uint8_t cur_lv = game_upgrade_level(g, uid);
            const uint8_t next_lv = rw->choice_next_level[i] > 0 ? rw->choice_next_level[i] : 1;
            char lv_buf[48];
            snprintf(lv_buf, sizeof lv_buf, "%s %u -> %u", text_get(TEXT_REWARD_LV, r->use_ja),
                     (unsigned)cur_lv, (unsigned)next_lv);
            render_draw_text_centered(r, lv_buf, (Vector2){cx, vis.y + 168.0f}, 20.0f,
                                      PAL_SUMI_HEAVY, 0.92f * text_a, false);

            const Vector2 div_pts[2] = {{vis.x + 28.0f, vis.y + 196.0f},
                                        {vis.x + vis.width - 28.0f, vis.y + 196.0f}};
            ink_stroke(r, div_pts, 2,
                       (BrushProfile){.w_max = 2.4f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.15f,
                                      .exit = 0.25f,
                                      .dry = 0.3f,
                                      .seed = 460u + (uint32_t)i},
                       (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.70f * text_a}, 1.0f);

            const char *eff = text_get(text_upgrade_effect(uid, (int)next_lv), r->use_ja);
            render_draw_text_wrapped(
                r, eff, (Rectangle){vis.x + 18.0f, vis.y + 210.0f, vis.width - 36.0f, 96.0f}, 18.0f,
                13.0f, PAL_SUMI_DARK, 0.94f * text_a, false, true);
        }

        if (is_hover) {
            seal_stamp(r, TEXT_SEAL_DONE,
                       (Vector2){vis.x + vis.width - 32.0f, vis.y + vis.height - 32.0f}, 34.0f,
                       SEAL_SHUBUN, 0.5f);
        }
    }

    if (layout.has_reroll) {
        const bool can_reroll =
            (rw->kind == REWARD_KIND_UPGRADE || rw->kind == REWARD_KIND_RELIC) &&
            g->run.rerolls_left > 0;
        const bool hov = hover == UI_HOVER_REROLL && can_reroll;
        ink_fill_panel(r, layout.reroll_rect, can_reroll ? PAL_GOFUN : PAL_SUMI_CLEAR,
                       can_reroll ? 0.94f : 0.45f,
                       hov ? PAL_SHU : (can_reroll ? PAL_SUMI_DARK : PAL_SUMI_LIGHT), 0.90f, 480u);
        char rb[64];
        snprintf(rb, sizeof rb, "%s (%d)", text_get(TEXT_REWARD_REROLL, r->use_ja),
                 g->run.rerolls_left);
        render_draw_text_fit(r, rb,
                             (Vector2){layout.reroll_rect.x + layout.reroll_rect.width * 0.5f,
                                       layout.reroll_rect.y + layout.reroll_rect.height * 0.5f},
                             layout.reroll_rect.width - 24.0f, 20.0f, 14.0f,
                             can_reroll ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, 0.95f, false);
    }

    if (layout.has_repair) {
        const bool can_rep = rw->repair_available;
        const bool hov = hover == UI_HOVER_REPAIR && can_rep;
        if (rw->kind == REWARD_KIND_FALLBACK && layout.has_crystal) {
            Rectangle card = layout.repair_rect;
            if (hov) {
                card.y -= 8.0f;
            }
            const float rcx = card.x + card.width * 0.5f;
            ink_fill_panel(r, card, can_rep ? PAL_GOFUN : PAL_SUMI_CLEAR, can_rep ? 0.96f : 0.45f,
                           hov ? PAL_SHU : (can_rep ? PAL_SUMI_DARK : PAL_SUMI_LIGHT), 0.94f, 481u);
            draw_mini_shrine_icon(r, (Vector2){rcx, card.y + 62.0f}, can_rep);
            render_draw_text_centered(r, "[1] / [4]", (Vector2){rcx, card.y + 104.0f}, 18.0f,
                                      PAL_SUMI_HEAVY, 0.92f, false);
            const int32_t rep_amt = rw->repair_amount > 0 ? rw->repair_amount : 1;
            char rep_buf[64];
            snprintf(rep_buf, sizeof rep_buf, "%s +%d",
                     text_get(TEXT_REWARD_REPAIR_SHORT, r->use_ja), rep_amt);
            render_draw_text_centered(r, rep_buf, (Vector2){rcx, card.y + 150.0f}, 26.0f,
                                      can_rep ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, 0.98f, false);
            render_draw_text_fit(r, text_get(TEXT_REWARD_REPAIR_EXCESS, r->use_ja),
                                 (Vector2){rcx, card.y + 206.0f}, card.width - 32.0f, 18.0f, 13.0f,
                                 PAL_KIN, 0.94f, false);
            if (hov) {
                seal_stamp(r, TEXT_SEAL_DONE,
                           (Vector2){card.x + card.width - 32.0f, card.y + card.height - 32.0f},
                           34.0f, SEAL_SHUBUN, 0.5f);
            }
        } else {
            ink_fill_panel(r, layout.repair_rect, can_rep ? PAL_GOFUN : PAL_SUMI_CLEAR,
                           can_rep ? 0.94f : 0.45f,
                           hov ? PAL_SHU : (can_rep ? PAL_SUMI_DARK : PAL_SUMI_LIGHT), 0.90f, 481u);
            draw_mini_shrine_icon(
                r,
                (Vector2){layout.repair_rect.x + 30.0f,
                          layout.repair_rect.y + layout.repair_rect.height * 0.5f},
                can_rep);
            render_draw_text_fit(
                r, text_get(TEXT_REWARD_REPAIR, r->use_ja),
                (Vector2){layout.repair_rect.x + layout.repair_rect.width * 0.5f + 18.0f,
                          layout.repair_rect.y + layout.repair_rect.height * 0.5f},
                layout.repair_rect.width - 76.0f, 19.0f, 13.0f,
                can_rep ? PAL_SUMI_SCORCH : PAL_SUMI_LIGHT, 0.95f, false);
        }
    }

    if (layout.has_crystal) {
        Rectangle card = layout.crystal_rect;
        const bool hov = (hover == UI_HOVER_CRYSTAL);
        if (hov) {
            card.y -= 8.0f;
        }
        const float ccx = card.x + card.width * 0.5f;
        ink_fill_panel(r, card, PAL_GOFUN, 0.96f, hov ? PAL_SHU : PAL_KIN, 0.94f, 482u);
        draw_crystal_emblem(r, (Vector2){ccx, card.y + 62.0f});
        render_draw_text_centered(r, "[2]", (Vector2){ccx, card.y + 104.0f}, 18.0f, PAL_KIN, 0.94f,
                                  false);
        render_draw_text_centered(r, text_get(TEXT_REWARD_CRYSTAL_TITLE, r->use_ja),
                                  (Vector2){ccx, card.y + 150.0f}, 26.0f, PAL_SUMI_SCORCH, 0.98f,
                                  false);
        char cry_buf[64];
        snprintf(cry_buf, sizeof cry_buf, "+%d %s", rw->crystal_reward,
                 text_get(TEXT_BOKUAN_COST_UNIT, r->use_ja));
        render_draw_text_centered(r, cry_buf, (Vector2){ccx, card.y + 206.0f}, 24.0f, PAL_KIN,
                                  0.98f, false);
        if (hov) {
            seal_stamp(r, TEXT_SEAL_DONE,
                       (Vector2){card.x + card.width - 32.0f, card.y + card.height - 32.0f}, 34.0f,
                       SEAL_SHUBUN, 0.5f);
        }
    }
}
