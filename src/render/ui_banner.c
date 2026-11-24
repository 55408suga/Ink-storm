#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>

static void draw_clear_banner(RenderState *r)
{
    if (r->clear_banner_t <= 0.0f || r->clear_banner_text == TEXT_NONE) {
        return;
    }
    const float u = 1.0f - r->clear_banner_t;
    const float open = u < 0.18f ? u / 0.18f : (u > 0.78f ? (1.0f - u) / 0.22f : 1.0f);
    const float a = ink_clampf(open, 0.0f, 1.0f);
    const float cx = (float)INK_SCREEN_W * 0.5f;
    const float bw = 520.0f * ink_clampf(0.35f + 0.65f * open, 0.0f, 1.0f);
    const Rectangle band = {cx - bw * 0.5f, 262.0f, bw, 78.0f};
    ink_fill_panel(r, band, PAL_GOFUN, 0.90f * a, PAL_KIN, 0.85f * a, 640u);
    if (a > 0.25f) {
        render_draw_text_fit(r, text_get(r->clear_banner_text, r->use_ja),
                             (Vector2){cx - 24.0f, band.y + band.height * 0.5f},
                             band.width - 110.0f, 44.0f, 30.0f, PAL_SUMI_SCORCH, a, true);
    }
}

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

        char title_buf[128];
        const char *title_str = NULL;
        if (g->run.act_index >= GAME_MAIN_ACT_COUNT) {
            static const TextId k_subtitles[GAME_MAIN_ACT_COUNT] = {
                TEXT_ACT1_SUBTITLE, TEXT_ACT2_SUBTITLE, TEXT_ACT3_SUBTITLE};
            const int32_t stage_act = g->run.act_index % GAME_MAIN_ACT_COUNT;
            const int32_t loop_n = g->run.act_index / GAME_MAIN_ACT_COUNT;
            const char *sub = text_get(k_subtitles[stage_act], r->use_ja);
            if (r->use_ja) {
                snprintf(title_buf, sizeof title_buf, "%s%d%s (%s%d) %s",
                         text_get(TEXT_ENDLESS_ACT_PREFIX, true), g->run.act_index + 1,
                         text_get(TEXT_ENDLESS_ACT_SUFFIX, true),
                         text_get(TEXT_ENDLESS_LOOP_LABEL, true), loop_n, sub);
            } else {
                snprintf(title_buf, sizeof title_buf, "Act %d (Loop %d): %s", g->run.act_index + 1,
                         loop_n, sub);
            }
            title_str = title_buf;
        } else {
            const TextId title_id = text_act_title(g->run.act_index);
            title_str = text_get(title_id != TEXT_NONE ? title_id : TEXT_ACT1_TITLE, r->use_ja);
        }

        const Vector2 tm = render_measure_text(r, title_str, 44.0f, true);
        const float target_bw = ink_clampf(fmaxf(720.0f, tm.x + 96.0f), 720.0f, 1180.0f);
        const float bw = target_bw * ink_clampf(open, 0.1f, 1.0f);
        const Rectangle band = {cx - bw * 0.5f, (float)INK_SCREEN_H * 0.38f, bw, 118.0f};
        ink_fill_panel(r, band, PAL_GOFUN, 0.92f * open, PAL_SUMI_DARK, 0.88f * open, 600u);
        if (open > 0.35f) {
            render_draw_text_centered(r, title_str, (Vector2){cx, band.y + band.height * 0.5f},
                                      44.0f, PAL_SUMI_SCORCH, open, true);
        }
    }

    /* Boss entrance calligraphic title */
    if (r->boss_banner_t > 0.0f) {
        const float a = ink_clampf(r->boss_banner_t * 1.4f, 0.0f, 1.0f);
        const Rectangle band = {cx - 320.0f, 88.0f, 640.0f, 82.0f};
        ink_fill_panel(r, band, PAL_GOFUN, 0.90f * a, PAL_BENGARA, 0.88f * a, 610u);
        const TextId ep = text_boss_epithet(r->boss_banner_id);
        render_draw_text_fit(r, text_get(ep != TEXT_NONE ? ep : TEXT_BOSS_TENGU_EPITHET, r->use_ja),
                             (Vector2){cx, 129.0f}, band.width - 40.0f, 42.0f, 28.0f,
                             PAL_SUMI_SCORCH, a, true);
    }

    /* Enemy intro strip at top (2.0s per queued enemy; waits for boss banner to avoid overlap).
     * Hidden during the purification so it never covers the clear banner. */
    if (r->intro_count > 0 && r->boss_banner_t <= 0.0f && g->phase == RUN_PHASE_PLAYING) {
        const EnemyDefId eid = r->intro_queue[r->intro_head];
        const float a = r->intro_t < 0.2f ? (r->intro_t / 0.2f)
                                          : (r->intro_t > 1.7f ? (2.0f - r->intro_t) / 0.3f : 1.0f);
        const Rectangle box = {cx - 300.0f, 68.0f, 600.0f, 66.0f};
        ink_fill_panel(r, box, PAL_GOFUN, 0.92f * a, PAL_SUMI_DARK, 0.85f * a, 620u);
        render_draw_text_fit(r, text_get(text_enemy_name(eid), r->use_ja),
                             (Vector2){cx, box.y + 22.0f}, box.width - 32.0f, 22.0f, 16.0f,
                             PAL_SUMI_SCORCH, a, false);
        render_draw_text_fit(r, text_get(text_enemy_intro(eid), r->use_ja),
                             (Vector2){cx, box.y + 46.0f}, box.width - 32.0f, 18.0f, 13.0f,
                             PAL_SUMI_HEAVY, a, false);
    }

    draw_clear_banner(r);

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

