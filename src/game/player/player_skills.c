#include "player/player_skills.h"

#include "core.h"
#include "roguelike/upgrades.h"

#include <math.h>
#include <stddef.h>

static void decay_timer(float *t)
{
    if (*t > 0.0f) {
        *t -= INK_FIXED_DT;
        if (*t <= 1e-4f) {
            *t = 0.0f;
        }
    }
}

static void regen_ink_idle(GameState *g)
{
    g->player.idle_t += INK_FIXED_DT;
    const float base_rate = g->player.idle_t > 0.32f ? 26.0f : 15.0f;
    const float rate = base_rate * g->internal.mods.ink_regen_mul;
    g->player.ink = ink_clampf(g->player.ink + rate * INK_FIXED_DT, 0.0f, g->player.ink_max);
    g->player.dry = g->player.ink < 20.0f;
}

static void register_active_cyclone(GameState *g, float x, float y, float sustain_rad,
                                    float duration, float storm_sustain)
{
    int32_t slot = 0;
    float min_life = g->internal.cyclones[0].life;
    for (int32_t i = 0; i < GAME_MAX_ACTIVE_CYCLONES; ++i) {
        if (!g->internal.cyclones[i].active) {
            slot = i;
            break;
        }
        if (g->internal.cyclones[i].life < min_life) {
            min_life = g->internal.cyclones[i].life;
            slot = i;
        }
    }
    g->internal.cyclones[slot] = (ActiveCyclone){
        .active = true,
        .x = x,
        .y = y,
        .rad_cells = sustain_rad,
        .life = duration,
        .max_life = duration,
        .storm_sustain = storm_sustain,
    };
}

static float mass_gale_multiplier(MassTier tier)
{
    switch (tier) {
    case MASS_LIGHT:
        return 1.0f;
    case MASS_MEDIUM:
        return 0.6f;
    case MASS_HEAVY:
        return 0.35f;
    default:
        return 0.0f;
    }
}

static void cast_cyclone(GameState *g)
{
    const RunModifiers *mods = &g->internal.mods;
    const float cost = g->player.surge_t > 0.0f ? 26.0f * 0.5f : 26.0f;
    if (g->player.cyclone_cd > 0.0f || g->player.ink < cost) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_SKILL_DENIED,
                                     .x = g->player.cursor_x,
                                     .y = g->player.cursor_y,
                                     .value = (int32_t)SKILL_CYCLONE,
                                     .flags = 0,
                                 });
        return;
    }

    g->player.ink -= cost;
    g->player.dry = g->player.ink < 20.0f;
    g->player.cyclone_cd = mods->cyclone_cd;

    const float rad_cells = 15.5f * mods->cyclone_radius_mul;
    const float duration = 0.72f + mods->cyclone_duration_add;
    float storm_add = 0.0f;
    float storm_sustain = 0.0f;
    upgrades_fire_on_cyclone(g, g->player.cursor_x, g->player.cursor_y, &storm_add, &storm_sustain);

    const float dye[FLUID_MAX_CHANNELS] = {0.92f, 0.0f, storm_add, 0.0f};
    fluid_swirl(&g->fluid, g->player.cursor_x, g->player.cursor_y, rad_cells, 54.0f, dye);
    register_active_cyclone(g, g->player.cursor_x, g->player.cursor_y,
                            13.0f * mods->cyclone_radius_mul, duration, storm_sustain);

    /* Pull nearby Ink Orbs into the vortex center. */
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        const InkOrbComp *orb = game_orb(g, id);
        if (tr == NULL || vel == NULL || orb == NULL) {
            continue;
        }
        const double dx = (double)g->player.cursor_x - (double)tr->x;
        const double dy = (double)g->player.cursor_y - (double)tr->y;
        const double d = hypot(dx, dy);
        if (d < 220.0 && d > 1e-6) {
            vel->vx += (float)(dx / d) * 320.0f;
            vel->vy += (float)(dy / d) * 320.0f;
        }
    }

    const float px_rad = ink_clampf(rad_cells * FLUID_CELL_PX, 1.0f, 100000.0f);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_CYCLONE,
                                 .x = g->player.cursor_x,
                                 .y = g->player.cursor_y,
                                 .value = (int32_t)floorf(px_rad + 0.5f),
                                 .flags = 0,
                             });
}

