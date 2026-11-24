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

    const float start_x = r->use_ja ? 54.0f : 92.0f;
    const int32_t shown = max_hp > 16 ? 16 : max_hp;
    const float step_x = shown > 12 ? 13.5f : 18.0f;
    for (int32_t i = 0; i < shown; ++i) {
        const Vector2 p = {start_x + (float)i * step_x, 26.0f};
        if (i < hp) {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, 11.0f, shown > 12 ? 5.6f : 7.0f,
                         pal_color_alpha(PAL_SHU, 0.92f));
            ink_teardrop(r, (Vector2){p.x, p.y + 1.0f}, (Vector2){0.0f, -1.0f}, 6.0f,
                         shown > 12 ? 3.0f : 3.8f, pal_color_alpha(PAL_SHU_HOT, 0.96f));
        } else {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, 9.0f, shown > 12 ? 4.4f : 5.5f,
                         pal_color_alpha(PAL_SUMI_CLEAR, 0.55f));
        }
    }

    const float ink_x = start_x + (float)shown * step_x + 16.0f;
    render_draw_text_id(r, TEXT_HUD_INK, (Vector2){ink_x, 14.0f}, 20.0f,
                        g->player.dry ? PAL_SHU : PAL_SUMI_DARK, 0.95f, false);
    char ink_buf[32];
    snprintf(ink_buf, sizeof ink_buf, "%3d/%3d", (int)lroundf(g->player.ink),
             (int)lroundf(g->player.ink_max));
    render_draw_number_fixed(r, ink_buf, (Vector2){ink_x + 42.0f, 14.0f}, 20.0f,
                             g->player.surge_t > 0.0f ? PAL_KIN : PAL_SUMI_SCORCH, 0.95f);
}

/* The kill quota is the wave's win condition, so it gets a bar and an explicit countdown rather
 * than a bare counter: when the last foes vanish into the purification the player should already
 * know why. */
static void draw_quota_progress(RenderState *r, const GameState *g, float cx)
{
    const int32_t quota = g->run.wave_quota;
    const int32_t kills = g->run.wave_kills < quota ? g->run.wave_kills : quota;
    const int32_t left = quota - kills;
    const bool done = left <= 0;
    const bool close = !done && left <= 2;
    const float pulse = close ? 0.5f + 0.5f * sinf(r->vfx_time * 9.0f) : 0.0f;
    const PaletteId accent =
        done ? PAL_KIN : (close ? (pulse > 0.5f ? PAL_KIN : PAL_SHU) : PAL_SHU);

    char q_buf[64];
    snprintf(q_buf, sizeof q_buf, "%s %2d/%2d", text_get(TEXT_HUD_QUOTA, r->use_ja), kills, quota);
    render_draw_text_centered(r, q_buf, (Vector2){cx + 96.0f, 22.0f}, 20.0f, PAL_SUMI_SCORCH, 0.95f,
                              false);

    const float bar_x = cx + 20.0f;
    const float bar_w = 152.0f;
    const float bar_y = 43.0f;
    const Vector2 track[2] = {{bar_x, bar_y}, {bar_x + bar_w, bar_y}};
    ink_stroke(r, track, 2,
               (BrushProfile){.w_max = 5.0f,
                              .w_min_ratio = 0.5f,
                              .entry = 0.05f,
                              .exit = 0.1f,
                              .dry = 0.35f,
                              .seed = 342u},
               (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.55f}, 1.0f);
    const float ratio = quota > 0 ? (float)kills / (float)quota : 0.0f;
    if (ratio > 0.01f) {
        const Vector2 fill[2] = {{bar_x, bar_y}, {bar_x + bar_w * ratio, bar_y}};
        ink_stroke(r, fill, 2,
                   (BrushProfile){.w_max = 5.0f + 2.0f * pulse,
                                  .w_min_ratio = 0.45f,
                                  .entry = 0.05f,
                                  .exit = 0.25f,
                                  .dry = 0.15f,
                                  .seed = 343u},
                   (InkTone){.color = accent, .alpha = 0.95f}, 1.0f);
    }
    if (!done) {
        char left_buf[32];
        if (r->use_ja) {
            snprintf(left_buf, sizeof left_buf, "%s%d", text_get(TEXT_HUD_REMAINING, true), left);
        } else {
            snprintf(left_buf, sizeof left_buf, "%d %s", left, text_get(TEXT_HUD_REMAINING, false));
        }
        render_draw_text(r, left_buf, (Vector2){bar_x + bar_w + 10.0f, 34.0f}, 18.0f,
                         close ? accent : PAL_SUMI_HEAVY, 0.95f, false);
    }
}

