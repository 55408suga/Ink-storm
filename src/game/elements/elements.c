#include "elements/elements.h"

#include "combat/damage.h"
#include "core.h"

#include <math.h>
#include <stddef.h>

static float sanitize_sample(float v) { return isfinite(v) ? v : 0.0f; }

static bool index_in_list(const uint32_t *list, int32_t count, uint32_t idx)
{
    for (int32_t i = 0; i < count; ++i) {
        if (list[i] == idx) {
            return true;
        }
    }
    return false;
}

static void clamp_proj_max_speed(VelocityComp *vel, float max_sp)
{
    const float sp = hypotf(vel->vx, vel->vy);
    if (sp > max_sp && sp > 1e-6f) {
        const float scale = max_sp / sp;
        vel->vx *= scale;
        vel->vy *= scale;
    }
}

static int32_t elements_stage1_cells(GameState *g)
{
    float *ink_ch = fluid_channel_mut(&g->fluid, 0);
    float *fire_ch = fluid_channel_mut(&g->fluid, 1);
    float *storm_ch = fluid_channel_mut(&g->fluid, 2);
    if (ink_ch == NULL || fire_ch == NULL || storm_ch == NULL) {
        return 0;
    }

    const float *u = g->fluid.u;
    const float *v = g->fluid.v;
    const uint8_t *solid = g->fluid.solid;
    const float max_dye = g->fluid.cfg.max_dye;
    const float burn_mul =
        g->internal.mods.burn_rate_mul > 0.0f ? g->internal.mods.burn_rate_mul : 1.0f;

    int32_t blazing_cells = 0;
    float blaze_sum_x = 0.0f;
    float blaze_sum_y = 0.0f;
    int32_t storm_cells = 0;

    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const int i = row + x;
            if (solid[i] != 0) {
                continue;
            }
            float ink = ink_ch[i];
            float fire = fire_ch[i];
            float storm = storm_ch[i];
            if (!isfinite(ink) || !isfinite(fire) || !isfinite(storm)) {
                continue;
            }

            if (fire > 0.14f && ink > 0.20f) {
                const float curl =
                    0.5f * ((v[i + 1] - v[i - 1]) - (u[i + FLUID_W] - u[i - FLUID_W]));
                const float vboost = 1.0f + fminf(2.4f, fabsf(curl) * 0.28f);
                const float burn = fminf(ink, fire) * 0.030f * vboost * burn_mul;
                fire = ink_clampf(fire + burn * 1.15f, 0.0f, max_dye);
                ink = ink_clampf(ink - burn * 0.14f, 0.0f, max_dye);
                fire_ch[i] = fire;
                ink_ch[i] = ink;
            }
            if (fire > 0.34f) {
                ++blazing_cells;
                blaze_sum_x += ((float)x + 0.5f) * FLUID_CELL_PX;
                blaze_sum_y += ((float)y + 0.5f) * FLUID_CELL_PX;
            }

            if (storm > 0.14f && ink > 0.24f) {
                const float cond = fminf(ink, storm) * 0.015f;
                storm = ink_clampf(storm + cond, 0.0f, max_dye);
                storm_ch[i] = storm;
            }
            if (storm > 0.24f) {
                ++storm_cells;
            }
        }
    }

    g->internal.blazing_active = blazing_cells >= 24;
    if (blazing_cells >= 24 && g->internal.ignite_callout_cd <= 0.0f) {
        g->internal.ignite_callout_cd = 3.2f;
        if (g->run.ignitions < META_MAX_COUNTERS) {
            ++g->run.ignitions;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_IGNITE,
                                     .x = blaze_sum_x / (float)blazing_cells,
                                     .y = blaze_sum_y / (float)blazing_cells,
                                     .value = blazing_cells,
                                     .flags = 0,
                                 });
    }
    return storm_cells;
}

