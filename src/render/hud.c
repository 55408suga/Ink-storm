#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>

static const float k_hud_pi = 3.14159265359f;
static const float k_hud_two_pi = 6.28318530718f;

static void draw_top_band(RenderState *r, int act_index)
{
    const Rectangle band = {0.0f, 0.0f, (float)INK_SCREEN_W, 56.0f};
    ink_fill_panel(r, band, pal_paper_for_act(act_index), 0.82f, PAL_SUMI_CLEAR, 0.0f, 300u);
    const Vector2 edge[3] = {
        {16.0f, 56.0f},
        {(float)INK_SCREEN_W * 0.5f, 57.0f},
        {(float)INK_SCREEN_W - 16.0f, 55.5f},
    };
    const BrushProfile ep = {.w_max = 2.8f,
                             .w_min_ratio = 0.35f,
                             .entry = 0.08f,
                             .exit = 0.14f,
                             .dry = 0.50f,
                             .seed = 301u};
    ink_stroke(r, edge, 3, ep, (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.65f}, 1.0f);
}

static void draw_shrine_flames_and_ink(RenderState *r, const GameState *g)
{
    const int32_t hp = g->shrine_count > 0 ? g->shrines[0].hp : 0;
    const int32_t max_hp = g->shrine_count > 0 ? g->shrines[0].max_hp : 8;

    render_draw_text_id(r, TEXT_HUD_SHRINE, (Vector2){20.0f, 14.0f}, 20.0f, PAL_SUMI_DARK, 0.95f,
                        false);

    const float start_x = r->use_ja ? 58.0f : 96.0f;
    const int32_t shown = max_hp > 12 ? 12 : max_hp;
    for (int32_t i = 0; i < shown; ++i) {
        const Vector2 p = {start_x + (float)i * 18.0f, 26.0f};
        if (i < hp) {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, 11.0f, 7.0f,
                         pal_color_alpha(PAL_SHU, 0.92f));
            ink_teardrop(r, (Vector2){p.x, p.y + 1.0f}, (Vector2){0.0f, -1.0f}, 6.0f, 3.8f,
                         pal_color_alpha(PAL_SHU_HOT, 0.96f));
        } else {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, 9.0f, 5.5f,
                         pal_color_alpha(PAL_SUMI_CLEAR, 0.55f));
        }
    }

    /* Ink numeric readout (fixed digit width) */
    const float ink_x = start_x + (float)shown * 18.0f + 18.0f;
    render_draw_text_id(r, TEXT_HUD_INK, (Vector2){ink_x, 14.0f}, 20.0f,
                        g->player.dry ? PAL_SHU : PAL_SUMI_DARK, 0.95f, false);
    char ink_buf[32];
    snprintf(ink_buf, sizeof ink_buf, "%3d/%3d", (int)lroundf(g->player.ink),
             (int)lroundf(g->player.ink_max));
    render_draw_number_fixed(r, ink_buf, (Vector2){ink_x + 42.0f, 14.0f}, 20.0f,
                             g->player.surge_t > 0.0f ? PAL_KIN : PAL_SUMI_SCORCH, 0.95f);
}