static void cast_gale(GameState *g)
{
    const RunModifiers *mods = &g->internal.mods;
    const float cost = g->player.surge_t > 0.0f ? 16.0f * 0.5f : 16.0f;
    if (g->player.gale_cd > 0.0f || g->player.ink < cost) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_SKILL_DENIED,
                                     .x = g->player.cursor_x,
                                     .y = g->player.cursor_y,
                                     .value = (int32_t)SKILL_GALE,
                                     .flags = 0,
                                 });
        return;
    }

    g->player.ink -= cost;
    g->player.dry = g->player.ink < 20.0f;
    g->player.gale_cd = mods->gale_cd;

    float gx = g->player.aim_x;
    float gy = g->player.aim_y;
    const float aim_len = hypotf(gx, gy);
    if (aim_len >= 0.5f) {
        gx /= aim_len;
        gy /= aim_len;
    } else {
        const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
        const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
        const double dx = (double)g->player.cursor_x - (double)sx;
        const double dy = (double)g->player.cursor_y - (double)sy;
        const double d = hypot(dx, dy);
        if (d > 1e-6) {
            gx = (float)(dx / d);
            gy = (float)(dy / d);
        } else {
            gx = 1.0f;
            gy = 0.0f;
        }
    }

    static const float k_gale_dye[FLUID_MAX_CHANNELS] = {0.28f, 0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 12; ++k) {
        const float dist = (float)k * 20.0f;
        fluid_splat(&g->fluid, g->player.cursor_x + gx * dist, g->player.cursor_y + gy * dist, 4.6f,
                    gx * 52.0f, gy * 52.0f, k_gale_dye);
    }

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        if (tr == NULL || vel == NULL || tr->dissolving) {
            continue;
        }

        const double ex = (double)tr->x - (double)g->player.cursor_x;
        const double ey = (double)tr->y - (double)g->player.cursor_y;
        const double d = hypot(ex, ey);
        if (d <= 1e-6 || d >= 245.0) {
            continue;
        }
        const float dot = (float)((ex * (double)gx + ey * (double)gy) / d);

        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY)) {
            const float mass_mul = mass_gale_multiplier(vel->mass_tier);
            StatusComp *st = game_status_mut(g, id);
            if (mass_mul > 0.0f && st != NULL && dot > 0.35f) {
                const float imp = 560.0f * mass_mul * mods->gale_impulse_mul;
                vel->vx += gx * imp;
                vel->vy += gy * imp;
                if (st->knockback_t < 1.15f) {
                    st->knockback_t = 1.15f;
                }
            }
        } else if (ecs_has_mask(&g->ecs, id, COMP_PROJ) && d < 230.0 && dot > 0.25f) {
            ProjComp *proj = game_proj_mut(g, id);
            if (proj != NULL) {
                const bool was_deflected = proj->deflected;
                proj->deflected = true;
                if (proj->kind == PROJ_STORM_ORB) {
                    proj->electrified = true;
                }
                vel->vx = gx * 480.0f;
                vel->vy = gy * 480.0f;
                if (!was_deflected) {
                    (void)game_push_event(g, (GameEvent){
                                                 .type = EV_DEFLECT,
                                                 .x = tr->x,
                                                 .y = tr->y,
                                                 .x2 = gx,
                                                 .y2 = gy,
                                                 .value = (int32_t)proj->kind,
                                                 .flags = 0,
                                             });
                }
            }
        }
    }

    upgrades_fire_on_gale(g, g->player.cursor_x, g->player.cursor_y, gx, gy);

    (void)game_push_event(g, (GameEvent){
                                 .type = EV_GALE,
                                 .x = g->player.cursor_x,
                                 .y = g->player.cursor_y,
                                 .x2 = gx,
                                 .y2 = gy,
                                 .value = 0,
                                 .flags = 0,
                             });
}

