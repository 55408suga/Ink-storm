#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>

void render_draw_banners(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }

    const float cx = (float)INK_SCREEN_W * 0.5f;

    /* Act transition scroll title (2.0s) */
    if (g->phase == RUN_PHASE_ACT_TRANSITION) {
        const float u = ink_clampf(g->phase_t / 2.0f, 0.0f, 1.0f);
        const float open =
            u < 0.25f ? (u / 0.25f) : (u > 0.80f ? (1.0f - (u - 0.80f) / 0.20f) : 1.0f);
        const float bw = 720.0f * ink_clampf(open, 0.1f, 1.0f);
        const Rectangle band = {cx - bw * 0.5f, (float)INK_SCREEN_H * 0.38f, bw, 118.0f};
        ink_fill_panel(r, band, PAL_GOFUN, 0.92f * open, PAL_SUMI_DARK, 0.88f * open, 600u);
        if (open > 0.35f) {
            const TextId title_id = text_act_title(g->run.act_index);
            render_draw_text_centered(
                r, text_get(title_id != TEXT_NONE ? title_id : TEXT_ACT1_TITLE, r->use_ja),
                (Vector2){cx, band.y + band.height * 0.5f}, 42.0f, PAL_SUMI_SCORCH, open, true);
        }
    }

    /* Boss entrance calligraphic title */
    if (r->boss_banner_t > 0.0f) {
        const float a = ink_clampf(r->boss_banner_t * 1.4f, 0.0f, 1.0f);
        const Rectangle band = {cx - 320.0f, 88.0f, 640.0f, 82.0f};
        ink_fill_panel(r, band, PAL_GOFUN, 0.90f * a, PAL_BENGARA, 0.88f * a, 610u);
        const TextId ep = text_boss_epithet(r->boss_banner_id);
        render_draw_text_centered(
            r, text_get(ep != TEXT_NONE ? ep : TEXT_BOSS_TENGU_EPITHET, r->use_ja),
            (Vector2){cx, 129.0f}, 40.0f, PAL_SUMI_SCORCH, a, true);
    }

    /* Enemy intro strip at top (2.0s per queued enemy; waits for boss banner to avoid overlap) */
    if (r->intro_count > 0 && r->boss_banner_t <= 0.0f &&
        (g->phase == RUN_PHASE_PLAYING || g->phase == RUN_PHASE_WAVE_AFTERMATH)) {
        const EnemyDefId eid = r->intro_queue[r->intro_head];
        const float a = r->intro_t < 0.2f ? (r->intro_t / 0.2f)
                                          : (r->intro_t > 1.7f ? (2.0f - r->intro_t) / 0.3f : 1.0f);
        const Rectangle box = {cx - 280.0f, 68.0f, 560.0f, 66.0f};
        ink_fill_panel(r, box, PAL_GOFUN, 0.92f * a, PAL_SUMI_DARK, 0.85f * a, 620u);
        render_draw_text_centered(r, text_get(text_enemy_name(eid), r->use_ja),
                                  (Vector2){cx, box.y + 22.0f}, 22.0f, PAL_SUMI_SCORCH, a, false);
        render_draw_text_centered(r, text_get(text_enemy_intro(eid), r->use_ja),
                                  (Vector2){cx, box.y + 46.0f}, 18.0f, PAL_SUMI_HEAVY, a, false);
    }

    /* Game-over falling ink curtains (1.5s) */
    if (r->gameover_drip_active && g->phase == RUN_PHASE_SHRINE_BREAK) {
        const float u = ink_clampf(r->gameover_drip_t / 1.5f, 0.0f, 1.0f);
        for (int k = 0; k < 10; ++k) {
            const float x = (float)k * ((float)INK_SCREEN_W / 10.0f) + 64.0f;
            const float drop_h = (float)INK_SCREEN_H * u * (0.75f + 0.35f * sinf((float)k * 1.9f));
            const Vector2 dpts[2] = {{x, -10.0f}, {x, drop_h}};
            ink_stroke(r, dpts, 2,
                       (BrushProfile){.w_max = 110.0f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.05f,
                                      .exit = 0.35f,
                                      .dry = 0.2f,
                                      .seed = 650u + (uint32_t)k},
                       (InkTone){.color = PAL_SUMI_SCORCH, .alpha = 0.78f * u}, 1.0f);
        }
    }

    (void)ui;
}

