#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "roguelike/waves.h"

#include <math.h>
#include <stddef.h>

static const GameVec2 k_nue_waypoints[8] = {
    {640.0f, 130.0f}, {1060.0f, 170.0f}, {1080.0f, 360.0f}, {1060.0f, 550.0f},
    {640.0f, 590.0f}, {220.0f, 550.0f},  {200.0f, 360.0f},  {220.0f, 170.0f},
};

static void update_dash_vector(const TransformComp *tr, BossComp *boss, int32_t wp_idx)
{
    const GameVec2 wp = k_nue_waypoints[wp_idx & 7];
    const float dx = wp.x - tr->x;
    const float dy = wp.y - tr->y;
    const float d = hypotf(dx, dy);
    if (d > 1e-6f) {
        boss->gale_dx = dx / d;
        boss->gale_dy = dy / d;
    } else {
        boss->gale_dx = 1.0f;
        boss->gale_dy = 0.0f;
    }
}

void boss_nue_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || en == NULL || boss == NULL) {
        return;
    }

    en->ai_state = 0; /* 0 = dash telegraph (0.5s), 1 = stopped (1.5s), 2 = dashing (600px/s) */
    en->phase = 1;    /* First target waypoint */
    en->fire_cd = 0.5f;
    en->windup = 0.0f;

    boss->pattern = 0;
    boss->pattern_timer = 4.0f; /* Pack Call 12s CD (initial grace 4s) */
    boss->phase_idx = 0;
    boss->gale_t = 0.0f;
    boss->part_id = 0;
    boss->parent_id = (EntityId){ECS_INVALID_INDEX, 0};
    boss->telegraph_pattern = 0;
    boss->telegraph = 0.0f;
    update_dash_vector(tr, boss, en->phase);

    g->run.boss_id = id;
    g->internal.last_boss_x = tr->x;
    g->internal.last_boss_y = tr->y;

    static const float k_swirl_dye[FLUID_MAX_CHANNELS] = {0.8f, 0.0f, 0.9f, 0.0f};
    fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 52.0f, k_swirl_dye);
}