static void draw_center_progress(RenderState *r, const GameState *g)
{
    const float cx = (float)INK_SCREEN_W * 0.5f;
    const HealthComp *boss_hp = game_health(g, g->run.boss_id);

    if (g->run.wave_index == GAME_BOSS_WAVE_INDEX && boss_hp != NULL && boss_hp->max_hp > 0) {
        const EnemyComp *ben = game_enemy(g, g->run.boss_id);
        const TextId name_id =
            ben != NULL ? text_boss_epithet(ben->def_id) : TEXT_BOSS_TENGU_EPITHET;
        const char *title =
            text_get(name_id != TEXT_NONE ? name_id : TEXT_BOSS_TENGU_EPITHET, r->use_ja);
        render_draw_text_centered(r, title, (Vector2){cx, 16.0f}, 20.0f, PAL_SUMI_SCORCH, 0.96f,
                                  false);

        const float bar_w = 340.0f;
        const float bar_x = cx - bar_w * 0.5f;
        const float ratio = ink_clampf((float)boss_hp->hp / (float)boss_hp->max_hp, 0.0f, 1.0f);
        ink_fill_panel(r, (Rectangle){bar_x, 32.0f, bar_w, 12.0f}, PAL_GOFUN, 0.90f, PAL_SUMI_DARK,
                       0.85f, 320u);
        if (ratio > 0.005f) {
            ink_fill_panel(r, (Rectangle){bar_x + 2.0f, 34.0f, (bar_w - 4.0f) * ratio, 8.0f},
                           PAL_BENGARA, 0.92f, PAL_BENGARA, 0.0f, 321u);
        }
        return;
    }

    char wave_buf[96];
    snprintf(wave_buf, sizeof wave_buf, "%s  %s %d/3", text_get(TEXT_HUD_ACT, r->use_ja),
             text_get(TEXT_HUD_WAVE, r->use_ja), g->run.wave_index + 1);
    render_draw_text_centered(r, wave_buf, (Vector2){cx - 72.0f, 26.0f}, 20.0f, PAL_SUMI_DARK,
                              0.95f, false);

    if (g->run.wave_quota > 0) {
        char q_buf[64];
        snprintf(q_buf, sizeof q_buf, "%s %2d/%2d", text_get(TEXT_HUD_QUOTA, r->use_ja),
                 g->run.wave_kills, g->run.wave_quota);
        render_draw_text_centered(r, q_buf, (Vector2){cx + 96.0f, 26.0f}, 20.0f, PAL_SUMI_SCORCH,
                                  0.95f, false);
    }
}

static void draw_score_and_chain(RenderState *r, const GameState *g)
{
    const float rx = (float)INK_SCREEN_W - 220.0f;
    render_draw_text_id(r, TEXT_HUD_SCORE, (Vector2){rx, 14.0f}, 20.0f, PAL_SUMI_DARK, 0.95f,
                        false);
    char sc_buf[32];
    snprintf(sc_buf, sizeof sc_buf, "%07d", g->run.score);
    render_draw_number_fixed(r, sc_buf, (Vector2){rx + 68.0f, 14.0f}, 20.0f, PAL_SUMI_SCORCH,
                             0.96f);

    if (g->run.chain >= 2 && g->run.chain_t > 0.0f) {
        char ch_buf[48];
        snprintf(ch_buf, sizeof ch_buf, "%s%d", text_get(TEXT_CALLOUT_CHAIN, r->use_ja),
                 g->run.chain);
        const Rectangle cbg = {rx - 10.0f, 60.0f, 172.0f, 40.0f};
        ink_fill_panel(r, cbg, PAL_GOFUN, 0.84f, PAL_SUMI_CLEAR, 0.0f, 349u);
        render_draw_text(r, ch_buf, (Vector2){rx, 64.0f}, 22.0f,
                         g->player.surge_t > 0.0f ? PAL_KIN : PAL_SUMI_SCORCH, 0.96f, false);
        const float gauge = ink_clampf(g->run.chain_t / 2.65f, 0.0f, 1.0f);
        const Vector2 gpts[2] = {{rx, 92.0f}, {rx + 150.0f * gauge, 92.0f}};
        ink_stroke(r, gpts, 2,
                   (BrushProfile){.w_max = 4.2f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.05f,
                                  .exit = 0.3f,
                                  .dry = 0.1f,
                                  .seed = 350u},
                   (InkTone){.color = g->player.surge_t > 0.0f ? PAL_KIN : PAL_SHU, .alpha = 0.90f},
                   1.0f);
    }
}

