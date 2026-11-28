#include "roguelike/stage_gimmicks.h"

#include "combat/damage.h"
#include "core.h"

#include <math.h>
#include <stddef.h>

static void spawn_spirit_pillar(GameState *g, float x, float y)
{
    fluid_set_solid_circle(&g->fluid, x, y, 16.0f, true);
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
    if (id.index == ECS_INVALID_INDEX) {
        return;
    }
    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = 16.0f;
    }
    GimmickComp *gim = game_gimmick_mut(g, id);
    if (gim != NULL) {
        gim->kind = (int32_t)GIMMICK_SPIRIT_PILLAR;
        gim->elem_kind = (int32_t)GIMMICK_ELEM_NONE;
        gim->timer = 0.0f;
        gim->param_a = 0.0f;
        gim->param_b = 0.0f;
    }
}

void stage_gimmicks_setup_act(GameState *g, int32_t act_index)
{
    if (g == NULL) {
        return;
    }

    fluid_clear_solids(&g->fluid);
    g->internal.wind_timer = 0.0f;
    g->internal.wind_telegraph_t = 0.0f;
    g->internal.wind_active_t = 0.0f;
    g->internal.wind_dir = 0;

    /* Clear any leftover spirit pillar entities if setup is called directly. */
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            continue;
        }
        const GimmickComp *gim = game_gimmick(g, id);
        if (gim != NULL && gim->kind == (int32_t)GIMMICK_SPIRIT_PILLAR) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);

    const int32_t stage_act = act_index < 0 ? 0 : (act_index % GAME_MAIN_ACT_COUNT);
    if (stage_act == 1) {
        /* Act 2 (and endless cycle 1): Two Torii gates + four rock pillars. */
        fluid_set_solid_rect(&g->fluid, 376.0f, 320.0f, 16.0f, 16.0f, true);
        fluid_set_solid_rect(&g->fluid, 376.0f, 384.0f, 16.0f, 16.0f, true);
        fluid_set_solid_rect(&g->fluid, 888.0f, 320.0f, 16.0f, 16.0f, true);
        fluid_set_solid_rect(&g->fluid, 888.0f, 384.0f, 16.0f, 16.0f, true);

        fluid_set_solid_circle(&g->fluid, 340.0f, 190.0f, 24.0f, true);
        fluid_set_solid_circle(&g->fluid, 940.0f, 190.0f, 24.0f, true);
        fluid_set_solid_circle(&g->fluid, 340.0f, 530.0f, 24.0f, true);
        fluid_set_solid_circle(&g->fluid, 940.0f, 530.0f, 24.0f, true);
    } else if (stage_act == 2) {
        /* Act 3 (and endless cycle 2): Two upper rock pillars + three Spirit Pillars. */
        fluid_set_solid_circle(&g->fluid, 340.0f, 190.0f, 24.0f, true);
        fluid_set_solid_circle(&g->fluid, 940.0f, 190.0f, 24.0f, true);

        spawn_spirit_pillar(g, 640.0f, 130.0f);
        spawn_spirit_pillar(g, 441.0f, 475.0f);
        spawn_spirit_pillar(g, 839.0f, 475.0f);

        /* Mountain wind starts with period=12.0s; first telegraph at 10.5s. */
        g->internal.wind_timer = 12.0f - 1.5f;
        g->internal.wind_telegraph_t = 0.0f;
        g->internal.wind_active_t = 0.0f;
        g->internal.wind_dir = 0;
    }
}

static void inject_pillar_pulse(GameState *g, float x, float y, GimmickElemKind elem)
{
    float dye[FLUID_MAX_CHANNELS] = {0};
    if (elem == GIMMICK_ELEM_FIRE) {
        dye[1] = 0.50f;
    } else if (elem == GIMMICK_ELEM_STORM) {
        dye[2] = 0.50f;
    } else {
        return;
    }
    fluid_splat(&g->fluid, x, y, GAME_PX_TO_CELLS(32.0f), 0.0f, 0.0f, dye);
}