void player_skills_step(GameState *g, const BrushInput *valid_in)
{
    if (g == NULL) {
        return;
    }
    const RunModifiers *mods = &g->internal.mods;

    decay_timer(&g->player.cyclone_cd);
    decay_timer(&g->player.gale_cd);
    decay_timer(&g->player.surge_t);
    if (g->run.chain_t > 0.0f) {
        g->run.chain_t -= INK_FIXED_DT;
        if (g->run.chain_t <= 1e-4f) {
            g->run.chain_t = 0.0f;
            g->run.chain = 0;
        }
    }
    for (int32_t i = 0; i < g->shrine_count && i < GAME_MAX_SHRINES; ++i) {
        decay_timer(&g->shrines[i].invuln_t);
    }

    g->player.cyclone_cd_total = mods->cyclone_cd;
    g->player.gale_cd_total = mods->gale_cd;
    g->player.painting = false;
    g->player.stroke_speed = 0.0f;

    if (valid_in != NULL) {
        g->player.cursor_x = valid_in->mx;
        g->player.cursor_y = valid_in->my;

        const double mdx = (double)valid_in->mx - (double)valid_in->pmx;
        const double mdy = (double)valid_in->my - (double)valid_in->pmy;
        const double mdist_d = hypot(mdx, mdy);
        const float mdist = mdist_d > 1e6 ? 1e6f : (float)mdist_d;
        g->player.stroke_speed = mdist * (float)INK_TICK_HZ;

        if (mdist_d > 2.0) {
            const float nx = (float)(mdx / mdist_d);
            const float ny = (float)(mdy / mdist_d);
            if (g->player.aim_x == 0.0f && g->player.aim_y == 0.0f) {
                g->player.aim_x = nx;
                g->player.aim_y = ny;
            } else {
                const float ax = g->player.aim_x * 0.65f + nx * 0.35f;
                const float ay = g->player.aim_y * 0.65f + ny * 0.35f;
                const float al = hypotf(ax, ay);
                if (al > 1e-6f) {
                    g->player.aim_x = ax / al;
                    g->player.aim_y = ay / al;
                } else {
                    g->player.aim_x = nx;
                    g->player.aim_y = ny;
                }
            }
        }

        if (valid_in->paint) {
            g->player.idle_t = 0.0f;
            const float surge_cost_mul = g->player.surge_t > 0.0f ? 0.5f : 1.0f;
            const float raw_cost =
                (0.16f + mdist * 0.028f) * surge_cost_mul * mods->stroke_cost_mul;
            if (g->player.ink > 0.5f) {
                const float spent = fminf(g->player.ink, raw_cost);
                g->player.ink -= spent;
                if (g->player.ink < 0.0f) {
                    g->player.ink = 0.0f;
                }
                const bool dry = g->player.ink < 20.0f;
                g->player.dry = dry;

                const float state_force = dry ? 0.46f : (g->player.surge_t > 0.0f ? 1.18f : 1.0f);
                const float force_scale = state_force * mods->brush_force_mul;
                const float ink_scale = dry ? 0.34f : 1.0f;
                const float base_rad = dry ? 2.8f : (g->player.surge_t > 0.0f ? 4.2f : 3.7f);
                const float rad = base_rad * mods->brush_radius_mul;

                const int steps = (int)ink_clampf(mdist / 5.0f, 1.0f, 18.0f);
                const float fx =
                    ink_clampf((float)(mdx * 0.58 * (double)force_scale), -46.0f, 46.0f);
                const float fy =
                    ink_clampf((float)(mdy * 0.58 * (double)force_scale), -46.0f, 46.0f);
                const float ink_amt = ink_clampf(0.26f + mdist * 0.011f, 0.26f, 0.65f) /
                                      (float)steps * 2.35f * ink_scale;
                const float dye[FLUID_MAX_CHANNELS] = {ink_amt, 0.0f, 0.0f, 0.0f};

                for (int k = 1; k <= steps; ++k) {
                    if (dry && (((int)(g->ticks & 1u) + k) % 2 == 0)) {
                        continue;
                    }
                    const double t = (double)k / (double)steps;
                    const float px = (float)((double)valid_in->pmx + mdx * t);
                    const float py = (float)((double)valid_in->pmy + mdy * t);
                    fluid_splat(&g->fluid, px, py, rad, fx, fy, dye);
                }
                g->player.painting = true;
                upgrades_fire_on_stroke(g, valid_in->mx, valid_in->my, fx, fy, mdist);
            } else {
                g->player.dry = true;
            }
        } else {
            regen_ink_idle(g);
        }

        if (valid_in->cyclone) {
            cast_cyclone(g);
        }
        if (valid_in->gale) {
            cast_gale(g);
        }
    } else {
        regen_ink_idle(g);
    }

    for (int32_t i = 0; i < GAME_MAX_ACTIVE_CYCLONES; ++i) {
        ActiveCyclone *ac = &g->internal.cyclones[i];
        if (!ac->active) {
            continue;
        }
        ac->life -= INK_FIXED_DT;
        if (ac->life > ac->max_life * 0.45f) {
            const float sdye[FLUID_MAX_CHANNELS] = {0.14f, 0.0f, ac->storm_sustain, 0.0f};
            fluid_swirl(&g->fluid, ac->x, ac->y, ac->rad_cells, 18.0f, sdye);
        }
        if (ac->life <= 1e-4f) {
            ac->active = false;
        }
    }

    if (g->internal.boss_vortex_t > 0.0f) {
        g->internal.boss_vortex_t -= INK_FIXED_DT;
        fluid_swirl(&g->fluid, g->internal.boss_vortex_x, g->internal.boss_vortex_y, 14.0f, -36.0f,
                    NULL);
        if (g->internal.boss_vortex_t <= 1e-4f) {
            g->internal.boss_vortex_t = 0.0f;
        }
    }
}
