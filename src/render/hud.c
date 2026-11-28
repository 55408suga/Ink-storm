#include "ink_prims.h"
#include "player/player_skills.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>

static const float k_hud_pi = 3.14159265359f;
static const float k_hud_two_pi = 6.28318530718f;

static void draw_filled_rect(RenderState *r, Rectangle rc, Color c)
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

static void draw_framed_bar(RenderState *r, Rectangle rc, float ratio, PaletteId frame_pal,
                            float frame_alpha, PaletteId track_pal, float track_alpha,
                            PaletteId fill_pal, float fill_alpha)
{
    draw_filled_rect(r, rc, pal_color_alpha(frame_pal, frame_alpha));
    const Rectangle inner = {rc.x + 1.5f, rc.y + 1.5f, rc.width - 3.0f, rc.height - 3.0f};
    draw_filled_rect(r, inner, pal_color_alpha(track_pal, track_alpha));
    const float clamped = ink_clampf(ratio, 0.0f, 1.0f);
    if (clamped > 0.005f) {
        const Rectangle fill = {inner.x, inner.y, inner.width * clamped, inner.height};
        draw_filled_rect(r, fill, pal_color_alpha(fill_pal, fill_alpha));
    }
}

static void format_act_tag(const RenderState *r, int32_t act_index, char *out, size_t cap)
{
    if (act_index >= GAME_MAIN_ACT_COUNT) {
        const int32_t loop_n = act_index / GAME_MAIN_ACT_COUNT;
        if (r->use_ja) {
            snprintf(out, cap, "%s%d%s(%s%d)", text_get(TEXT_ENDLESS_ACT_PREFIX, true),
                     act_index + 1, text_get(TEXT_ENDLESS_ACT_SUFFIX, true),
                     text_get(TEXT_HUD_LOOP_SHORT, true), loop_n);
        } else {
            snprintf(out, cap, "Act %d (%s%d)", act_index + 1, text_get(TEXT_HUD_LOOP_SHORT, false),
                     loop_n);
        }
        return;
    }
    const TextId act_id = text_act_short(act_index);
    snprintf(out, cap, "%s", text_get(act_id != TEXT_NONE ? act_id : TEXT_ACT1_SHORT, r->use_ja));
}

static void draw_top_band(RenderState *r, int act_index)
{
    const Rectangle band = {0.0f, 0.0f, (float)INK_SCREEN_W, 60.0f};
    ink_fill_panel(r, band, pal_paper_for_act(act_index), 0.86f, PAL_SUMI_CLEAR, 0.0f, 300u);
    const Vector2 edge[3] = {
        {16.0f, 60.0f},
        {(float)INK_SCREEN_W * 0.5f, 61.0f},
        {(float)INK_SCREEN_W - 16.0f, 59.5f},
    };
    const BrushProfile ep = {.w_max = 2.8f,
                             .w_min_ratio = 0.35f,
                             .entry = 0.08f,
                             .exit = 0.14f,
                             .dry = 0.50f,
                             .seed = 301u};
    ink_stroke(r, edge, 3, ep, (InkTone){.color = PAL_SUMI_LIGHT, .alpha = 0.68f}, 1.0f);
}