static void step_spirit_pillar(GameState *g, const TransformComp *tr, GimmickComp *gim)
{
    const float *fire_ch = fluid_channel_const(&g->fluid, 1);
    const float *storm_ch = fluid_channel_const(&g->fluid, 2);
    if (fire_ch == NULL || storm_ch == NULL) {
        return;
    }

    const float max_r = 28.0f;
    const int reach = fluid_cell_reach(max_r);
    const int cx = (int)(tr->x / FLUID_CELL_PX);
    const int cy = (int)(tr->y / FLUID_CELL_PX);
    const int x0 = cx - reach < 1 ? 1 : cx - reach;
    const int x1 = cx + reach > FLUID_W - 2 ? FLUID_W - 2 : cx + reach;
    const int y0 = cy - reach < 1 ? 1 : cy - reach;
    const int y1 = cy + reach > FLUID_H - 2 ? FLUID_H - 2 : cy + reach;

    float max_fire = 0.0f;
    float max_storm = 0.0f;
    const float min_r2 = 16.0f * 16.0f;
    const float max_r2 = max_r * max_r;

    for (int gy = y0; gy <= y1; ++gy) {
        const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
        const float dy = cell_y - tr->y;
        const int row = gy * FLUID_W;
        for (int gx = x0; gx <= x1; ++gx) {
            const int idx = row + gx;
            if (g->fluid.solid[idx] != 0) {
                continue;
            }
            const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
            const float dx = cell_x - tr->x;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= min_r2 || d2 > max_r2) {
                continue;
            }
            const float fv = fire_ch[idx];
            const float sv = storm_ch[idx];
            if (isfinite(fv) && fv > max_fire) {
                max_fire = fv;
            }
            if (isfinite(sv) && sv > max_storm) {
                max_storm = sv;
            }
        }
    }

    if (max_fire > 0.40f || max_storm > 0.40f) {
        const GimmickElemKind incoming =
            (max_fire >= max_storm && max_fire > 0.40f) ? GIMMICK_ELEM_FIRE : GIMMICK_ELEM_STORM;
        const bool switching = (gim->timer <= 0.0f || gim->elem_kind != (int32_t)incoming);
        gim->elem_kind = (int32_t)incoming;
        gim->timer = 6.0f;
        if (switching) {
            gim->param_a = 0.50f;
            inject_pillar_pulse(g, tr->x, tr->y, incoming);
        } else {
            gim->param_a -= INK_FIXED_DT;
            if (gim->param_a <= 1e-4f) {
                gim->param_a += 0.50f;
                inject_pillar_pulse(g, tr->x, tr->y, incoming);
            }
        }
    } else if (gim->timer > 0.0f) {
        gim->timer -= INK_FIXED_DT;
        gim->param_a -= INK_FIXED_DT;
        if (gim->param_a <= 1e-4f) {
            gim->param_a += 0.50f;
            inject_pillar_pulse(g, tr->x, tr->y, (GimmickElemKind)gim->elem_kind);
        }
        if (gim->timer <= 1e-4f) {
            gim->timer = 0.0f;
            gim->elem_kind = (int32_t)GIMMICK_ELEM_NONE;
            gim->param_a = 0.0f;
        }
    }
}

static void resolve_lightning_strike(GameState *g, float lx, float ly)
{
    const float *ink_ch = fluid_channel_const(&g->fluid, 0);
    float max_ink = 0.0f;
    if (ink_ch != NULL) {
        const float rod_r = 40.0f;
        const int reach = fluid_cell_reach(rod_r);
        const int cx = (int)(lx / FLUID_CELL_PX);
        const int cy = (int)(ly / FLUID_CELL_PX);
        const int x0 = cx - reach < 1 ? 1 : cx - reach;
        const int x1 = cx + reach > FLUID_W - 2 ? FLUID_W - 2 : cx + reach;
        const int y0 = cy - reach < 1 ? 1 : cy - reach;
        const int y1 = cy + reach > FLUID_H - 2 ? FLUID_H - 2 : cy + reach;
        for (int gy = y0; gy <= y1; ++gy) {
            const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
            const float dy = cell_y - ly;
            const int row = gy * FLUID_W;
            for (int gx = x0; gx <= x1; ++gx) {
                const int idx = row + gx;
                if (g->fluid.solid[idx] != 0) {
                    continue;
                }
                const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
                const float dx = cell_x - lx;
                if (dx * dx + dy * dy <= rod_r * rod_r) {
                    const float iv = ink_ch[idx];
                    if (isfinite(iv) && iv > max_ink) {
                        max_ink = iv;
                    }
                }
            }
        }
    }

    if (max_ink > 0.30f) {
        /* Ink lightning rod: shrine unharmed, inject storm and zap nearest alive enemy. */
        static const float k_rod_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.0f, 1.1f, 0.0f};
        fluid_splat(&g->fluid, lx, ly, GAME_PX_TO_CELLS(32.0f), 0.0f, 0.0f, k_rod_dye);

        EntityId best = {ECS_INVALID_INDEX, 0};
        float best_d2 = 1e18f;
        float bx = lx;
        float by = ly;
        const uint32_t hw = g->ecs.high_water;
        for (uint32_t j = 0; j < hw; ++j) {
            const EntityId eid = ecs_entity_at(&g->ecs, j);
            if (eid.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, eid)) {
                continue;
            }
            const TransformComp *etr = game_transform(g, eid);
            const EnemyComp *een = game_enemy(g, eid);
            if (etr == NULL || een == NULL) {
                continue;
            }
            const float dx = etr->x - lx;
            const float dy = etr->y - ly;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = eid;
                bx = etr->x;
                by = etr->y;
            }
        }
        if (best.index != ECS_INVALID_INDEX) {
            if (g->run.storm_zaps < META_MAX_COUNTERS) {
                ++g->run.storm_zaps;
            }
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_STORM_ZAP,
                                         .x = lx,
                                         .y = ly,
                                         .x2 = bx,
                                         .y2 = by,
                                         .value = 0,
                                         .flags = 0,
                                     });
            (void)combat_apply_damage(g, best, 20, DMG_STORM, 0.0f, 0.0f);
        }
    } else if (g->shrine_count > 0) {
        const float sx = g->shrines[0].x;
        const float sy = g->shrines[0].y;
        const float hit_r = g->shrines[0].hit_radius_proj;
        if (hypotf(lx - sx, ly - sy) <= hit_r) {
            combat_damage_shrine(g, lx, ly, 1);
        }
    }
}