static void elements_stage4_enemies(GameState *g, int32_t storm_cells)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        HealthComp *hp = game_health_mut(g, id);
        StatusComp *st = game_status_mut(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || hp == NULL || st == NULL || en == NULL || tr->dissolving) {
            continue;
        }

        if (st->ink_mark_t > 0.0f) {
            st->ink_mark_t -= INK_FIXED_DT;
            if (st->ink_mark_t <= 1e-4f) {
                st->ink_mark_t = 0.0f;
            }
        }
        if (st->charge_t > 0.0f) {
            st->charge_t -= INK_FIXED_DT;
            if (st->charge_t <= 1e-4f) {
                st->charge_t = 0.0f;
            }
        }
        if (st->knockback_t > 0.0f) {
            st->knockback_t -= INK_FIXED_DT;
            if (st->knockback_t <= 1e-4f) {
                st->knockback_t = 0.0f;
            }
        }
        if (st->stun_t > 0.0f) {
            st->stun_t -= INK_FIXED_DT;
            if (st->stun_t <= 1e-4f) {
                st->stun_t = 0.0f;
            }
        }
        if (st->barrier_t > 0.0f) {
            st->barrier_t -= INK_FIXED_DT;
            if (st->barrier_t <= 1e-4f) {
                st->barrier_t = 0.0f;
            }
        }
        if (hp->flash_t > 0.0f) {
            hp->flash_t -= INK_FIXED_DT * 2.6f;
            if (hp->flash_t <= 1e-4f) {
                hp->flash_t = 0.0f;
            }
        }
        if (hp->invuln_t > 0.0f) {
            hp->invuln_t -= INK_FIXED_DT;
            if (hp->invuln_t <= 1e-4f) {
                hp->invuln_t = 0.0f;
            }
        }

        float fu = 0.0f;
        float fv = 0.0f;
        float dye[FLUID_MAX_CHANNELS] = {0};
        fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, dye);
        const float ink = sanitize_sample(dye[0]);
        const float fire = sanitize_sample(dye[1]);
        const float storm = sanitize_sample(dye[2]);

        if ((en->elite_flags & ELITE_INK_REPEL) != 0) {
            st->ink_mark_t = 0.0f;
        } else if (ink > 0.28f) {
            st->ink_mark_t = g->internal.mods.ink_mark_duration;
        }
        if (storm > 0.25f) {
            st->charge_t = 2.0f;
        }
        if (fire > 0.42f) {
            st->burn_acc += INK_FIXED_DT;
            const float interval = game_is_boss_enemy(en->def_id) ? 0.55f : 0.36f;
            if (st->burn_acc + 1e-4f >= interval) {
                st->burn_acc = 0.0f;
                (void)combat_apply_damage_f(g, id, g->internal.mods.burn_base_dmg, DMG_FIRE,
                                            fu * FLUID_CELL_PX * 0.2f, fv * FLUID_CELL_PX * 0.2f);
            }
        } else if (st->burn_acc > 0.0f) {
            st->burn_acc -= INK_FIXED_DT * 0.5f;
            if (st->burn_acc < 0.0f) {
                st->burn_acc = 0.0f;
            }
        }
    }

    if (storm_cells < 10 || g->internal.storm_zap_cd > 0.0f) {
        return;
    }

    const float *storm_ch = fluid_channel_const(&g->fluid, 2);
    if (storm_ch == NULL) {
        return;
    }
    const uint8_t *solid = g->fluid.solid;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const StatusComp *st = game_status(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || st == NULL || en == NULL || tr->dissolving ||
            (en->elite_flags & ELITE_STORMPROOF) != 0) {
            continue;
        }

        const int cx = (int)(tr->x / FLUID_CELL_PX);
        const int cy = (int)(tr->y / FLUID_CELL_PX);
        const int x0 = cx - 25 < 1 ? 1 : cx - 25;
        const int x1 = cx + 25 > FLUID_W - 2 ? FLUID_W - 2 : cx + 25;
        const int y0 = cy - 25 < 1 ? 1 : cy - 25;
        const int y1 = cy + 25 > FLUID_H - 2 ? FLUID_H - 2 : cy + 25;

        float best_val = 0.28f;
        int best_idx = -1;
        float best_x = 0.0f;
        float best_y = 0.0f;
        float best_d2 = 0.0f;

        for (int gy = y0; gy <= y1; ++gy) {
            const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
            const float dy = cell_y - tr->y;
            const int row = gy * FLUID_W;
            for (int gx = x0; gx <= x1; ++gx) {
                const int idx = row + gx;
                if (solid[idx] != 0) {
                    continue;
                }
                const float val = storm_ch[idx];
                if (!isfinite(val) || val <= best_val) {
                    continue;
                }
                const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
                const float dx = cell_x - tr->x;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= 195.0f * 195.0f) {
                    best_val = val;
                    best_idx = idx;
                    best_x = cell_x;
                    best_y = cell_y;
                    best_d2 = d2;
                }
            }
        }

        if (best_idx < 0) {
            continue;
        }
        if (st->ink_mark_t <= 0.0f && st->charge_t <= 0.0f && best_d2 > 150.0f * 150.0f) {
            continue;
        }

        g->internal.storm_zap_cd = g->internal.mods.storm_zap_cd;
        if (g->run.storm_zaps < META_MAX_COUNTERS) {
            ++g->run.storm_zaps;
        }

        uint32_t visited[16];
        int32_t visited_n = 0;
        visited[visited_n++] = id.index;

        float cur_x = tr->x;
        float cur_y = tr->y;
        float dmg = 10.0f;

        (void)game_push_event(g, (GameEvent){
                                     .type = EV_STORM_ZAP,
                                     .x = best_x,
                                     .y = best_y,
                                     .x2 = cur_x,
                                     .y2 = cur_y,
                                     .value = 0,
                                     .flags = 0,
                                 });
        (void)combat_apply_damage_f(g, id, dmg, DMG_STORM, 0.0f, 0.0f);

        int32_t max_hops = 2 + g->internal.mods.chain_hops_add;
        if (max_hops > 15) {
            max_hops = 15;
        }
        const float range_mul =
            g->internal.mods.chain_range_mul > 0.0f ? g->internal.mods.chain_range_mul : 1.0f;
        const float hop_range = 160.0f * range_mul;
        const float hop_range2 = hop_range * hop_range;

        for (int32_t hop = 1; hop <= max_hops; ++hop) {
            dmg *= 0.7f;
            EntityId next_id = {ECS_INVALID_INDEX, 0};
            float next_d2 = hop_range2 + 1.0f;
            float next_x = 0.0f;
            float next_y = 0.0f;

            for (uint32_t j = 0; j < hw; ++j) {
                if (index_in_list(visited, visited_n, j)) {
                    continue;
                }
                const EntityId cand = ecs_entity_at(&g->ecs, j);
                if (cand.index == ECS_INVALID_INDEX) {
                    continue;
                }
                const TransformComp *ctr = game_transform(g, cand);
                const StatusComp *cst = game_status(g, cand);
                const EnemyComp *cen = game_enemy(g, cand);
                if (ctr == NULL || cst == NULL || cen == NULL || ctr->dissolving ||
                    (cen->elite_flags & ELITE_STORMPROOF) != 0) {
                    continue;
                }
                if (cst->ink_mark_t <= 0.0f && cst->charge_t <= 0.0f) {
                    continue;
                }
                const float dx = ctr->x - cur_x;
                const float dy = ctr->y - cur_y;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= hop_range2 && d2 < next_d2) {
                    next_d2 = d2;
                    next_id = cand;
                    next_x = ctr->x;
                    next_y = ctr->y;
                }
            }
            if (next_id.index == ECS_INVALID_INDEX) {
                break;
            }
            visited[visited_n++] = next_id.index;
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_STORM_ZAP,
                                         .x = cur_x,
                                         .y = cur_y,
                                         .x2 = next_x,
                                         .y2 = next_y,
                                         .value = hop,
                                         .flags = 0,
                                     });
            (void)combat_apply_damage_f(g, next_id, dmg, DMG_STORM, 0.0f, 0.0f);
            cur_x = next_x;
            cur_y = next_y;
        }
        break;
    }
}