static void draw_shrine_flames_and_ink(RenderState *r, const GameState *g)
{
    const int32_t hp = g->shrine_count > 0 ? g->shrines[0].hp : 0;
    const int32_t max_hp = g->shrine_count > 0 ? g->shrines[0].max_hp : 8;

    render_draw_text_id(r, TEXT_HUD_SHRINE, (Vector2){16.0f, 18.0f}, 19.0f, PAL_SUMI_DARK, 0.96f,
                        false);

    const float start_x = r->use_ja ? 46.0f : 80.0f;
    const int32_t shown = max_hp > 16 ? 16 : max_hp;
    const float max_flames_w = r->use_ja ? 156.0f : 122.0f;
    const float step_x = shown > 0 ? fminf(15.0f, max_flames_w / (float)shown) : 15.0f;
    const float flame_w = step_x < 11.5f ? 4.8f : (step_x < 13.5f ? 5.6f : 6.4f);
    const float flame_h = step_x < 11.5f ? 9.5f : 11.0f;
    for (int32_t i = 0; i < shown; ++i) {
        const Vector2 p = {start_x + (float)i * step_x, 29.0f};
        if (i < hp) {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, flame_h, flame_w,
                         pal_color_alpha(PAL_SHU, 0.94f));
            ink_teardrop(r, (Vector2){p.x, p.y + 1.0f}, (Vector2){0.0f, -1.0f}, flame_h * 0.54f,
                         flame_w * 0.54f, pal_color_alpha(PAL_SHU_HOT, 0.96f));
        } else {
            ink_teardrop(r, p, (Vector2){0.0f, -1.0f}, flame_h * 0.80f, flame_w * 0.78f,
                         pal_color_alpha(PAL_SUMI_CLEAR, 0.58f));
        }
    }

    char hp_buf[24];
    snprintf(hp_buf, sizeof hp_buf, "%d/%d", hp, max_hp);
    const float hp_x = start_x + (float)shown * step_x + 4.0f;
    const PaletteId hp_pal = hp <= 3 ? PAL_BENGARA : PAL_SUMI_SCORCH;
    render_draw_number_fixed(r, hp_buf, (Vector2){hp_x, 19.0f}, 18.0f, hp_pal, 0.96f);

    const float ink_x = 264.0f;
    const float label_w = r->use_ja ? 24.0f : 32.0f;
    const float num_x = ink_x + label_w;
    const bool surging = g->player.surge_t > 0.0f;
    char ink_buf[32];
    snprintf(ink_buf, sizeof ink_buf, "%3d/%3d", (int)lroundf(g->player.ink),
             (int)lroundf(g->player.ink_max));

    if (surging) {
        const Rectangle badge = {ink_x - 6.0f, 7.0f, label_w + 94.0f, 25.0f};
        ink_fill_panel(r, badge, PAL_SUMI_SCORCH, 0.94f, PAL_KIN, 0.95f, 308u);
        render_draw_text_id(r, TEXT_HUD_INK, (Vector2){ink_x, 11.0f}, 17.0f, PAL_KIN_HOT, 0.98f,
                            false);
        render_draw_number_fixed(r, ink_buf, (Vector2){num_x, 11.0f}, 17.0f, PAL_KIN_HOT, 0.98f);
    } else {
        render_draw_text_id(r, TEXT_HUD_INK, (Vector2){ink_x, 11.0f}, 17.0f,
                            g->player.dry ? PAL_SHU : PAL_SUMI_DARK, 0.96f, false);
        render_draw_number_fixed(r, ink_buf, (Vector2){num_x, 11.0f}, 17.0f,
                                 g->player.dry ? PAL_SHU : PAL_SUMI_SCORCH, 0.96f);
    }

    const float ink_ratio =
        g->player.ink_max > 0.0f ? ink_clampf(g->player.ink / g->player.ink_max, 0.0f, 1.0f) : 0.0f;
    const Rectangle bar_rc = {num_x, 38.0f, 88.0f, 7.0f};
    draw_framed_bar(r, bar_rc, ink_ratio, surging ? PAL_KIN : PAL_SUMI_DARK, 0.92f,
                    surging ? PAL_SUMI_SCORCH : PAL_GOFUN, 0.94f,
                    surging ? PAL_KIN_HOT : (g->player.dry ? PAL_SHU : PAL_SUMI_SCORCH), 0.96f);
    if (g->player.ink_max > 20.0f) {
        const float dry_x =
            bar_rc.x + 1.5f +
            (bar_rc.width - 3.0f) * ink_clampf(20.0f / g->player.ink_max, 0.0f, 1.0f);
        draw_filled_rect(r, (Rectangle){dry_x - 0.75f, bar_rc.y - 1.0f, 1.5f, bar_rc.height + 2.0f},
                         pal_color_alpha(PAL_SHU, 0.90f));
    }
}

/* The kill quota is the wave's win condition, so it sits inside a high-contrast dark lacquer
 * focal plaque that remains unmistakable in peripheral vision. */