static void step_mountain_wind(GameState *g)
{
    const int32_t stage_act = g->run.act_index < 0 ? 0 : (g->run.act_index % GAME_MAIN_ACT_COUNT);
    if (stage_act != 2) {
        return;
    }
    float period = 12.0f;
    if (g->run.boss_id.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, g->run.boss_id)) {
        const EnemyComp *ben = game_enemy(g, g->run.boss_id);
        const BossComp *bb = game_boss(g, g->run.boss_id);
        if (ben != NULL && ben->def_id == ENEMY_BOSS_RYU && bb != NULL && bb->phase_idx >= 1) {
            period = 6.0f;
        }
    }

    if (g->internal.wind_telegraph_t <= 0.0f && g->internal.wind_active_t <= 0.0f) {
        if (period < 12.0f && g->internal.wind_timer > period - 4.5f) {
            g->internal.wind_timer = period - 4.5f;
        }
        g->internal.wind_timer -= INK_FIXED_DT;
        if (g->internal.wind_timer <= 1e-4f) {
            const int32_t dir = (int32_t)(ink_rng_next(&g->rng) % UINT64_C(4));
            g->internal.wind_dir = dir;
            g->internal.wind_timer = 0.0f;
            g->internal.wind_telegraph_t = 1.5f;

            float ex = 0.0f;
            float ey = (float)INK_SCREEN_H * 0.5f;
            float dx = 1.0f;
            float dy = 0.0f;
            if (dir == 1) {
                ex = (float)INK_SCREEN_W;
                ey = (float)INK_SCREEN_H * 0.5f;
                dx = -1.0f;
                dy = 0.0f;
            } else if (dir == 2) {
                ex = (float)INK_SCREEN_W * 0.5f;
                ey = 0.0f;
                dx = 0.0f;
                dy = 1.0f;
            } else if (dir == 3) {
                ex = (float)INK_SCREEN_W * 0.5f;
                ey = (float)INK_SCREEN_H;
                dx = 0.0f;
                dy = -1.0f;
            }
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_WIND_TELEGRAPH,
                                         .x = ex,
                                         .y = ey,
                                         .x2 = dx,
                                         .y2 = dy,
                                         .value = dir,
                                         .flags = 0,
                                     });
        }
    } else if (g->internal.wind_telegraph_t > 0.0f) {
        g->internal.wind_telegraph_t -= INK_FIXED_DT;
        if (g->internal.wind_telegraph_t <= 1e-4f) {
            g->internal.wind_telegraph_t = 0.0f;
            g->internal.wind_active_t = 3.0f;
        }
    } else if (g->internal.wind_active_t > 0.0f) {
        g->internal.wind_active_t -= INK_FIXED_DT;
        const int32_t dir = g->internal.wind_dir;
        const float max_vel = g->fluid.cfg.max_vel;
        const int band = fluid_cell_reach(160.0f);
        const float wind = GAME_PX_TO_CELLS(96.0f);
        float target_u = 0.0f;
        float target_v = 0.0f;
        int x0 = 1;
        int x1 = FLUID_W - 2;
        int y0 = 1;
        int y1 = FLUID_H - 2;

        if (dir == 0) {
            target_u = wind;
            x0 = 1;
            x1 = band;
        } else if (dir == 1) {
            target_u = -wind;
            x0 = FLUID_W - 1 - band;
            x1 = FLUID_W - 2;
        } else if (dir == 2) {
            target_v = wind;
            y0 = 1;
            y1 = band;
        } else {
            target_v = -wind;
            y0 = FLUID_H - 1 - band;
            y1 = FLUID_H - 2;
        }

        for (int gy = y0; gy <= y1; ++gy) {
            const int row = gy * FLUID_W;
            for (int gx = x0; gx <= x1; ++gx) {
                const int idx = row + gx;
                if (g->fluid.solid[idx] != 0) {
                    continue;
                }
                g->fluid.u[idx] = ink_clampf(g->fluid.u[idx] + (target_u - g->fluid.u[idx]) * 0.22f,
                                             -max_vel, max_vel);
                g->fluid.v[idx] = ink_clampf(g->fluid.v[idx] + (target_v - g->fluid.v[idx]) * 0.22f,
                                             -max_vel, max_vel);
            }
        }

        if (g->internal.wind_active_t <= 1e-4f) {
            g->internal.wind_active_t = 0.0f;
            g->internal.wind_timer = period - 4.5f;
        }
    }
}

