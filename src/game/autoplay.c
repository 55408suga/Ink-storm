#include "game.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

BrushInput game_autoplay_input(const GameState *g)
{
    BrushInput in = {0};
    if (g == NULL) {
        return in;
    }
    const float cx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float cy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float bx = g->player.cursor_x;
    const float by = g->player.cursor_y;

    in.pmx = bx;
    in.pmy = by;
    in.mx = bx;
    in.my = by;

    if (g->phase != RUN_PHASE_PLAYING) {
        return in;
    }

    const float t = (float)g->ticks * INK_FIXED_DT;
    const bool ink_dip =
        (g->player.ink < 14.0f) || (g->player.idle_t > 0.01f && g->player.ink < 46.0f);

    const float orbit_a = t * 2.8f;
    const float orbit_r = 130.0f + 65.0f * sinf(orbit_a * 0.6f);
    float tx = cx + cosf(orbit_a) * orbit_r * 1.38f;
    float ty = cy + sinf(orbit_a * 1.18f) * orbit_r * 0.90f;
    bool want_paint = !ink_dip;

    /* Scan active entities once for orbs, projectiles, umbrellas, and boss. */
    const uint32_t hw = g->ecs.high_water;
    float best_orb_d2 = (ink_dip || g->player.ink < 55.0f) ? (620.0f * 620.0f) : (250.0f * 250.0f);
    bool has_orb = false;
    float orb_x = cx;
    float orb_y = cy;

    float best_p_score = 1e12f;
    bool has_proj = false;
    float proj_x = cx;
    float proj_y = cy;
    float proj_vx = 0.0f;
    float proj_vy = 0.0f;
    int32_t close_proj_n = 0;
    float urgent_d2 = 1e12f;
    float clx = cx;
    float cly = cy;
    bool fireball_near_ink = false;
    float fb_x = cx;
    float fb_y = cy;

    float best_umb_d2 = 340.0f * 340.0f;
    bool has_umb = false;
    float umb_x = cx;
    float umb_y = cy;

    bool has_boss = false;
    float boss_x = cx;
    float boss_y = 140.0f;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        if (tr == NULL || tr->dissolving) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_INK_ORB)) {
            const float dx = tr->x - bx;
            const float dy = tr->y - by;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best_orb_d2) {
                best_orb_d2 = d2;
                has_orb = true;
                orb_x = tr->x;
                orb_y = tr->y;
            }
        } else if (ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            const ProjComp *p = game_proj(g, id);
            const VelocityComp *vel = game_velocity(g, id);
            if (p == NULL || vel == NULL || p->deflected) {
                continue;
            }
            const float dx = tr->x - cx;
            const float dy = tr->y - cy;
            const float d2 = dx * dx + dy * dy;
            if (d2 < 280.0f * 280.0f) {
                ++close_proj_n;
                if (d2 < urgent_d2) {
                    urgent_d2 = d2;
                    clx = tr->x;
                    cly = tr->y;
                }
            }
            if (p->kind == PROJ_FIREBALL && !fireball_near_ink) {
                float dye[FLUID_MAX_CHANNELS] = {0};
                fluid_sample(&g->fluid, tr->x, tr->y, NULL, NULL, dye);
                if (dye[0] > 0.14f) {
                    fireball_near_ink = true;
                    fb_x = tr->x;
                    fb_y = tr->y;
                }
            }
            const float score = d2 * (p->kind == PROJ_FIREBALL ? 0.65f : 1.0f);
            if (d2 > 34.0f * 34.0f && d2 < 460.0f * 460.0f && score < best_p_score) {
                best_p_score = score;
                has_proj = true;
                proj_x = tr->x;
                proj_y = tr->y;
                proj_vx = vel->vx;
                proj_vy = vel->vy;
            }
        } else if (ecs_has_mask(&g->ecs, id, COMP_ENEMY)) {
            const EnemyComp *en = game_enemy(g, id);
            if (en == NULL) {
                continue;
            }
            if (en->def_id == ENEMY_UMBRELLA) {
                const float dx = tr->x - cx;
                const float dy = tr->y - cy;
                const float d2 = dx * dx + dy * dy;
                if (d2 < best_umb_d2) {
                    best_umb_d2 = d2;
                    has_umb = true;
                    umb_x = tr->x;
                    umb_y = tr->y;
                }
            } else if (game_is_boss_enemy(en->def_id)) {
                has_boss = true;
                boss_x = tr->x;
                boss_y = tr->y;
            }
        }
    }

    const float cyc_cost = g->player.surge_t > 0.0f ? 13.0f : 26.0f;
    const float gale_cost = g->player.surge_t > 0.0f ? 8.0f : 16.0f;

    if (ink_dip) {
        want_paint = false;
        if (has_orb) {
            tx = orb_x;
            ty = orb_y;
        } else if (has_proj) {
            tx = proj_x;
            ty = proj_y;
        }
    } else if (has_umb && best_umb_d2 < 260.0f * 260.0f) {
        const float dx = umb_x - cx;
        const float dy = umb_y - cy;
        const float d = hypotf(dx, dy);
        if (d > 1e-6f) {
            tx = umb_x - (dx / d) * 32.0f;
            ty = umb_y - (dy / d) * 32.0f;
        } else {
            tx = umb_x;
            ty = umb_y;
        }
        want_paint = true;
        if (hypotf(umb_x - bx, umb_y - by) < 155.0f && g->player.gale_cd <= 0.0f &&
            g->player.ink >= gale_cost + 2.0f) {
            in.gale = true;
        }
    } else if (has_proj) {
        const float psp = hypotf(proj_vx, proj_vy);
        if (psp > 1e-6f) {
            const float nx = -proj_vy / psp;
            const float ny = proj_vx / psp;
            const float slash = sinf(t * 10.5f) * 62.0f;
            tx = proj_x + nx * slash;
            ty = proj_y + ny * slash;
        } else {
            tx = proj_x;
            ty = proj_y;
        }
        want_paint = true;
    } else if (has_orb && g->player.ink < 82.0f) {
        tx = orb_x;
        ty = orb_y;
        want_paint = false;
    } else if (has_boss) {
        tx = boss_x + cosf(t * 4.5f) * 85.0f;
        ty = boss_y + sinf(t * 4.5f) * 60.0f + 45.0f;
        want_paint = true;
    }

    if (g->player.cyclone_cd <= 0.0f && g->player.ink >= cyc_cost + 4.0f) {
        if (fireball_near_ink) {
            in.cyclone = true;
            tx = fb_x;
            ty = fb_y;
        } else if (close_proj_n >= 2 || urgent_d2 < 145.0f * 145.0f ||
                   (has_boss && has_proj && (g->ticks % UINT64_C(95) == 0))) {
            in.cyclone = true;
            tx = clx;
            ty = cly;
        }
    }

    if (!in.gale && !in.cyclone && g->player.gale_cd <= 0.0f &&
        g->player.ink >= gale_cost + 12.0f &&
        ((g->internal.blazing_active || has_boss) && (g->ticks % UINT64_C(130) == 35))) {
        in.gale = true;
    }

    in.mx = ink_clampf(bx + ink_clampf(tx - bx, -34.0f, 34.0f), 90.0f, (float)INK_SCREEN_W - 90.0f);
    in.my = ink_clampf(by + ink_clampf(ty - by, -34.0f, 34.0f), 88.0f, (float)INK_SCREEN_H - 82.0f);
    in.paint = want_paint && !in.cyclone && !in.gale;
    return in;
}