void boss_nue_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    const HealthComp *hp = game_health(g, id);
    StatusComp *st = game_status_mut(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || boss == NULL) {
        return;
    }

    if (boss->pattern_timer > 0.0f) {
        boss->pattern_timer -= INK_FIXED_DT;
        if (boss->pattern_timer <= 1e-4f) {
            boss->pattern_timer = 0.0f;
        }
    }

    if (st->stun_t > 0.0f) {
        vel->mass_tier = (en->ai_state == 1) ? MASS_MEDIUM : MASS_BOSS;
        if (st->knockback_t <= 0.0f) {
            vel->vx *= 0.85f;
            vel->vy *= 0.85f;
        }
        boss->telegraph = 0.0f;
        return;
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const int32_t wp_idx = en->phase & 7;
    const GameVec2 wp = k_nue_waypoints[wp_idx];

    if (en->ai_state == 0) {
        /* Dash telegraph (0.5s). */
        vel->mass_tier = MASS_BOSS;
        st->knockback_t = 0.0f;
        vel->vx = 0.0f;
        vel->vy = 0.0f;
        update_dash_vector(tr, boss, wp_idx);
        tr->facing = atan2f(boss->gale_dy, boss->gale_dx);
        boss->telegraph_pattern = 0;

        en->fire_cd -= INK_FIXED_DT * slow;
        boss->telegraph = ink_clampf((0.5f - en->fire_cd) / 0.5f, 0.0f, 1.0f);
        if (en->fire_cd <= 0.0f) {
            /* Register lightning trail along the dash path (2.0s; 2 simultaneous in Phase 2). */
            if (boss->phase_idx >= 1) {
                g->internal.nue_trail_x[1][0] = g->internal.nue_trail_x[0][0];
                g->internal.nue_trail_x[1][1] = g->internal.nue_trail_x[0][1];
                g->internal.nue_trail_y[1][0] = g->internal.nue_trail_y[0][0];
                g->internal.nue_trail_y[1][1] = g->internal.nue_trail_y[0][1];
                g->internal.nue_trail_t[1] = g->internal.nue_trail_t[0];
            } else {
                g->internal.nue_trail_t[1] = 0.0f;
            }
            g->internal.nue_trail_x[0][0] = tr->x;
            g->internal.nue_trail_y[0][0] = tr->y;
            g->internal.nue_trail_x[0][1] = tr->x;
            g->internal.nue_trail_y[0][1] = tr->y;
            g->internal.nue_trail_t[0] = 2.0f;

            en->ai_state = 2;
            en->fire_cd = 1.5f;
            boss->telegraph = 0.0f;
        }
    } else if (en->ai_state == 2) {
        /* Dashing at 600px/s along lightning path (immune to knockback/slam). */
        vel->mass_tier = MASS_BOSS;
        st->knockback_t = 0.0f;
        const float dx = wp.x - tr->x;
        const float dy = wp.y - tr->y;
        const float dist = hypotf(dx, dy);
        const float step_dist = 600.0f * slow * INK_FIXED_DT;

        if (dist > 1e-6f) {
            boss->gale_dx = dx / dist;
            boss->gale_dy = dy / dist;
            tr->facing = atan2f(boss->gale_dy, boss->gale_dx);
        }

        static const float k_dash_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.0f, 0.6f, 0.0f};
        fluid_splat(&g->fluid, tr->x, tr->y, 2.8f, 0.0f, 0.0f, k_dash_dye);

        en->fire_cd -= INK_FIXED_DT;
        const bool reaching_wp = (dist <= step_dist + 8.0f);
        const float target_x = reaching_wp ? wp.x
                                           : ink_clampf(tr->x + boss->gale_dx * step_dist, 72.0f,
                                                        (float)INK_SCREEN_W - 72.0f);
        const float target_y = reaching_wp ? wp.y
                                           : ink_clampf(tr->y + boss->gale_dy * step_dist, 84.0f,
                                                        (float)INK_SCREEN_H - 72.0f);
        const float move_dx = target_x - tr->x;
        const float move_dy = target_y - tr->y;
        const float move_dist = hypotf(move_dx, move_dy);
        int steps = 1;
        if (move_dist > 6.0f) {
            steps = (int)ceilf(move_dist / 6.0f);
            if (steps > 16) {
                steps = 16;
            }
        }
        bool hit_solid = false;
        float safe_x = tr->x;
        float safe_y = tr->y;
        for (int s = 1; s <= steps; ++s) {
            const float frac = (float)s / (float)steps;
            const float probe_x = tr->x + move_dx * frac;
            const float probe_y = tr->y + move_dy * frac;
            if (fluid_is_solid(&g->fluid, probe_x, probe_y)) {
                hit_solid = true;
                break;
            }
            safe_x = probe_x;
            safe_y = probe_y;
        }
        tr->x = safe_x;
        tr->y = safe_y;
        if (g->internal.nue_trail_t[0] > 0.0f) {
            g->internal.nue_trail_x[0][1] = tr->x;
            g->internal.nue_trail_y[0][1] = tr->y;
        }

        if (hit_solid || reaching_wp || en->fire_cd <= 0.0f) {
            en->ai_state = 1;
            vel->mass_tier = MASS_MEDIUM;
            vel->vx = 0.0f;
            vel->vy = 0.0f;
            en->windup = 0.65f;
            en->fire_cd = fmaxf(1.5f * enemy_endless_boss_cd_mul(g), en->windup + 0.10f);
            boss->telegraph = 0.0f;
            boss->telegraph_pattern = (boss->phase_idx >= 1 && (boss->pattern & 1) == 1) ? 3 : 1;
        }
    } else {
        /* Stopped & attacking (1.5s; vulnerable to gale knockback and slam). */
        vel->mass_tier = MASS_MEDIUM;
        if (st->knockback_t <= 0.0f) {
            vel->vx *= 0.8f;
            vel->vy *= 0.8f;
        }
        tr->facing = atan2f(sy - tr->y, sx - tr->x);

        if (hp->invuln_t > 0.0f) {
            boss->telegraph = 0.0f;
        } else if (en->windup > 0.0f) {
            en->windup -= INK_FIXED_DT * slow;
            boss->telegraph = ink_clampf((0.65f - en->windup) / 0.65f, 0.0f, 1.0f);
            if (en->windup <= 0.0f) {
                en->windup = 0.0f;
                boss->telegraph = 0.0f;
                if (boss->phase_idx >= 1 && (boss->pattern & 1) == 1) {
                    /* Pattern D (Phase 2): Night Cry radial shockwave (radius 200px = 25 cells). */
                    fluid_add_radial_impulse(&g->fluid, tr->x, tr->y, 25.0f, 58.0f);
                } else {
                    /* Pattern B: 3-way Bounce Storm Orbs toward the shrine. */
                    const float base_ang = atan2f(sy - tr->y, sx - tr->x);
                    for (int k = -1; k <= 1; ++k) {
                        const float ang = base_ang + (float)k * 0.28f;
                        (void)projectiles_spawn_dir(g, PROJ_BOUNCE_ORB, tr->x, tr->y, cosf(ang),
                                                    sinf(ang), 230.0f);
                    }
                }

                /* Pattern C: Pack Call (up to 2 Kamaitachi, 12s CD scaled, shared escort cap). */
                const int32_t max_escorts = 4 + g->internal.mods.max_alive_add;
                if (boss->pattern_timer <= 0.0f && waves_count_alive_escorts(g) < max_escorts) {
                    boss->pattern_timer = 12.0f * enemy_endless_boss_cd_mul(g);
                    for (int s = 0; s < 2 && waves_count_alive_escorts(g) < max_escorts; ++s) {
                        const float off_x = s == 0 ? -56.0f : 56.0f;
                        const float sx_p =
                            ink_clampf(tr->x + off_x, 84.0f, (float)INK_SCREEN_W - 84.0f);
                        const float sy_p = ink_clampf(tr->y, 96.0f, (float)INK_SCREEN_H - 84.0f);
                        const float spawn_x = fluid_is_solid(&g->fluid, sx_p, sy_p) ? tr->x : sx_p;
                        const float spawn_y = fluid_is_solid(&g->fluid, sx_p, sy_p) ? tr->y : sy_p;
                        (void)waves_spawn_summoned_escort(g, ENEMY_KAMAITACHI, spawn_x, spawn_y);
                    }
                }
                ++boss->pattern;
            }
        }

        /* Re-arming the dash mid-knockback would zero the gale velocity before the wall slam. */
        if (st->knockback_t <= 0.0f) {
            en->fire_cd -= INK_FIXED_DT * slow;
        }
        if (en->fire_cd <= 0.0f) {
            const int32_t jump = 2 + (int32_t)(ink_rng_next(&g->rng) % UINT64_C(3));
            en->phase = (wp_idx + jump) & 7;
            en->ai_state = 0;
            vel->mass_tier = MASS_BOSS;
            st->knockback_t = 0.0f;
            en->fire_cd = 0.5f;
            boss->telegraph = 0.0f;
            update_dash_vector(tr, boss, en->phase);
        }
    }
}