static void draw_cursor_ring_and_pips(RenderState *r, const GameState *g)
{
    if (g->phase == RUN_PHASE_REWARD || g->phase == RUN_PHASE_VICTORY ||
        g->phase == RUN_PHASE_GAME_OVER) {
        return;
    }

    const Vector2 c = {g->player.cursor_x, g->player.cursor_y};
    if (c.x < 0.0f || c.x > (float)INK_SCREEN_W || c.y < 0.0f || c.y > (float)INK_SCREEN_H) {
        return;
    }

    const float ink_ratio =
        g->player.ink_max > 0.0f ? ink_clampf(g->player.ink / g->player.ink_max, 0.0f, 1.0f) : 0.0f;
    const float dry_ratio =
        g->player.ink_max > 0.0f ? ink_clampf(20.0f / g->player.ink_max, 0.0f, 1.0f) : 0.2f;
    const float rad = 24.0f;
    const float start_a = -k_hud_pi * 0.5f;

    /* Subtle ring track */
    ink_arc_stroke(r, c, rad, start_a, start_a + k_hud_two_pi * 0.96f,
                   (BrushProfile){.w_max = 2.6f,
                                  .w_min_ratio = 0.4f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.2f,
                                  .seed = 360u},
                   (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.45f});

    /* Active ink gauge arc */
    if (ink_ratio > 0.01f) {
        const PaletteId ring_pal =
            g->player.surge_t > 0.0f ? PAL_KIN : (g->player.dry ? PAL_SHU : PAL_SUMI_DARK);
        ink_arc_stroke(r, c, rad, start_a, start_a + k_hud_two_pi * ink_ratio,
                       (BrushProfile){.w_max = 3.4f,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.12f,
                                      .exit = 0.20f,
                                      .dry = 0.0f,
                                      .seed = 361u},
                       (InkTone){.color = ring_pal, .alpha = 0.92f});
    }

    /* Dry-brush tick mark at ink = 20 */
    const float tick_a = start_a + k_hud_two_pi * dry_ratio;
    const Vector2 tick[2] = {
        {c.x + cosf(tick_a) * (rad - 4.5f), c.y + sinf(tick_a) * (rad - 4.5f)},
        {c.x + cosf(tick_a) * (rad + 4.5f), c.y + sinf(tick_a) * (rad + 4.5f)},
    };
    ink_stroke(r, tick, 2,
               (BrushProfile){.w_max = 2.2f,
                              .w_min_ratio = 0.4f,
                              .entry = 0.1f,
                              .exit = 0.1f,
                              .dry = 0.0f,
                              .seed = 362u},
               (InkTone){.color = PAL_SHU, .alpha = 0.85f}, 1.0f);

    /* 2 skill readiness pips below cursor: Cyclone (left) & Gale (right) */
    const float cds[2] = {g->player.cyclone_cd, g->player.gale_cd};
    const float totals[2] = {g->player.cyclone_cd_total > 0.0f ? g->player.cyclone_cd_total : 1.4f,
                             g->player.gale_cd_total > 0.0f ? g->player.gale_cd_total : 1.0f};
    const float x_offs[2] = {-11.0f, 11.0f};

    for (int s = 0; s < 2; ++s) {
        const float shake = r->skill_deny_t[s] > 0.0f ? sinf(r->vfx_time * 80.0f) * 3.5f : 0.0f;
        const Vector2 pp = {c.x + x_offs[s] + shake, c.y + rad + 13.0f};
        const bool ready = cds[s] <= 0.001f;
        const float fill = ink_clampf(1.0f - cds[s] / totals[s], 0.0f, 1.0f);
        const PaletteId pal = ready ? PAL_SUMI_DARK : PAL_SUMI_CLEAR;
        ink_teardrop(r, pp, (Vector2){0.0f, -1.0f}, 6.0f * (0.45f + 0.55f * fill),
                     5.5f * (0.45f + 0.55f * fill),
                     pal_color_alpha(r->skill_deny_t[s] > 0.0f ? PAL_SHU : pal, 0.92f));
    }
}

void render_draw_hud(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }
    if (ui != NULL && ui->scene == UI_SCENE_SHOWCASE &&
        (ui->showcase_page == 3 || ui->showcase_page == 4)) {
        return;
    }
    draw_top_band(r, g->run.act_index);
    draw_shrine_flames_and_ink(r, g);
    draw_center_progress(r, g);
    draw_score_and_chain(r, g);
    draw_cursor_ring_and_pips(r, g);
}