static void draw_objective_plaque(RenderState *r, const GameState *g, Rectangle box)
{
    const int32_t quota = g->run.wave_quota;
    const int32_t kills = g->run.wave_kills < quota ? g->run.wave_kills : quota;
    const int32_t left = quota > kills ? quota - kills : 0;
    const bool done = quota > 0 && left <= 0;
    const bool close = !done && quota > 0 && left <= 3;
    const float pulse = close ? (0.5f + 0.5f * sinf(r->vfx_time * 9.0f)) : 0.0f;

    const PaletteId border_pal =
        done ? PAL_KIN : (close ? (pulse > 0.45f ? PAL_KIN_HOT : PAL_KIN) : PAL_SHU);
    ink_fill_panel(r, box, PAL_SUMI_SCORCH, 0.94f, border_pal, 0.96f, 340u);

    char focal_buf[48];
    if (done) {
        snprintf(focal_buf, sizeof focal_buf, "%s", text_get(TEXT_HUD_WAVE_CLEAR_SHORT, r->use_ja));
    } else if (r->use_ja) {
        snprintf(focal_buf, sizeof focal_buf, "%s %d %s", text_get(TEXT_HUD_REMAINING, true), left,
                 text_get(TEXT_HUD_REMAINING_UNIT, true));
    } else {
        snprintf(focal_buf, sizeof focal_buf, "%d %s", left,
                 text_get(TEXT_HUD_REMAINING_UNIT, false));
    }
    const PaletteId focal_pal =
        done ? PAL_KIN_HOT : (close && pulse > 0.40f ? PAL_KIN_HOT : PAL_GOFUN);
    render_draw_text_fit(r, focal_buf, (Vector2){box.x + 76.0f, box.y + 21.0f}, 134.0f, 22.0f,
                         16.0f, focal_pal, 0.99f, false);

    char ratio_buf[24];
    snprintf(ratio_buf, sizeof ratio_buf, "%d/%d", kills, quota);
    const float rw = render_measure_number_fixed(r, ratio_buf, 16.0f);
    render_draw_number_fixed(r, ratio_buf, (Vector2){box.x + box.width - 14.0f - rw, box.y + 13.0f},
                             16.0f, (close || done) ? PAL_GOFUN : PAL_SUMI_CLEAR, 0.95f);

    const float ratio = quota > 0 ? (float)kills / (float)quota : 0.0f;
    const Rectangle bar_rc = {box.x + 14.0f, box.y + 42.0f, box.width - 28.0f, 8.0f};
    draw_framed_bar(r, bar_rc, ratio, PAL_SUMI_CLEAR, 0.72f, PAL_SUMI_HEAVY, 0.65f,
                    (close || done) ? PAL_KIN_HOT : PAL_SHU, 0.98f);
}