void render_draw_hint_strip(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL || ui->active_hint_t <= 0.0f || ui->paused) {
        return;
    }
    if (g->phase == RUN_PHASE_REWARD || g->phase == RUN_PHASE_VICTORY ||
        g->phase == RUN_PHASE_GAME_OVER || g->phase == RUN_PHASE_WAVE_AFTERMATH) {
        return;
    }
    const TextId tid = text_hint_message(ui->active_hint);
    if (tid == TEXT_NONE) {
        return;
    }

    const float cx = (float)INK_SCREEN_W * 0.5f;
    float y = 66.0f;
    if (r->boss_banner_t > 0.0f) {
        y = 178.0f;
    } else if (r->intro_count > 0) {
        y = 142.0f;
    }

    const float t = ui->active_hint_t;
    const float a = t > 1.75f ? ((2.0f - t) / 0.25f) : (t < 0.30f ? (t / 0.30f) : 1.0f);
    const char *msg = text_get(tid, r->use_ja);
    const Vector2 m = render_measure_text(r, msg, 18.0f, false);
    const float bw = ink_clampf(m.x + 56.0f, 360.0f, (float)INK_SCREEN_W - 80.0f);
    const Rectangle strip = {cx - bw * 0.5f, y, bw, 40.0f};
    ink_fill_panel(r, strip, PAL_GOFUN, 0.92f * a, PAL_KIN, 0.85f * a, 635u);
    render_draw_text_fit(r, msg, (Vector2){cx, strip.y + strip.height * 0.5f}, bw - 32.0f, 18.0f,
                         13.0f, PAL_SUMI_SCORCH, a, false);
}