void render_draw_showcase_overlay(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL || ui->scene != UI_SCENE_SHOWCASE) {
        return;
    }

    /* Showcase page 1: palette swatches + 6 brush primitive specimens in bottom band */
    if (ui->showcase_page == 1) {
        const Rectangle strip = {24.0f, 548.0f, (float)INK_SCREEN_W - 48.0f, 152.0f};
        ink_fill_panel(r, strip, PAL_PAPER_ACT1, 0.90f, PAL_SUMI_LIGHT, 0.65f, 800u);

        /* Palette swatches (all 16 tokens) */
        for (int p = 0; p < PAL_COUNT; ++p) {
            const float sx = 42.0f + (float)p * 34.0f;
            const float sy = 568.0f;
            const Vector2 sq[4] = {
                {sx, sy}, {sx + 26.0f, sy}, {sx + 26.0f, sy + 26.0f}, {sx, sy + 26.0f}};
            ink_fill_poly(r, sq, 4, pal_color((PaletteId)p));
            ink_outline_tone(r, sq, 4, true, 1.8f, (Vector2){0.7f, 0.7f},
                             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.65f},
                             810u + (uint32_t)p);
        }

        /* 6 primitives specimen row */
        const float py = 648.0f;
        /* 1. ink_stroke */
        const Vector2 spts[4] = {
            {46.0f, py + 12.0f}, {92.0f, py - 14.0f}, {148.0f, py + 10.0f}, {198.0f, py - 8.0f}};
        ink_stroke(r, spts, 4,
                   (BrushProfile){.w_max = 9.0f,
                                  .w_min_ratio = 0.22f,
                                  .entry = 0.15f,
                                  .exit = 0.30f,
                                  .dry = 0.45f,
                                  .seed = 820u},
                   (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.95f}, 1.0f);
        /* 2. ink_wash */
        ink_wash(r, (Vector2){286.0f, py}, 38.0f, 22.0f, -0.15f,
                 (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.55f}, 3, 821u);
        /* 3. ink_splatter */
        uint64_t demo_rng = UINT64_C(0x53484f5743415345);
        ink_splatter(r, (Vector2){410.0f, py}, (Vector2){1.0f, -0.25f}, 0.85f,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f}, &demo_rng);
        /* 4. ink_outline */
        const Vector2 opt[6] = {{530.0f, py - 20.0f}, {566.0f, py - 14.0f}, {574.0f, py + 14.0f},
                                {542.0f, py + 24.0f}, {510.0f, py + 12.0f}, {512.0f, py - 12.0f}};
        ink_outline(r, opt, 6, true, 4.5f, (Vector2){0.7f, 0.7f}, 822u);
        /* 5. ink_knockout on dark ink swatch */
        const Vector2 dark_bg[4] = {
            {630.0f, py - 30.0f}, {720.0f, py - 30.0f}, {720.0f, py + 30.0f}, {630.0f, py + 30.0f}};
        ink_fill_poly(r, dark_bg, 4, pal_color_alpha(PAL_SUMI_SCORCH, 0.92f));
        const Vector2 ko_demo[5] = {{656.0f, py - 14.0f},
                                    {690.0f, py - 12.0f},
                                    {696.0f, py + 12.0f},
                                    {668.0f, py + 16.0f},
                                    {648.0f, py + 4.0f}};
        ink_knockout(r, ko_demo, 5, 4.0f, 0);
        ink_fill_poly(r, ko_demo, 5, pal_color_alpha(PAL_SUMI_DARK, 0.96f));
        /* 6. seal_stamp */
        seal_stamp(r, TEXT_SEAL_PURIFY, (Vector2){786.0f, py}, 46.0f, SEAL_HAKUBUN, 0.8f);
        seal_stamp(r, TEXT_SEAL_SLAY, (Vector2){846.0f, py}, 46.0f, SEAL_SHUBUN, 0.8f);

        /* Legend labels on right */
        render_draw_text(r, "P1: Entities / Projectiles / Fluid / Palette / 6 Prims",
                         (Vector2){624.0f, 570.0f}, 18.0f, PAL_SUMI_DARK, 0.92f, false);
        render_draw_text(r, "[1-4] Page  [F1] Metrics  [F12] Save", (Vector2){884.0f, 640.0f},
                         16.0f, PAL_SUMI_HEAVY, 0.88f, false);
    }

    char pg[64];
    snprintf(pg, sizeof pg, "%s %d/4", text_get(TEXT_SHOWCASE_HEADER, r->use_ja),
             ui->showcase_page);
    const Rectangle badge = {14.0f, (float)INK_SCREEN_H - 38.0f, 154.0f, 28.0f};
    ink_fill_panel(r, badge, PAL_GOFUN, 0.88f, PAL_SUMI_LIGHT, 0.65f, 830u);
    render_draw_text(r, pg, (Vector2){24.0f, (float)INK_SCREEN_H - 34.0f}, 18.0f, PAL_SUMI_SCORCH,
                     0.94f, false);
}