static void draw_stage_plaque(RenderState *r, const GameState *g, Rectangle box)
{
    ink_fill_panel(r, box, PAL_GOFUN, 0.94f, PAL_SUMI_DARK, 0.88f, 332u);

    const int32_t stage_act = g->run.act_index >= 0 ? (g->run.act_index % GAME_MAIN_ACT_COUNT) : 0;
    char act_tag[48];
    format_act_tag(r, g->run.act_index, act_tag, sizeof act_tag);
    const TextId sub_id = text_act_subtitle(stage_act);
    const char *sub_str = text_get(sub_id != TEXT_NONE ? sub_id : TEXT_ACT1_SUBTITLE, r->use_ja);

    char line1[128];
    if (r->use_ja) {
        snprintf(line1, sizeof line1, "%s・%s", act_tag, sub_str);
    } else {
        snprintf(line1, sizeof line1, "%s - %s", act_tag, sub_str);
    }
    const float cx = box.x + box.width * 0.5f;
    render_draw_text_fit(r, line1, (Vector2){cx, box.y + 16.0f}, box.width - 18.0f, 17.0f, 12.0f,
                         PAL_SUMI_SCORCH, 0.97f, false);

    char wave_part[32];
    snprintf(wave_part, sizeof wave_part, "%s %d/3", text_get(TEXT_HUD_WAVE, r->use_ja),
             g->run.wave_index + 1);
    const TextId trait_id = text_act_trait(stage_act);
    const char *trait_str = text_get(trait_id != TEXT_NONE ? trait_id : TEXT_ACT1_TRAIT, r->use_ja);

    float sz = 14.0f;
    const float max_w = box.width - 18.0f;
    const float sep_w = 10.0f;
    float w1 = render_measure_text(r, wave_part, sz, false).x;
    float w2 = render_measure_text(r, trait_str, sz, false).x;
    while (sz > 11.0f && w1 + sep_w + w2 > max_w) {
        sz -= 1.0f;
        w1 = render_measure_text(r, wave_part, sz, false).x;
        w2 = render_measure_text(r, trait_str, sz, false).x;
    }
    const float total_w = fminf(max_w, w1 + sep_w + w2);
    const float x0 = cx - total_w * 0.5f;
    const float line2_y = box.y + 38.0f - sz * 0.5f;
    render_draw_text(r, wave_part, (Vector2){x0, line2_y}, sz, PAL_BENGARA, 0.96f, false);
    draw_filled_rect(r, (Rectangle){x0 + w1 + 4.0f, box.y + 36.0f, 2.0f, 4.0f},
                     pal_color_alpha(PAL_SUMI_LIGHT, 0.85f));
    render_draw_text_fit(
        r, trait_str,
        (Vector2){x0 + w1 + sep_w + fmaxf(0.0f, total_w - w1 - sep_w) * 0.5f, box.y + 38.0f},
        fmaxf(20.0f, max_w - w1 - sep_w), sz, 11.0f, PAL_SUMI_HEAVY, 0.95f, false);
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
        const char *epithet =
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

        const PaletteId border_pal =
            (is_multipart_ryu && bbc != NULL && bbc->phase_idx >= 2)
                ? PAL_KIN
                : ((is_multipart_ryu && bbc != NULL && bbc->phase_idx == 1) ? PAL_SHU
                                                                            : PAL_BENGARA);
        const Rectangle plaque = {cx - 216.0f, 4.0f, 432.0f, 54.0f};
        ink_fill_panel(r, plaque, PAL_SUMI_SCORCH, 0.94f, border_pal, 0.95f, 319u);

        char act_tag[48];
        format_act_tag(r, g->run.act_index, act_tag, sizeof act_tag);
        char title_buf[128];
        if (r->use_ja) {
            snprintf(title_buf, sizeof title_buf, "%s・%s", act_tag, epithet);
        } else {
            snprintf(title_buf, sizeof title_buf, "%s - %s", act_tag, epithet);
        }
        render_draw_text_fit(r, title_buf, (Vector2){cx, 18.0f}, plaque.width - 28.0f, 18.0f, 13.0f,
                             PAL_GOFUN, 0.98f, false);

        const float ratio =
            total_max > 0 ? ink_clampf((float)cur_hp / (float)total_max, 0.0f, 1.0f) : 0.0f;
        const Rectangle bar_rc = {cx - 196.0f, 34.0f, 392.0f, 14.0f};
        draw_framed_bar(r, bar_rc, ratio, PAL_SUMI_CLEAR, 0.85f, PAL_GOFUN, 0.94f, PAL_BENGARA,
                        0.96f);
        if (ratio > 0.01f) {
            const Rectangle sheen = {bar_rc.x + 1.5f, bar_rc.y + 1.5f,
                                     (bar_rc.width - 3.0f) * ratio, 3.0f};
            draw_filled_rect(r, sheen, pal_color_alpha(PAL_SHU, 0.65f));
        }

        /* 3 Dragon Orb status marks on a light pill strip beneath the boss plaque */
        if (is_multipart_ryu) {
            static const PaletteId k_orb_pal[GAME_RYU_ORB_COUNT] = {PAL_SHU, PAL_AO, PAL_GOFUN};
            static const PaletteId k_orb_hot[GAME_RYU_ORB_COUNT] = {PAL_SHU_HOT, PAL_AO_HOT,
                                                                    PAL_SUMI_LIGHT};
            const Rectangle pill = {cx - 64.0f, 60.0f, 128.0f, 22.0f};
            ink_fill_panel(r, pill, PAL_GOFUN, 0.93f, PAL_SUMI_DARK, 0.88f, 323u);
            const float orb_y = 71.0f;
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

    if (g->run.wave_index >= GAME_BOSS_WAVE_INDEX) {
        char act_tag[48];
        format_act_tag(r, g->run.act_index, act_tag, sizeof act_tag);
        char wave_buf[96];
        snprintf(wave_buf, sizeof wave_buf, "%s  %s", act_tag, text_get(TEXT_HUD_BOSS, r->use_ja));
        const Rectangle plaque = {cx - 150.0f, 8.0f, 300.0f, 44.0f};
        ink_fill_panel(r, plaque, PAL_SUMI_SCORCH, 0.92f, PAL_BENGARA, 0.92f, 338u);
        render_draw_text_fit(r, wave_buf, (Vector2){cx, 30.0f}, plaque.width - 24.0f, 20.0f, 14.0f,
                             PAL_GOFUN, 0.97f, false);
        return;
    }

    if (g->run.wave_quota > 0 && g->phase != RUN_PHASE_ACT_TRANSITION) {
        const Rectangle stage_box = {402.0f, 4.0f, 254.0f, 52.0f};
        draw_stage_plaque(r, g, stage_box);
        const Rectangle obj_box = {666.0f, 4.0f, 224.0f, 58.0f};
        draw_objective_plaque(r, g, obj_box);
    } else {
        const Rectangle stage_box = {cx - 127.0f, 4.0f, 254.0f, 52.0f};
        draw_stage_plaque(r, g, stage_box);
    }
}

static void draw_score_and_chain(RenderState *r, const GameState *g)
{
    const float rx = (float)INK_SCREEN_W - 224.0f;
    render_draw_text_id(r, TEXT_HUD_SCORE, (Vector2){rx, 17.0f}, 19.0f, PAL_SUMI_DARK, 0.96f,
                        false);
    char sc_buf[32];
    snprintf(sc_buf, sizeof sc_buf, "%07d", g->run.score);
    render_draw_number_fixed(r, sc_buf, (Vector2){rx + 68.0f, 16.0f}, 21.0f, PAL_SUMI_SCORCH,
                             0.98f);

    if (g->run.chain >= 2 && g->run.chain_t > 0.0f) {
        static const float k_chain_grace[4] = {2.65f, 3.20f, 3.80f, 4.50f};
        uint8_t lv = game_upgrade_level(g, UPG_CHAIN_PRIMER);
        if (lv > 3u) {
            lv = 3u;
        }
        const bool surging = g->player.surge_t > 0.0f;
        char ch_buf[48];
        snprintf(ch_buf, sizeof ch_buf, "%s%d", text_get(TEXT_CALLOUT_CHAIN, r->use_ja),
                 g->run.chain);
        const Rectangle cbg = {rx - 14.0f, 64.0f, 224.0f, 44.0f};
        ink_fill_panel(r, cbg, PAL_SUMI_SCORCH, 0.94f, surging ? PAL_KIN : PAL_SHU, 0.96f, 349u);
        render_draw_text_fit(r, ch_buf, (Vector2){cbg.x + cbg.width * 0.5f, cbg.y + 16.0f},
                             cbg.width - 24.0f, 21.0f, 14.0f, surging ? PAL_KIN_HOT : PAL_GOFUN,
                             0.98f, false);
        const float gauge = ink_clampf(g->run.chain_t / k_chain_grace[lv], 0.0f, 1.0f);
        const Rectangle gbar = {cbg.x + 14.0f, cbg.y + 32.0f, cbg.width - 28.0f, 6.0f};
        draw_framed_bar(r, gbar, gauge, PAL_SUMI_CLEAR, 0.55f, PAL_SUMI_HEAVY, 0.65f,
                        surging ? PAL_KIN_HOT : PAL_SHU, 0.98f);
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
    const float cyc_cost = player_skills_cyclone_cost(g);
    const float gale_cost = player_skills_gale_cost(g);
    const float cyc_shake =
        r->skill_deny_t[SKILL_CYCLONE] > 0.0f ? sinf(r->vfx_time * 80.0f) * 3.5f * cs : 0.0f;
    const float cyc_x = c.x + (double_gale ? -14.0f : -11.0f) * cs + cyc_shake;
    const bool cyc_ready =
        render_skill_pip_ready(g->player.cyclone_cd <= 0.001f, g->player.ink, cyc_cost);
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
            const bool charged = (k < charges);
            const bool ready = render_skill_pip_ready(charged, g->player.ink, gale_cost);
            const bool recharging = (k == charges);
            const float fill =
                charged
                    ? 1.0f
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
        const bool gale_ready = render_skill_pip_ready(
            g->player.gale_charges > 0 && g->player.gale_cd <= 0.001f, g->player.ink, gale_cost);
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
            ui->scene == UI_SCENE_BOKUAN || ui->scene == UI_SCENE_SETTINGS ||
            ui->scene == UI_SCENE_GUIDE) {
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