void render_draw_showcase_overlay(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || ui == NULL || ui->scene != UI_SCENE_SHOWCASE) {
        return;
    }

    if (ui->showcase_page == 1) {
        const Rectangle strip = {24.0f, 558.0f, (float)INK_SCREEN_W - 48.0f, 144.0f};
        ink_fill_panel(r, strip, PAL_PAPER_ACT1, 0.90f, PAL_SUMI_LIGHT, 0.65f, 800u);

        for (int p = 0; p < PAL_COUNT; ++p) {
            const float sx = 42.0f + (float)p * 34.0f;
            const float sy = 574.0f;
            const Vector2 sq[4] = {
                {sx, sy}, {sx + 26.0f, sy}, {sx + 26.0f, sy + 24.0f}, {sx, sy + 24.0f}};
            ink_fill_poly(r, sq, 4, pal_color((PaletteId)p));
            ink_outline_tone(r, sq, 4, true, 1.8f, (Vector2){0.7f, 0.7f},
                             (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.65f},
                             810u + (uint32_t)p);
        }

        const float py = 650.0f;
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
        ink_wash(r, (Vector2){286.0f, py}, 38.0f, 22.0f, -0.15f,
                 (InkTone){.color = PAL_SUMI_HEAVY, .alpha = 0.55f}, 3, 821u);
        uint64_t demo_rng = UINT64_C(0x53484f5743415345);
        ink_splatter(r, (Vector2){410.0f, py}, (Vector2){1.0f, -0.25f}, 0.85f,
                     (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.90f}, &demo_rng);
        const Vector2 opt[6] = {{530.0f, py - 20.0f}, {566.0f, py - 14.0f}, {574.0f, py + 14.0f},
                                {542.0f, py + 24.0f}, {510.0f, py + 12.0f}, {512.0f, py - 12.0f}};
        ink_outline(r, opt, 6, true, 4.5f, (Vector2){0.7f, 0.7f}, 822u);
        const Vector2 dark_bg[4] = {
            {630.0f, py - 28.0f}, {720.0f, py - 28.0f}, {720.0f, py + 28.0f}, {630.0f, py + 28.0f}};
        ink_fill_poly(r, dark_bg, 4, pal_color_alpha(PAL_SUMI_SCORCH, 0.92f));
        const Vector2 ko_demo[5] = {{656.0f, py - 14.0f},
                                    {690.0f, py - 12.0f},
                                    {696.0f, py + 12.0f},
                                    {668.0f, py + 16.0f},
                                    {648.0f, py + 4.0f}};
        ink_knockout(r, ko_demo, 5, 4.0f, 0);
        ink_fill_poly(r, ko_demo, 5, pal_color_alpha(PAL_SUMI_DARK, 0.96f));
        seal_stamp(r, TEXT_SEAL_PURIFY, (Vector2){784.0f, py}, 44.0f, SEAL_HAKUBUN, 0.8f);
        seal_stamp(r, TEXT_SEAL_SANGEN, (Vector2){844.0f, py}, 44.0f, SEAL_SHUBUN, 0.8f);

        const char *p1_label =
            ui->showcase_subpage == 0
                ? "P1: 7 Regulars / 4 Elites / 3 Bosses / 5 Proj / Terrain / 6 Prims"
                : "P2: Ofuda Fox / Barrier / Homing Ofuda / Multi-part Ryu / Sunago";
        render_draw_text(r, p1_label, (Vector2){624.0f, 574.0f}, 18.0f, PAL_SUMI_DARK, 0.92f,
                         false);
        render_draw_text(r, "[1-6] Page  [Tab] Sub  [F1] Metrics  [F12] Save",
                         (Vector2){884.0f, 642.0f}, 16.0f, PAL_SUMI_HEAVY, 0.88f, false);
    }

    char pg[64];
    snprintf(pg, sizeof pg, "%s %d/6 [%d]", text_get(TEXT_SHOWCASE_HEADER, r->use_ja),
             ui->showcase_page, ui->showcase_subpage + 1);
    const Rectangle badge = {14.0f, (float)INK_SCREEN_H - 38.0f, 192.0f, 28.0f};
    ink_fill_panel(r, badge, PAL_GOFUN, 0.88f, PAL_SUMI_LIGHT, 0.65f, 830u);
    render_draw_text(r, pg, (Vector2){24.0f, (float)INK_SCREEN_H - 34.0f}, 18.0f, PAL_SUMI_SCORCH,
                     0.94f, false);
}