static void elements_stage5_projectiles(GameState *g)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        ProjComp *proj = game_proj_mut(g, id);
        if (tr == NULL || vel == NULL || proj == NULL || tr->dissolving) {
            continue;
        }

        float dye[FLUID_MAX_CHANNELS] = {0};
        fluid_sample(&g->fluid, tr->x, tr->y, NULL, NULL, dye);
        const float ink = sanitize_sample(dye[0]);
        const float fire = sanitize_sample(dye[1]);
        const float storm = sanitize_sample(dye[2]);

        if (!proj->burning && fire > 0.36f && (proj->deflected || ink > 0.16f)) {
            proj->burning = true;
            proj->pierce = g->internal.mods.burning_proj_pierce;
            vel->vx *= 1.25f;
            vel->vy *= 1.25f;
            const float max_mul = proj->deflected ? g->internal.mods.deflected_max_speed_mul : 1.0f;
            clamp_proj_max_speed(vel, 640.0f * max_mul);
        }
        if (proj->deflected && !proj->electrified && storm > 0.25f) {
            proj->electrified = true;
        }
        if (!proj->deflected && !proj->accelerated && storm > 0.25f) {
            proj->accelerated = true;
            vel->vx *= 1.3f;
            vel->vy *= 1.3f;
            clamp_proj_max_speed(vel, proj->burning ? 640.0f : 365.0f);
        }
    }
}

void elements_step(GameState *g)
{
    if (g == NULL) {
        return;
    }
    if (g->internal.ignite_callout_cd > 0.0f) {
        g->internal.ignite_callout_cd -= INK_FIXED_DT;
        if (g->internal.ignite_callout_cd <= 1e-4f) {
            g->internal.ignite_callout_cd = 0.0f;
        }
    }
    if (g->internal.storm_zap_cd > 0.0f) {
        g->internal.storm_zap_cd -= INK_FIXED_DT;
        if (g->internal.storm_zap_cd <= 1e-4f) {
            g->internal.storm_zap_cd = 0.0f;
        }
    }

    /* Stage 1: cell reactions (Blazing Inkstorm + Tempest Lightning growth). */
    const int32_t storm_cells = elements_stage1_cells(g);

    /* Stage 2 (Cataclysm) and Stage 3 (Thunderburst) are reserved for Phase 4. */

    /* Stage 4: enemy statuses, burn ticks, and Tempest Lightning chains. */
    elements_stage4_enemies(g, storm_cells);

    /* Stage 5: projectile elemental upgrades and pre-deflect storm acceleration. */
    elements_stage5_projectiles(g);
}