static void step_boss_field_effects(GameState *g)
{
    /* Nue persistent lightning trails (up to 2 in Phase 2). */
    static const float k_trail_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.0f, 0.6f, 0.0f};
    for (int t = 0; t < 2; ++t) {
        if (g->internal.nue_trail_t[t] <= 0.0f) {
            continue;
        }
        g->internal.nue_trail_t[t] -= INK_FIXED_DT;
        const float x0 = g->internal.nue_trail_x[t][0];
        const float y0 = g->internal.nue_trail_y[t][0];
        const float x1 = g->internal.nue_trail_x[t][1];
        const float y1 = g->internal.nue_trail_y[t][1];
        const float dist = hypotf(x1 - x0, y1 - y0);
        const int steps = (int)ink_clampf(ceilf(dist / 16.0f), 1.0f, 64.0f);
        for (int s = 0; s <= steps; ++s) {
            const float u = (float)s / (float)steps;
            fluid_splat(&g->fluid, x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, GAME_PX_TO_CELLS(22.4f),
                        0.0f, 0.0f, k_trail_dye);
        }
        if (g->internal.nue_trail_t[t] <= 1e-4f) {
            g->internal.nue_trail_t[t] = 0.0f;
        }
    }

    /* Ippitsu-Ryu Dragon Coil reverse swirl around the shrine. */
    if (g->internal.ryu_coil_t > 0.0f) {
        g->internal.ryu_coil_t -= INK_FIXED_DT;
        const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
        const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
        fluid_swirl(&g->fluid, sx, sy, GAME_PX_TO_CELLS(240.0f), GAME_PX_TO_CELLS(-352.0f), NULL);
        if (g->internal.ryu_coil_t <= 1e-4f) {
            g->internal.ryu_coil_t = 0.0f;
        }
    }
}

void stage_gimmicks_step(GameState *g)
{
    if (g == NULL) {
        return;
    }

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        GimmickComp *gim = game_gimmick_mut(g, id);
        if (tr == NULL || gim == NULL) {
            continue;
        }
        if (gim->kind == (int32_t)GIMMICK_SPIRIT_PILLAR) {
            step_spirit_pillar(g, tr, gim);
        } else if (gim->kind == (int32_t)GIMMICK_KAMAITACHI_VORTEX) {
            fluid_swirl(&g->fluid, tr->x, tr->y, GAME_PX_TO_CELLS(80.0f), GAME_PX_TO_CELLS(-336.0f),
                        NULL);
            gim->timer -= INK_FIXED_DT;
            if (gim->timer <= 1e-4f) {
                ecs_destroy(&g->ecs, id);
            }
        } else if (gim->kind == (int32_t)GIMMICK_LIGHTNING_STRIKE) {
            gim->timer -= INK_FIXED_DT;
            if (gim->timer <= 1e-4f) {
                resolve_lightning_strike(g, tr->x, tr->y);
                ecs_destroy(&g->ecs, id);
            }
        }
    }

    step_mountain_wind(g);
    step_boss_field_effects(g);
}
