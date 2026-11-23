#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"

#include <math.h>
#include <stddef.h>

void boss_tengu_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || en == NULL || boss == NULL) {
        return;
    }
    en->phase = (int32_t)(ink_rng_f01(&g->rng) * 6283.0f);
    en->fire_cd = 1.1f;
    en->windup = 0.0f;

    boss->pattern = 0;
    boss->pattern_timer = 0.0f;
    boss->phase_idx = 0;
    boss->gale_dx = 0.0f;
    boss->gale_dy = 1.0f;
    boss->gale_t = 0.0f;
    boss->part_id = 0;
    boss->parent_id = (EntityId){ECS_INVALID_INDEX, 0};
    boss->telegraph_pattern = 0;
    boss->telegraph = 0.0f;

    g->run.boss_id = id;
    g->internal.last_boss_x = tr->x;
    g->internal.last_boss_y = tr->y;

    static const float k_swirl_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.0f, 0.0f, 0.0f};
    static const float k_splat_dye[FLUID_MAX_CHANNELS] = {0.7f, 0.85f, 0.0f, 0.0f};
    fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 52.0f, k_swirl_dye);
    fluid_splat(&g->fluid, tr->x, tr->y, 7.5f, 0.0f, 18.0f, k_splat_dye);
}

void boss_tengu_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    const HealthComp *hp = game_health(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || boss == NULL) {
        return;
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float ph = (float)en->phase * 0.001f;
    en->phase = (en->phase + 24) % 6283;

    const float tx = sx + sinf(ph * 0.62f) * 390.0f;
    const float ty = 135.0f + (1.0f - cosf(ph * 1.24f)) * 90.0f;
    vel->vx = (vel->vx + (tx - tr->x) * 2.4f * INK_FIXED_DT * slow) * 0.90f;
    vel->vy = (vel->vy + (ty - tr->y) * 2.4f * INK_FIXED_DT * slow) * 0.90f;
    tr->x = ink_clampf(tr->x + vel->vx * INK_FIXED_DT, 130.0f, (float)INK_SCREEN_W - 130.0f);
    tr->y = ink_clampf(tr->y + vel->vy * INK_FIXED_DT, 96.0f, (float)INK_SCREEN_H - 210.0f);
    tr->facing = atan2f(sy - tr->y, sx - tr->x);

    if (boss->gale_t > 0.0f) {
        boss->gale_t -= INK_FIXED_DT;
        if (boss->gale_t <= 1e-4f) {
            boss->gale_t = 0.0f;
        }
        static const float k_gale_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.14f, 0.0f, 0.0f};
        for (int k = 1; k <= 9; ++k) {
            const float dist = (float)k * 36.0f;
            fluid_splat(&g->fluid, tr->x + boss->gale_dx * dist, tr->y + boss->gale_dy * dist, 5.2f,
                        boss->gale_dx * 36.0f, boss->gale_dy * 36.0f, k_gale_dye);
        }
    }

    if (hp->invuln_t > 0.0f) {
        boss->telegraph = 0.0f;
        return;
    }

    const int32_t next_pat = boss->pattern;
    const float tele_dur = next_pat == 3 ? 0.8f : 0.6f;

    en->fire_cd -= INK_FIXED_DT * slow;
    if (en->fire_cd <= 0.0f) {
        boss->telegraph = 0.0f;
        const float dx = sx - tr->x;
        const float dy = sy - tr->y;
        const float d = hypotf(dx, dy);
        const float dir_x = d > 1e-6f ? dx / d : 0.0f;
        const float dir_y = d > 1e-6f ? dy / d : 1.0f;
        const float base_ang = atan2f(dir_y, dir_x);
        boss->gale_dx = dir_x;
        boss->gale_dy = dir_y;

        const float cd_mul = (boss->phase_idx >= 1 ? 0.8f : 1.0f) * enemy_endless_boss_cd_mul(g);
        if (next_pat == 0) {
            /* Pattern A: Crimson Gale Sweep + Twin Fireballs. */
            boss->gale_t = 0.85f;
            (void)projectiles_spawn_dir(g, PROJ_FIREBALL, tr->x, tr->y, cosf(base_ang - 0.22f),
                                        sinf(base_ang - 0.22f), 205.0f);
            (void)projectiles_spawn_dir(g, PROJ_FIREBALL, tr->x, tr->y, cosf(base_ang + 0.22f),
                                        sinf(base_ang + 0.22f), 205.0f);
            en->fire_cd = 1.95f * cd_mul;
        } else if (next_pat == 1) {
            /* Pattern B: Fan Arrow Volley (4 arrows) + brief gust. */
            boss->gale_t = 0.40f;
            for (int k = -1; k <= 2; ++k) {
                const float ang = base_ang + ((float)k - 0.5f) * 0.22f;
                (void)projectiles_spawn_dir(g, PROJ_ARROW, tr->x, tr->y, cosf(ang), sinf(ang),
                                            255.0f);
            }
            en->fire_cd = 1.65f * cd_mul;
        } else if (next_pat == 2) {
            /* Pattern C: Full Crimson Gale + Storm Orb + Twin Arrows. */
            boss->gale_t = 1.15f;
            (void)projectiles_spawn_dir(g, PROJ_STORM_ORB, tr->x, tr->y, cosf(base_ang),
                                        sinf(base_ang), 215.0f);
            (void)projectiles_spawn_dir(g, PROJ_ARROW, tr->x, tr->y, cosf(base_ang - 0.30f),
                                        sinf(base_ang - 0.30f), 245.0f);
            (void)projectiles_spawn_dir(g, PROJ_ARROW, tr->x, tr->y, cosf(base_ang + 0.30f),
                                        sinf(base_ang + 0.30f), 245.0f);
            en->fire_cd = 2.05f * cd_mul;
        } else {
            /* Pattern D (Phase 2): Tengu Fan Reverse Vortex around the shrine. */
            g->internal.boss_vortex_t = 1.2f;
            g->internal.boss_vortex_x = sx;
            g->internal.boss_vortex_y = sy;
            fluid_swirl(&g->fluid, sx, sy, 14.0f, -48.0f, NULL);
            en->fire_cd = 2.0f * cd_mul;
        }

        const int32_t pat_count = boss->phase_idx >= 1 ? 4 : 3;
        boss->pattern = (boss->pattern + 1) % pat_count;
        const float next_tele = boss->pattern == 3 ? 0.8f : 0.6f;
        en->fire_cd = fmaxf(en->fire_cd, next_tele + 0.10f);
        boss->telegraph_pattern = boss->pattern;
    } else if (en->fire_cd <= tele_dur) {
        if (boss->gale_t <= 0.0f) {
            const float dx = sx - tr->x;
            const float dy = sy - tr->y;
            const float d = hypotf(dx, dy);
            boss->gale_dx = d > 1e-6f ? dx / d : 0.0f;
            boss->gale_dy = d > 1e-6f ? dy / d : 1.0f;
        }
        boss->telegraph_pattern = next_pat;
        boss->telegraph = ink_clampf((tele_dur - en->fire_cd) / tele_dur, 0.0f, 1.0f);
    } else {
        boss->telegraph = 0.0f;
    }
}