static void draw_center_progress(RenderState *r, const GameState *g)
{
    const float cx = (float)INK_SCREEN_W * 0.5f;
    const HealthComp *boss_hp = game_health(g, g->run.boss_id);

    if (g->run.wave_index == GAME_BOSS_WAVE_INDEX && boss_hp != NULL && boss_hp->max_hp > 0) {
        const EnemyComp *ben = game_enemy(g, g->run.boss_id);
        const BossComp *bbc = game_boss(g, g->run.boss_id);
        const TextId name_id =
            ben != NULL ? text_boss_epithet(ben->def_id) : TEXT_BOSS_TENGU_EPITHET;
        const char *title =
            text_get(name_id != TEXT_NONE ? name_id : TEXT_BOSS_TENGU_EPITHET, r->use_ja);

        const bool is_multipart_ryu = (g->run.boss_id.index != ECS_INVALID_INDEX &&
                                       g->ryu_body.head_id.index == g->run.boss_id.index &&
                                       g->ryu_body.head_id.gen == g->run.boss_id.gen);

        int32_t cur_hp = boss_hp->hp > 0 ? boss_hp->hp : 0;
        int32_t total_max = boss_hp->max_hp;
        if (is_multipart_ryu) {
            const int32_t orb_fallback_max = boss_hp->max_hp / 3 > 0 ? boss_hp->max_hp / 3 : 300;
            for (int k = 0; k < GAME_RYU_ORB_COUNT; ++k) {
                const bool broken = (g->ryu_body.orb_broken_mask & (1u << (unsigned)k)) != 0u;
                const HealthComp *ohp = game_health(g, g->ryu_body.orb_ids[k]);
                if (!broken && ohp != NULL && ohp->max_hp > 0) {
                    cur_hp += ohp->hp > 0 ? ohp->hp : 0;
                    total_max += ohp->max_hp;
                } else {
                    total_max += orb_fallback_max;
                }
            }
        }

        render_draw_text_centered(r, title, (Vector2){cx, is_multipart_ryu ? 14.0f : 16.0f}, 20.0f,
                                  PAL_SUMI_SCORCH, 0.96f, false);

        const float bar_w = 340.0f;
        const float bar_x = cx - bar_w * 0.5f;
        const float bar_y = is_multipart_ryu ? 28.0f : 32.0f;
        const float bar_h = is_multipart_ryu ? 10.0f : 12.0f;
        const float ratio =
            total_max > 0 ? ink_clampf((float)cur_hp / (float)total_max, 0.0f, 1.0f) : 0.0f;
        const PaletteId border_pal =
            (is_multipart_ryu && bbc != NULL && bbc->phase_idx >= 2)
                ? PAL_KIN
                : ((is_multipart_ryu && bbc != NULL && bbc->phase_idx == 1) ? PAL_SHU
                                                                            : PAL_SUMI_DARK);
        ink_fill_panel(r, (Rectangle){bar_x, bar_y, bar_w, bar_h}, PAL_GOFUN, 0.90f, border_pal,
                       0.88f, 320u);
        if (ratio > 0.005f) {
            ink_fill_panel(
                r, (Rectangle){bar_x + 2.0f, bar_y + 2.0f, (bar_w - 4.0f) * ratio, bar_h - 4.0f},
                PAL_BENGARA, 0.92f, PAL_BENGARA, 0.0f, 321u);
        }

        /* 3 Dragon Orb status marks directly beneath HP bar (Fire=Shu, Storm=Ao, Wind=Gofun) */
        if (is_multipart_ryu) {
            static const PaletteId k_orb_pal[GAME_RYU_ORB_COUNT] = {PAL_SHU, PAL_AO, PAL_GOFUN};
            static const PaletteId k_orb_hot[GAME_RYU_ORB_COUNT] = {PAL_SHU_HOT, PAL_AO_HOT,
                                                                    PAL_SUMI_LIGHT};
            const float orb_y = 47.0f;
            for (int k = 0; k < GAME_RYU_ORB_COUNT; ++k) {
                const Vector2 op = {cx + (float)(k - 1) * 32.0f, orb_y};
                const bool broken = (g->ryu_body.orb_broken_mask & (1u << (unsigned)k)) != 0u;
                if (broken) {
                    ink_arc_stroke(r, op, 6.2f, 0.4f, 4.9f,
                                   (BrushProfile){.w_max = 2.2f,
                                                  .w_min_ratio = 0.35f,
                                                  .entry = 0.12f,
                                                  .exit = 0.30f,
                                                  .dry = 0.45f,
                                                  .seed = 325u + (uint32_t)k},
                                   (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.85f});
                } else {
                    const bool active_orb = (g->ryu_body.active_orb_slot == k);
                    if (active_orb) {
                        ink_arc_stroke(r, op, 8.8f, 0.0f, k_hud_two_pi + 0.1f,
                                       (BrushProfile){.w_max = 2.0f,
                                                      .w_min_ratio = 0.40f,
                                                      .entry = 0.10f,
                                                      .exit = 0.10f,
                                                      .dry = 0.0f,
                                                      .seed = 328u + (uint32_t)k},
                                       (InkTone){.color = PAL_KIN, .alpha = 0.94f});
                    }
                    ink_teardrop(r, op, (Vector2){0.0f, -1.0f}, 8.0f, 6.8f,
                                 pal_color_alpha(k_orb_pal[k], 0.95f));
                    ink_teardrop(r, op, (Vector2){0.0f, -1.0f}, 4.2f, 3.6f,
                                 pal_color_alpha(k_orb_hot[k], 0.95f));
                    if (k == 2) {
                        ink_arc_stroke(r, op, 5.5f, 0.2f, 5.4f,
                                       (BrushProfile){.w_max = 1.6f,
                                                      .w_min_ratio = 0.40f,
                                                      .entry = 0.10f,
                                                      .exit = 0.20f,
                                                      .dry = 0.10f,
                                                      .seed = 331u},
                                       (InkTone){.color = PAL_SUMI_DARK, .alpha = 0.88f});
                    }
                }
            }
        }
        return;
    }

    char act_buf[48];
    const char *act_str = NULL;
    if (g->run.act_index >= GAME_MAIN_ACT_COUNT) {
        if (r->use_ja) {
            snprintf(act_buf, sizeof act_buf, "%s%d%s", text_get(TEXT_ENDLESS_ACT_PREFIX, true),
                     g->run.act_index + 1, text_get(TEXT_ENDLESS_ACT_SUFFIX, true));
        } else {
            snprintf(act_buf, sizeof act_buf, "Act %d", g->run.act_index + 1);
        }
        act_str = act_buf;
    } else {
        const TextId act_id = text_act_short(g->run.act_index);
        act_str = text_get(act_id != TEXT_NONE ? act_id : TEXT_ACT1_SHORT, r->use_ja);
    }

    char wave_buf[96];
    if (g->run.wave_index >= GAME_BOSS_WAVE_INDEX) {
        snprintf(wave_buf, sizeof wave_buf, "%s  %s", act_str, text_get(TEXT_HUD_BOSS, r->use_ja));
        render_draw_text_centered(r, wave_buf, (Vector2){cx, 26.0f}, 20.0f, PAL_SUMI_DARK, 0.95f,
                                  false);
        return;
    }
    snprintf(wave_buf, sizeof wave_buf, "%s  %s %d/3", act_str, text_get(TEXT_HUD_WAVE, r->use_ja),
             g->run.wave_index + 1);
    render_draw_text_centered(r, wave_buf, (Vector2){cx - 72.0f, 26.0f}, 20.0f, PAL_SUMI_DARK,
                              0.95f, false);

    if (g->run.wave_quota > 0) {
        draw_quota_progress(r, g, cx);
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
        static const float k_chain_grace[4] = {2.65f, 3.20f, 3.80f, 4.50f};
        uint8_t lv = game_upgrade_level(g, UPG_CHAIN_PRIMER);
        if (lv > 3u) {
            lv = 3u;
        }
        char ch_buf[48];
        snprintf(ch_buf, sizeof ch_buf, "%s%d", text_get(TEXT_CALLOUT_CHAIN, r->use_ja),
                 g->run.chain);
        const Rectangle cbg = {rx - 10.0f, 60.0f, 172.0f, 40.0f};
        ink_fill_panel(r, cbg, PAL_GOFUN, 0.84f, PAL_SUMI_CLEAR, 0.0f, 349u);
        render_draw_text_wrapped(r, ch_buf, (Rectangle){rx, 62.0f, cbg.width - 20.0f, 26.0f}, 22.0f,
                                 15.0f, g->player.surge_t > 0.0f ? PAL_KIN : PAL_SUMI_SCORCH, 0.96f,
                                 false, false);
        const float gauge = ink_clampf(g->run.chain_t / k_chain_grace[lv], 0.0f, 1.0f);
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

static void draw_owned_relics_hud(RenderState *r, const GameState *g)
{
    if (g->run.relic_count <= 0) {
        return;
    }
    const int32_t count =
        g->run.relic_count > GAME_MAX_OWNED_RELICS ? GAME_MAX_OWNED_RELICS : g->run.relic_count;
    const float tile_w = 112.0f;
    const float tile_h = 28.0f;
    const float gap = 8.0f;
    const float y = (float)INK_SCREEN_H - 42.0f;

    for (int32_t i = 0; i < count && i < 6; ++i) {
        const RelicId rid = g->run.relics[i];
        const float x = (float)INK_SCREEN_W - 18.0f - (float)(i + 1) * (tile_w + gap) + gap;
        const Rectangle box = {x, y, tile_w, tile_h};
        ink_fill_panel(r, box, PAL_GOFUN, 0.86f, PAL_KIN, 0.85f, 380u + (uint32_t)i);
        const char *name = text_get(text_relic_name(rid), r->use_ja);
        render_draw_text_fit(r, name, (Vector2){x + tile_w * 0.5f, y + tile_h * 0.5f},
                             tile_w - 12.0f, 18.0f, 12.0f, PAL_SUMI_SCORCH, 0.94f, false);
    }
}

static PaletteId brush_accent(BrushStyleId style)
{
    switch (style) {
    case BRUSH_STYLE_EMBER:
        return PAL_SHU;
    case BRUSH_STYLE_QUILL:
        return PAL_AO;
    case BRUSH_STYLE_HEAVY:
    default:
        return PAL_SUMI_SCORCH;
    }
}

static void draw_brush_badge(RenderState *r, const GameState *g)
{
    const BrushStyleId style = g->config.brush_style;
    const PaletteId accent = brush_accent(style);
    const Rectangle box = {18.0f, (float)INK_SCREEN_H - 46.0f, 184.0f, 32.0f};
    const PaletteId border =
        g->player.surge_t > 0.0f ? PAL_KIN : (g->player.dry ? PAL_SHU : accent);
    ink_fill_panel(r, box, PAL_GOFUN, 0.86f, border, 0.88f, 390u);
    const Vector2 mark = {box.x + 18.0f, box.y + box.height * 0.5f};
    if (style == BRUSH_STYLE_HEAVY) {
        ink_teardrop(r, mark, (Vector2){-0.6f, 0.8f}, 11.0f, 9.0f,
                     pal_color_alpha(PAL_SUMI_SCORCH, 0.92f));
    } else if (style == BRUSH_STYLE_EMBER) {
        ink_teardrop(r, mark, (Vector2){0.0f, -1.0f}, 11.0f, 6.5f, pal_color_alpha(PAL_SHU, 0.94f));
        ink_teardrop(r, mark, (Vector2){0.0f, -1.0f}, 5.5f, 3.4f,
                     pal_color_alpha(PAL_SHU_HOT, 0.96f));
    } else {
        ink_teardrop(r, mark, (Vector2){-0.6f, 0.8f}, 12.0f, 3.8f,
                     pal_color_alpha(PAL_SUMI_HEAVY, 0.92f));
        ink_teardrop(r, (Vector2){mark.x - 3.0f, mark.y + 4.0f}, (Vector2){-0.6f, 0.8f}, 5.0f, 2.4f,
                     pal_color_alpha(PAL_AO_HOT, 0.96f));
    }
    render_draw_text_fit(r, text_get(text_brush_name(style), r->use_ja),
                         (Vector2){box.x + 106.0f, box.y + box.height * 0.5f}, box.width - 48.0f,
                         18.0f, 13.0f, accent, 0.96f, false);
}

void render_draw_brush_cursor(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL || g->phase == RUN_PHASE_REWARD || g->phase == RUN_PHASE_VICTORY ||
        g->phase == RUN_PHASE_GAME_OVER) {
        return;
    }

    const Vector2 c = {g->player.cursor_x, g->player.cursor_y};
    if (c.x < 0.0f || c.x > (float)INK_SCREEN_W || c.y < 0.0f || c.y > (float)INK_SCREEN_H) {
        return;
    }

    const float cs = (ui != NULL && ui->cursor_scale >= 100 && ui->cursor_scale <= 150)
                         ? ((float)ui->cursor_scale * 0.01f)
                         : 1.0f;

    const BrushStyleId style = g->config.brush_style;
    const float ink_ratio =
        g->player.ink_max > 0.0f ? ink_clampf(g->player.ink / g->player.ink_max, 0.0f, 1.0f) : 0.0f;
    const float dry_ratio =
        g->player.ink_max > 0.0f ? ink_clampf(20.0f / g->player.ink_max, 0.0f, 1.0f) : 0.2f;
    const float base_rad =
        style == BRUSH_STYLE_HEAVY ? 26.0f : (style == BRUSH_STYLE_QUILL ? 21.0f : 24.0f);
    const float rad = base_rad * cs;
    const float start_a = -k_hud_pi * 0.5f;

    /* Pressing the brush flattens and widens the nib like a real tip on paper. */
    const float press =
        g->player.painting ? ink_clampf(g->player.stroke_speed / 900.0f, 0.25f, 1.0f) : 0.0f;
    const float nl = cs * (1.0f + 0.18f * press);
    const float nw = cs * (1.0f + 0.45f * press);
    const Vector2 tilt = {-0.6f, 0.8f};
    if (style == BRUSH_STYLE_HEAVY) {
        ink_teardrop(r, c, tilt, 12.0f * nl, 9.5f * nw, pal_color_alpha(PAL_SUMI_SCORCH, 0.90f));
        ink_teardrop(r, (Vector2){c.x + 1.5f * cs, c.y - 1.5f * cs}, tilt, 6.0f * nl, 4.5f * nw,
                     pal_color_alpha(PAL_SUMI_DARK, 0.92f));
    } else if (style == BRUSH_STYLE_EMBER) {
        const float flick = 1.0f + 0.12f * sinf(r->vfx_time * 18.0f);
        ink_teardrop(r, c, (Vector2){0.0f, -1.0f}, 12.0f * nl * flick, 6.8f * nw,
                     pal_color_alpha(PAL_SHU, 0.92f));
        ink_teardrop(r, (Vector2){c.x, c.y + 1.0f * cs}, (Vector2){0.0f, -1.0f}, 6.0f * nl,
                     3.6f * nw, pal_color_alpha(PAL_SHU_HOT, 0.96f));
        ink_teardrop(r, (Vector2){c.x + 5.0f * cs, c.y - 9.0f * cs}, (Vector2){0.3f, -1.0f},
                     3.0f * cs, 1.8f * cs, pal_color_alpha(PAL_KIN_HOT, 0.90f));
    } else {
        ink_teardrop(r, c, tilt, 13.0f * nl, 3.6f * nw, pal_color_alpha(PAL_SUMI_HEAVY, 0.90f));
        ink_teardrop(r, (Vector2){c.x - 3.0f * cs, c.y + 4.0f * cs}, tilt, 5.0f * nl, 2.2f * nw,
                     pal_color_alpha(PAL_AO_HOT, 0.96f));
    }

    ink_arc_stroke(r, c, rad, start_a, start_a + k_hud_two_pi * 0.96f,
                   (BrushProfile){.w_max = 2.6f * cs,
                                  .w_min_ratio = 0.4f,
                                  .entry = 0.1f,
                                  .exit = 0.1f,
                                  .dry = 0.2f,
                                  .seed = 360u},
                   (InkTone){.color = PAL_SUMI_CLEAR, .alpha = 0.45f});
    /* Surge/Dry recolor the ink ring, so the brush accent lives on a thin outer arc that never
     * changes color. */
    ink_arc_stroke(r, c, rad + 4.5f * cs, start_a - 0.5f, start_a + 0.5f,
                   (BrushProfile){.w_max = 2.2f * cs,
                                  .w_min_ratio = 0.4f,
                                  .entry = 0.15f,
                                  .exit = 0.15f,
                                  .dry = 0.1f,
                                  .seed = 363u},
                   (InkTone){.color = brush_accent(style), .alpha = 0.92f});

    if (ink_ratio > 0.01f) {
        PaletteId ring_pal = PAL_SUMI_DARK;
        if (g->player.surge_t > 0.0f) {
            ring_pal = PAL_KIN;
        } else if (g->player.dry) {
            ring_pal = PAL_SHU;
        } else if (style == BRUSH_STYLE_EMBER) {
            ring_pal = PAL_BENGARA;
        } else if (style == BRUSH_STYLE_QUILL) {
            ring_pal = PAL_AO;
        }
        ink_arc_stroke(r, c, rad, start_a, start_a + k_hud_two_pi * ink_ratio,
                       (BrushProfile){.w_max = (style == BRUSH_STYLE_HEAVY ? 4.2f : 3.2f) * cs,
                                      .w_min_ratio = 0.35f,
                                      .entry = 0.12f,
                                      .exit = 0.20f,
                                      .dry = 0.0f,
                                      .seed = 361u},
                       (InkTone){.color = ring_pal, .alpha = 0.92f});
    }

    const float tick_a = start_a + k_hud_two_pi * dry_ratio;
    const Vector2 tick[2] = {
        {c.x + cosf(tick_a) * (rad - 4.5f * cs), c.y + sinf(tick_a) * (rad - 4.5f * cs)},
        {c.x + cosf(tick_a) * (rad + 4.5f * cs), c.y + sinf(tick_a) * (rad + 4.5f * cs)},
    };
    ink_stroke(r, tick, 2,
               (BrushProfile){.w_max = 2.2f * cs,
                              .w_min_ratio = 0.4f,
                              .entry = 0.1f,
                              .exit = 0.1f,
                              .dry = 0.0f,
                              .seed = 362u},
               (InkTone){.color = PAL_SHU, .alpha = 0.85f}, 1.0f);

    const float cyc_total = g->player.cyclone_cd_total > 0.0f ? g->player.cyclone_cd_total : 1.4f;
    const float gale_total = g->player.gale_cd_total > 0.0f ? g->player.gale_cd_total : 1.0f;
    const bool double_gale = g->player.gale_max_charges >= 2;
    const float pip_y = c.y + rad + 13.0f * cs;

    /* Cyclone readiness pip (left) */
    const float cyc_shake =
        r->skill_deny_t[SKILL_CYCLONE] > 0.0f ? sinf(r->vfx_time * 80.0f) * 3.5f * cs : 0.0f;
    const float cyc_x = c.x + (double_gale ? -14.0f : -11.0f) * cs + cyc_shake;
    const bool cyc_ready = g->player.cyclone_cd <= 0.001f;
    const float cyc_fill = ink_clampf(1.0f - g->player.cyclone_cd / cyc_total, 0.0f, 1.0f);
    const PaletteId cyc_pal = r->skill_deny_t[SKILL_CYCLONE] > 0.0f
                                  ? PAL_SHU
                                  : (cyc_ready ? PAL_SUMI_DARK : PAL_SUMI_CLEAR);
    ink_teardrop(r, (Vector2){cyc_x, pip_y}, (Vector2){0.0f, -1.0f},
                 6.0f * cs * (0.45f + 0.55f * cyc_fill), 5.5f * cs * (0.45f + 0.55f * cyc_fill),
                 pal_color_alpha(cyc_pal, 0.92f));

    /* Gale readiness pip(s) (right: 1 pip normal, 2 pips with Double Gale Lv3) */
    const float gale_shake =
        r->skill_deny_t[SKILL_GALE] > 0.0f ? sinf(r->vfx_time * 80.0f) * 3.5f * cs : 0.0f;
    if (double_gale) {
        const float gx_offs[2] = {5.0f * cs, 17.0f * cs};
        const int32_t charges = g->player.gale_charges;
        for (int k = 0; k < 2; ++k) {
            const bool ready = (k < charges);
            const bool recharging = (k == charges);
            const float fill =
                ready ? 1.0f
                      : (recharging ? ink_clampf(1.0f - g->player.gale_cd / gale_total, 0.0f, 1.0f)
                                    : 0.0f);
            const PaletteId gpal = r->skill_deny_t[SKILL_GALE] > 0.0f
                                       ? PAL_SHU
                                       : (ready ? PAL_SUMI_DARK : PAL_SUMI_CLEAR);
            ink_teardrop(r, (Vector2){c.x + gx_offs[k] + gale_shake, pip_y}, (Vector2){0.0f, -1.0f},
                         5.8f * cs * (0.45f + 0.55f * fill), 5.2f * cs * (0.45f + 0.55f * fill),
                         pal_color_alpha(gpal, 0.92f));
        }
    } else {
        const bool gale_ready = g->player.gale_cd <= 0.001f;
        const float gale_fill = ink_clampf(1.0f - g->player.gale_cd / gale_total, 0.0f, 1.0f);
        const PaletteId gpal = r->skill_deny_t[SKILL_GALE] > 0.0f
                                   ? PAL_SHU
                                   : (gale_ready ? PAL_SUMI_DARK : PAL_SUMI_CLEAR);
        ink_teardrop(r, (Vector2){c.x + 11.0f * cs + gale_shake, pip_y}, (Vector2){0.0f, -1.0f},
                     6.0f * cs * (0.45f + 0.55f * gale_fill),
                     5.5f * cs * (0.45f + 0.55f * gale_fill), pal_color_alpha(gpal, 0.92f));
    }
}

void render_draw_hud(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }
    if (ui != NULL) {
        if (ui->scene == UI_SCENE_TITLE_LOBBY || ui->scene == UI_SCENE_BRUSH_SELECT ||
            ui->scene == UI_SCENE_BOKUAN || ui->scene == UI_SCENE_SETTINGS) {
            return;
        }
        if (ui->scene == UI_SCENE_SHOWCASE && ui->showcase_page >= 3) {
            return;
        }
    }
    draw_top_band(r, g->run.act_index);
    draw_shrine_flames_and_ink(r, g);
    draw_center_progress(r, g);
    draw_score_and_chain(r, g);
    if (ui == NULL || ui->scene != UI_SCENE_SHOWCASE || ui->showcase_page != 1) {
        draw_owned_relics_hud(r, g);
    }
    if (ui == NULL || ui->scene != UI_SCENE_SHOWCASE) {
        draw_brush_badge(r, g);
    }
    if (ui == NULL || !ui->paused) {
        render_draw_brush_cursor(r, g, ui);
    }
}
