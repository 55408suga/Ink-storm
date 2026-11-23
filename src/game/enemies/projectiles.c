#include "enemies/projectiles.h"

#include "combat/damage.h"
#include "core.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

EntityId projectiles_spawn(GameState *g, ProjKind kind, float x, float y, float vx, float vy)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || (unsigned)kind >= (unsigned)PROJ_KIND_COUNT) {
        return invalid;
    }
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_VELOCITY | COMP_PROJ);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }

    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = 6.0f;
        tr->facing = atan2f(vy, vx);
    }
    VelocityComp *vel = game_velocity_mut(g, id);
    if (vel != NULL) {
        vel->vx = vx;
        vel->vy = vy;
        vel->fluid_coupling = (kind == PROJ_WIND_SICKLE) ? 1.5f : 1.0f;
    }
    ProjComp *proj = game_proj_mut(g, id);
    if (proj != NULL) {
        proj->kind = kind;
        proj->bounces_left = (kind == PROJ_BOUNCE_ORB) ? 2 : 0;
        proj->trail_n = 1;
        proj->trail[0] = (GameVec2){x, y};
    }
    return id;
}

EntityId projectiles_spawn_dir(GameState *g, ProjKind kind, float x, float y, float dx, float dy,
                               float speed)
{
    const float len = hypotf(dx, dy);
    const float nx = len > 1e-6f ? dx / len : 1.0f;
    const float ny = len > 1e-6f ? dy / len : 0.0f;
    return projectiles_spawn(g, kind, x, y, nx * speed, ny * speed);
}

static bool is_storm_proj_kind(ProjKind kind)
{
    return kind == PROJ_STORM_ORB || kind == PROJ_BOUNCE_ORB;
}

static void trigger_electrified_bounce(GameState *g, EntityId hit_enemy, float ox, float oy,
                                       uint32_t hw)
{
    int32_t max_targets = g->internal.mods.thunder_proj_targets;
    if (max_targets < 1) {
        max_targets = 1;
    } else if (max_targets > 8) {
        max_targets = 8;
    }

    uint32_t already_hit[8];
    int32_t hit_n = 0;

    for (int32_t t = 0; t < max_targets; ++t) {
        EntityId best = {ECS_INVALID_INDEX, 0};
        float best_d2 = 160.0f * 160.0f + 1.0f;
        float bx = 0.0f;
        float by = 0.0f;

        for (uint32_t j = 0; j < hw; ++j) {
            const EntityId cand = ecs_entity_at(&g->ecs, j);
            if (cand.index == ECS_INVALID_INDEX || cand.index == hit_enemy.index) {
                continue;
            }
            bool used = false;
            for (int32_t k = 0; k < hit_n; ++k) {
                if (already_hit[k] == cand.index) {
                    used = true;
                    break;
                }
            }
            if (used) {
                continue;
            }
            if (!game_enemy_is_targetable(g, cand)) {
                continue;
            }
            const TransformComp *tr = game_transform(g, cand);
            const EnemyComp *en = game_enemy(g, cand);
            if (tr == NULL || en == NULL || (en->elite_flags & ELITE_STORMPROOF) != 0) {
                continue;
            }
            const float dx = tr->x - ox;
            const float dy = tr->y - oy;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= 160.0f * 160.0f && d2 < best_d2) {
                best_d2 = d2;
                best = cand;
                bx = tr->x;
                by = tr->y;
            }
        }

        if (best.index == ECS_INVALID_INDEX) {
            break;
        }
        already_hit[hit_n++] = best.index;
        if (g->run.storm_zaps < META_MAX_COUNTERS) {
            ++g->run.storm_zaps;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_STORM_ZAP,
                                     .x = ox,
                                     .y = oy,
                                     .x2 = bx,
                                     .y2 = by,
                                     .value = 0,
                                     .flags = 0,
                                 });
        (void)combat_apply_damage(g, best, 10, DMG_STORM, 0.0f, 0.0f);
    }
}