GameCommand game_autoplay_command(const GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD) {
        return (GameCommand){.type = CMD_NONE, .arg = 0};
    }
    if (g->shrine_count > 0 && g->shrines[0].hp <= 4 && g->run.reward.repair_available) {
        return (GameCommand){.type = CMD_REPAIR_SHRINE, .arg = 0};
    }
    const uint8_t max_rlv = meta_upgrade_max_level(META_UPG_REROLLS);
    const uint8_t rlv = g->config.meta_upgrade_lv[META_UPG_REROLLS] > max_rlv
                            ? max_rlv
                            : g->config.meta_upgrade_lv[META_UPG_REROLLS];
    const int32_t act_rerolls = 1 + (int32_t)rlv;
    const bool pickable_offer =
        g->run.reward.kind == REWARD_KIND_UPGRADE || g->run.reward.kind == REWARD_KIND_RELIC;
    if (pickable_offer && g->run.rerolls_left == act_rerolls) {
        return (GameCommand){.type = CMD_REROLL_REWARD, .arg = 0};
    }
    if (pickable_offer && g->run.reward.choice_count > 0) {
        return (GameCommand){
            .type = CMD_PICK_REWARD,
            .arg = (int32_t)(g->ticks % (uint64_t)g->run.reward.choice_count),
        };
    }
    if (g->run.reward.repair_available) {
        return (GameCommand){.type = CMD_REPAIR_SHRINE, .arg = 0};
    }
    return (GameCommand){.type = CMD_NONE, .arg = 0};
}