void projectiles_step(GameState *g)
{
    if (g == NULL) {
        return;
    }
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float shrine_proj_r = g->shrine_count > 0 ? g->shrines[0].hit_radius_proj : 32.0f;
    const uint32_t hw = g->ecs.high_water;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TransformComp *tr = game_transform_mut(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        ProjComp *proj = game_proj_mut(g, id);
        if (tr == NULL || vel == NULL || proj == NULL || tr->dissolving) {
            continue;
        }

        proj->age += INK_FIXED_DT;

        float fu = 0.0f;
        float fv = 0.0f;
        float dye[FLUID_MAX_CHANNELS] = {0};
        fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, dye);
        const float fu_px = fu * FLUID_CELL_PX;
        const float fv_px = fv * FLUID_CELL_PX;
        const float fsp = hypotf(fu_px, fv_px);

        const float coupling = vel->fluid_coupling > 0.0f ? vel->fluid_coupling : 1.0f;
        vel->vx += fu_px * 5.0f * coupling * INK_FIXED_DT;
        vel->vy += fv_px * 5.0f * coupling * INK_FIXED_DT;

        const float tcx = sx - tr->x;
        const float tcy = sy - tr->y;
        const float tcd = hypotf(tcx, tcy);
        float psp = hypotf(vel->vx, vel->vy);

        if (!proj->deflected) {
            const float toward_center = (tcd > 1e-6f && psp > 1e-6f)
                                            ? ((vel->vx * tcx + vel->vy * tcy) / (psp * tcd))
                                            : 1.0f;
            /* Bounce orbs require actual fluid flow to deflect so wall bounces do not self-deflect.
             */
            const bool deflect_hit =
                (proj->kind == PROJ_BOUNCE_ORB)
                    ? (fsp > 48.0f && toward_center < 0.90f)
                    : ((fsp > 48.0f && toward_center < 0.90f) || toward_center < 0.28f);
            if (deflect_hit) {
                proj->deflected = true;
                if (is_storm_proj_kind(proj->kind) || g->internal.mods.deflect_auto_electrify) {
                    proj->electrified = true;
                }
                vel->vx += fu_px * 0.55f;
                vel->vy += fv_px * 0.55f;

                const float ddye[FLUID_MAX_CHANNELS] = {
                    0.35f,
                    proj->kind == PROJ_FIREBALL ? 0.72f : 0.16f,
                    is_storm_proj_kind(proj->kind) ? 0.85f : 0.0f,
                    0.0f,
                };
                fluid_splat(&g->fluid, tr->x, tr->y, 3.0f,
                            ink_clampf(vel->vx * 0.03f, -20.0f, 20.0f),
                            ink_clampf(vel->vy * 0.03f, -20.0f, 20.0f), ddye);

                const float nsp = hypotf(vel->vx, vel->vy);
                const float dx = nsp > 1e-6f ? vel->vx / nsp : 1.0f;
                const float dy = nsp > 1e-6f ? vel->vy / nsp : 0.0f;
                (void)game_push_event(g, (GameEvent){
                                             .type = EV_DEFLECT,
                                             .x = tr->x,
                                             .y = tr->y,
                                             .x2 = dx,
                                             .y2 = dy,
                                             .value = (int32_t)proj->kind,
                                             .flags = 0,
                                         });
            }
        }

        if (proj->deflected) {
            float best_score = 1e12f;
            EntityId best = {ECS_INVALID_INDEX, 0};
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
                const float ex = etr->x - tr->x;
                const float ey = etr->y - tr->y;
                const float d2 = ex * ex + ey * ey;
                if (d2 >= 620.0f * 620.0f) {
                    continue;
                }
                const bool is_boss_target =
                    game_is_boss_enemy(een->def_id) || game_is_boss_part(een->def_id);
                const float sc = d2 * (is_boss_target ? 0.65f : 1.0f);
                if (sc < best_score) {
                    best_score = sc;
                    best = eid;
                }
            }
            if (best.index != ECS_INVALID_INDEX) {
                const TransformComp *btr = game_transform(g, best);
                if (btr != NULL) {
                    const float ex = btr->x - tr->x;
                    const float ey = btr->y - tr->y;
                    const float ed = hypotf(ex, ey);
                    if (ed > 1e-6f) {
                        const float steer =
                            (proj->burning ? 840.0f : 700.0f) * g->internal.mods.homing_steer_mul;
                        vel->vx += (ex / ed) * steer * INK_FIXED_DT;
                        vel->vy += (ey / ed) * steer * INK_FIXED_DT;
                    }
                }
            }
        }

        psp = hypotf(vel->vx, vel->vy);
        const float base_max_sp = proj->burning ? 640.0f : (proj->deflected ? 550.0f : 365.0f);
        const float max_sp = proj->deflected
                                 ? (base_max_sp * g->internal.mods.deflected_max_speed_mul)
                                 : base_max_sp;
        const float target_sp = ink_clampf(psp, 180.0f, max_sp);
        if (psp > 1e-6f) {
            vel->vx = (vel->vx / psp) * target_sp;
            vel->vy = (vel->vy / psp) * target_sp;
        } else {
            vel->vx = 180.0f;
            vel->vy = 0.0f;
        }

        const float prev_x = tr->x;
        const float prev_y = tr->y;
        tr->x += vel->vx * INK_FIXED_DT;
        tr->y += vel->vy * INK_FIXED_DT;
        tr->facing = atan2f(vel->vy, vel->vx);

        const bool hit_outer =
            tr->x < FLUID_CELL_PX || tr->x >= (float)INK_SCREEN_W - FLUID_CELL_PX ||
            tr->y < FLUID_CELL_PX || tr->y >= (float)INK_SCREEN_H - FLUID_CELL_PX;
        const bool hit_solid = !hit_outer && fluid_is_solid(&g->fluid, tr->x, tr->y);

        if (proj->kind == PROJ_BOUNCE_ORB && (hit_outer || hit_solid)) {
            if (proj->bounces_left > 0) {
                --proj->bounces_left;
                float nx = 0.0f;
                float ny = 0.0f;
                if (hit_outer) {
                    if (tr->x < FLUID_CELL_PX) {
                        nx += 1.0f;
                    } else if (tr->x >= (float)INK_SCREEN_W - FLUID_CELL_PX) {
                        nx -= 1.0f;
                    }
                    if (tr->y < FLUID_CELL_PX) {
                        ny += 1.0f;
                    } else if (tr->y >= (float)INK_SCREEN_H - FLUID_CELL_PX) {
                        ny -= 1.0f;
                    }
                    const float nlen = hypotf(nx, ny);
                    if (nlen > 1e-6f) {
                        nx /= nlen;
                        ny /= nlen;
                    }
                } else if (!fluid_solid_normal(&g->fluid, tr->x, tr->y, &nx, &ny)) {
                    nx = -vel->vx / target_sp;
                    ny = -vel->vy / target_sp;
                }
                const float vn = vel->vx * nx + vel->vy * ny;
                if (vn < 0.0f) {
                    vel->vx -= 2.0f * vn * nx;
                    vel->vy -= 2.0f * vn * ny;
                }
                tr->x = ink_clampf(prev_x, FLUID_CELL_PX + 1.0f,
                                   (float)INK_SCREEN_W - FLUID_CELL_PX - 1.0f);
                tr->y = ink_clampf(prev_y, FLUID_CELL_PX + 1.0f,
                                   (float)INK_SCREEN_H - FLUID_CELL_PX - 1.0f);
                tr->facing = atan2f(vel->vy, vel->vx);
            } else {
                ecs_destroy(&g->ecs, id);
                continue;
            }
        } else if (hit_solid) {
            ecs_destroy(&g->ecs, id);
            continue;
        }

        if (proj->kind == PROJ_FIREBALL || proj->burning) {
            const float ignite = dye[0] > 0.15f ? 0.78f : 0.34f;
            const float fdye[FLUID_MAX_CHANNELS] = {0.0f, ignite, 0.0f, 0.0f};
            fluid_splat(&g->fluid, tr->x, tr->y, dye[0] > 0.15f ? 3.4f : 2.3f, vel->vx * 0.012f,
                        vel->vy * 0.012f, fdye);
        } else if (is_storm_proj_kind(proj->kind) || proj->electrified) {
            static const float k_sdye[FLUID_MAX_CHANNELS] = {0.0f, 0.0f, 0.65f, 0.0f};
            fluid_splat(&g->fluid, tr->x, tr->y, 2.6f, vel->vx * 0.01f, vel->vy * 0.01f, k_sdye);
        } else if (proj->deflected) {
            static const float k_ddye[FLUID_MAX_CHANNELS] = {0.22f, 0.06f, 0.0f, 0.0f};
            fluid_splat(&g->fluid, tr->x, tr->y, 1.9f, vel->vx * 0.01f, vel->vy * 0.01f, k_ddye);
        }

        if (proj->trail_n < PROJ_TRAIL_LEN) {
            proj->trail[proj->trail_n++] = (GameVec2){tr->x, tr->y};
        } else {
            memmove(proj->trail, proj->trail + 1, sizeof(GameVec2) * (PROJ_TRAIL_LEN - 1));
            proj->trail[PROJ_TRAIL_LEN - 1] = (GameVec2){tr->x, tr->y};
        }

        if (!proj->deflected && hypotf(sx - tr->x, sy - tr->y) < shrine_proj_r) {
            if (g->internal.barrier_shield_ready) {
                g->internal.barrier_shield_ready = false;
                g->internal.barrier_shield_cd = 12.0f;
            } else {
                combat_damage_shrine(g, tr->x, tr->y, 1);
            }
            ecs_destroy(&g->ecs, id);
            continue;
        }

        if (proj->deflected) {
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
                const float hit_r = game_is_boss_enemy(een->def_id)
                                        ? 46.0f
                                        : (game_is_boss_part(een->def_id) ? 36.0f : 31.0f);
                const float ex = etr->x - tr->x;
                const float ey = etr->y - tr->y;
                if (ex * ex + ey * ey < hit_r * hit_r) {
                    const int32_t dmg =
                        proj->burning ? 40 : (proj->kind == PROJ_FIREBALL ? 30 : 20);
                    const uint32_t flags =
                        DMG_DEFLECTED | (proj->burning ? DMG_FIRE : DMG_PHYSICAL);
                    const float hit_x = etr->x;
                    const float hit_y = etr->y;
                    (void)combat_apply_damage(g, eid, dmg, flags, vel->vx, vel->vy);

                    if (proj->kind == PROJ_FIREBALL) {
                        static const float k_hit_fire[FLUID_MAX_CHANNELS] = {0.0f, 0.7f, 0.0f,
                                                                             0.0f};
                        fluid_splat(&g->fluid, hit_x, hit_y, 3.0f, 0.0f, 0.0f, k_hit_fire);
                    }
                    if (proj->burning && g->internal.mods.burning_proj_hit_splat) {
                        static const float k_burn_hit[FLUID_MAX_CHANNELS] = {0.0f, 0.65f, 0.0f,
                                                                             0.0f};
                        fluid_splat(&g->fluid, hit_x, hit_y, 3.2f, 0.0f, 0.0f, k_burn_hit);
                    }
                    if (proj->electrified || is_storm_proj_kind(proj->kind)) {
                        trigger_electrified_bounce(g, eid, hit_x, hit_y, hw);
                    }

                    if (proj->pierce > 0) {
                        --proj->pierce;
                        tr->x += (vel->vx / target_sp) * 28.0f;
                        tr->y += (vel->vy / target_sp) * 28.0f;
                    } else {
                        ecs_destroy(&g->ecs, id);
                    }
                    break;
                }
            }
        }

        if (ecs_is_alive(&g->ecs, id) &&
            (tr->x < -45.0f || tr->x > (float)INK_SCREEN_W + 45.0f || tr->y < -45.0f ||
             tr->y > (float)INK_SCREEN_H + 45.0f || proj->age > 7.5f)) {
            ecs_destroy(&g->ecs, id);
        }
    }
}
