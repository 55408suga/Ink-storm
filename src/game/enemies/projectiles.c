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
        if (kind == PROJ_WIND_SICKLE) {
            vel->fluid_coupling = 1.5f;
        } else if (kind == PROJ_HOMING_OFUDA) {
            vel->fluid_coupling = 0.6f;
        } else {
            vel->fluid_coupling = 1.0f;
        }
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

static bool proj_already_hit(const ProjComp *proj, EntityId target)
{
    const int32_t n = proj->hit_n < PROJ_MAX_HIT_IDS ? proj->hit_n : PROJ_MAX_HIT_IDS;
    for (int32_t k = 0; k < n; ++k) {
        if (proj->hit_ids[k].index == target.index && proj->hit_ids[k].gen == target.gen) {
            return true;
        }
    }
    return false;
}

static void proj_remember_hit(ProjComp *proj, EntityId target)
{
    if (proj->hit_n >= 0 && proj->hit_n < PROJ_MAX_HIT_IDS) {
        proj->hit_ids[proj->hit_n++] = target;
    }
}

static void block_with_barrier(GameState *g, float px, float py)
{
    g->internal.barrier_shield_ready = false;
    g->internal.barrier_shield_cd = 12.0f;
    if (g->shrine_count <= 0) {
        return;
    }
    const ShrineState *sh = &g->shrines[0];
    static const float k_ward_dye[FLUID_MAX_CHANNELS] = {0.18f, 0.0f, 0.0f, 0.0f};
    fluid_swirl(&g->fluid, sh->x, sh->y, 9.0f, 30.0f, k_ward_dye);
    const float dx = px - sh->x;
    const float dy = py - sh->y;
    const float d = hypotf(dx, dy);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_SHRINE_HIT,
                                 .x = sh->x,
                                 .y = sh->y,
                                 .x2 = d > 1e-6f ? dx / d : 0.0f,
                                 .y2 = d > 1e-6f ? dy / d : 0.0f,
                                 .value = sh->hp,
                                 .flags = EVF_SHRINE_BLOCKED,
                             });
}

static void ensure_enemy_cache(const GameState *g, uint32_t hw, EntityId ids[ECS_MAX_ENTITIES],
                               uint32_t *count, bool *built)
{
    if (*built) {
        return;
    }
    uint32_t n = 0;
    for (uint32_t j = 0; j < hw; ++j) {
        const EntityId eid = ecs_entity_at(&g->ecs, j);
        if (eid.index != ECS_INVALID_INDEX &&
            ecs_has_mask(&g->ecs, eid, COMP_ENEMY | COMP_TRANSFORM)) {
            ids[n++] = eid;
        }
    }
    *count = n;
    *built = true;
}

static void trigger_electrified_bounce(GameState *g, EntityId hit_enemy, float ox, float oy,
                                       const EntityId *enemy_ids, uint32_t enemy_count)
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

        for (uint32_t k = 0; k < enemy_count; ++k) {
            const EntityId cand = enemy_ids[k];
            if (cand.index == ECS_INVALID_INDEX || cand.index == hit_enemy.index) {
                continue;
            }
            bool used = false;
            for (int32_t m = 0; m < hit_n; ++m) {
                if (already_hit[m] == cand.index) {
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
            if (tr == NULL || en == NULL) {
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

static bool is_outer_border_cell(int cx, int cy)
{
    return cx <= 0 || cx >= FLUID_W - 1 || cy <= 0 || cy >= FLUID_H - 1;
}

static bool is_in_outer_border(float x, float y)
{
    return x < FLUID_CELL_PX || x >= (float)INK_SCREEN_W - FLUID_CELL_PX || y < FLUID_CELL_PX ||
           y >= (float)INK_SCREEN_H - FLUID_CELL_PX;
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
    EntityId enemy_ids[ECS_MAX_ENTITIES];
    uint32_t enemy_count = 0;
    bool enemy_cache_built = false;

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

        /* Undeflected homing ofuda burns up immediately in fire > 0.30 without hitting the shrine.
         */
        if (!proj->deflected && proj->kind == PROJ_HOMING_OFUDA && isfinite(dye[1]) &&
            dye[1] > 0.30f) {
            ecs_destroy(&g->ecs, id);
            continue;
        }

        const float fu_px = fu * FLUID_CELL_PX;
        const float fv_px = fv * FLUID_CELL_PX;
        const float fsp = hypotf(fu_px, fv_px);
        if (fsp > 35.0f) {
            proj->fluid_pushed = true;
        }

        const float coupling = vel->fluid_coupling > 0.0f ? vel->fluid_coupling : 1.0f;
        float add_fx = fu_px;
        float add_fy = fv_px;
        if (!proj->deflected) {
            const float cur_sp = hypotf(vel->vx, vel->vy);
            if (cur_sp > 1e-6f) {
                const float ux = vel->vx / cur_sp;
                const float uy = vel->vy / cur_sp;
                const float f_par = fu_px * ux + fv_px * uy;
                if (f_par > 0.0f) {
                    /* A fluid moving slower than an incoming projectile cannot tailwind-boost it
                     * (prevents fireballs/storm orbs from self-accelerating on their own wake),
                     * while crosswinds, headwinds, and faster gusts apply in full. */
                    const float eff_par = fmaxf(0.0f, f_par - cur_sp);
                    const float drop = f_par - eff_par;
                    add_fx -= ux * drop;
                    add_fy -= uy * drop;
                }
            }
        }
        vel->vx += add_fx * 5.0f * coupling * INK_FIXED_DT;
        vel->vy += add_fy * 5.0f * coupling * INK_FIXED_DT;

        const float tcx = sx - tr->x;
        const float tcy = sy - tr->y;
        const float tcd = hypotf(tcx, tcy);
        float psp = hypotf(vel->vx, vel->vy);

        if (!proj->deflected) {
            const float toward_center = (tcd > 1e-6f && psp > 1e-6f)
                                            ? ((vel->vx * tcx + vel->vy * tcy) / (psp * tcd))
                                            : 1.0f;
            /* Turning away from the shrine only counts as a deflect when ink actually pushed the
             * shot; fan volleys and wall bounces otherwise drift past at fsp == 0. */
            const bool fluid_turned = proj->fluid_pushed || fsp > 20.0f;
            const bool deflect_hit = (proj->kind == PROJ_BOUNCE_ORB)
                                         ? (fsp > 48.0f && toward_center < 0.90f)
                                         : ((fsp > 48.0f && toward_center < 0.90f) ||
                                            (toward_center < 0.28f && fluid_turned));
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

        if (!proj->deflected && proj->kind == PROJ_HOMING_OFUDA && tcd > 1e-6f && psp > 1e-6f) {
            /* Steer toward the shrine at up to 90 deg/s while preserving current speed. */
            const float cur_ang = atan2f(vel->vy, vel->vx);
            const float want_ang = atan2f(tcy, tcx);
            float diff = want_ang - cur_ang;
            const float k_pi = 3.14159265f;
            const float k_two_pi = 6.2831853f;
            if (diff > k_pi) {
                diff -= k_two_pi;
            } else if (diff < -k_pi) {
                diff += k_two_pi;
            }
            const float max_turn = 1.5707963f * INK_FIXED_DT;
            diff = ink_clampf(diff, -max_turn, max_turn);
            const float new_ang = cur_ang + diff;
            vel->vx = cosf(new_ang) * psp;
            vel->vy = sinf(new_ang) * psp;
        }

        if (proj->deflected) {
            ensure_enemy_cache(g, hw, enemy_ids, &enemy_count, &enemy_cache_built);
            float best_score = 1e12f;
            EntityId best = {ECS_INVALID_INDEX, 0};
            for (uint32_t k = 0; k < enemy_count; ++k) {
                const EntityId eid = enemy_ids[k];
                if (eid.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, eid) ||
                    proj_already_hit(proj, eid)) {
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
        const float base_max_sp = proj->deflected ? (proj->burning ? 640.0f : 550.0f) : 365.0f;
        const float max_sp = proj->deflected
                                 ? (base_max_sp * g->internal.mods.deflected_max_speed_mul)
                                 : base_max_sp;
        const float min_sp =
            (!proj->deflected && proj->kind == PROJ_HOMING_OFUDA) ? 140.0f : 180.0f;
        const float target_sp = ink_clampf(psp, min_sp, max_sp);
        if (psp > 1e-6f) {
            vel->vx = (vel->vx / psp) * target_sp;
            vel->vy = (vel->vy / psp) * target_sp;
        } else {
            vel->vx = min_sp;
            vel->vy = 0.0f;
        }

        const float prev_x = tr->x;
        const float prev_y = tr->y;
        const float end_x = prev_x + vel->vx * INK_FIXED_DT;
        const float end_y = prev_y + vel->vy * INK_FIXED_DT;
        tr->facing = atan2f(vel->vy, vel->vx);

        const FluidRayHit ray = fluid_trace_solid_segment(&g->fluid, prev_x, prev_y, end_x, end_y);
        const bool hit_outer_cell = ray.hit && is_outer_border_cell(ray.cell_x, ray.cell_y);
        const bool hit_interior_solid = ray.hit && !hit_outer_cell;
        const bool hit_outer = hit_outer_cell || (!ray.hit && is_in_outer_border(end_x, end_y));

        if (proj->kind == PROJ_BOUNCE_ORB && (hit_outer || hit_interior_solid)) {
            if (proj->bounces_left > 0) {
                --proj->bounces_left;
                float nx = ray.hit ? ray.nx : 0.0f;
                float ny = ray.hit ? ray.ny : 0.0f;
                if (!ray.hit && hit_outer) {
                    if (end_x < FLUID_CELL_PX) {
                        nx += 1.0f;
                    } else if (end_x >= (float)INK_SCREEN_W - FLUID_CELL_PX) {
                        nx -= 1.0f;
                    }
                    if (end_y < FLUID_CELL_PX) {
                        ny += 1.0f;
                    } else if (end_y >= (float)INK_SCREEN_H - FLUID_CELL_PX) {
                        ny -= 1.0f;
                    }
                    const float nlen = hypotf(nx, ny);
                    if (nlen > 1e-6f) {
                        nx /= nlen;
                        ny /= nlen;
                    }
                }
                if (nx == 0.0f && ny == 0.0f) {
                    nx = -vel->vx / target_sp;
                    ny = -vel->vy / target_sp;
                }
                const float vn = vel->vx * nx + vel->vy * ny;
                if (vn < 0.0f) {
                    vel->vx -= 2.0f * vn * nx;
                    vel->vy -= 2.0f * vn * ny;
                }
                const float safe_x = ray.hit ? ray.hit_x : prev_x;
                const float safe_y = ray.hit ? ray.hit_y : prev_y;
                tr->x = ink_clampf(safe_x, FLUID_CELL_PX + 1.0f,
                                   (float)INK_SCREEN_W - FLUID_CELL_PX - 1.0f);
                tr->y = ink_clampf(safe_y, FLUID_CELL_PX + 1.0f,
                                   (float)INK_SCREEN_H - FLUID_CELL_PX - 1.0f);
                tr->facing = atan2f(vel->vy, vel->vx);
            } else {
                ecs_destroy(&g->ecs, id);
                continue;
            }
        } else if (hit_interior_solid) {
            ecs_destroy(&g->ecs, id);
            continue;
        } else {
            tr->x = end_x;
            tr->y = end_y;
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
            if (g->internal.barrier_shield_ready && game_shrine_can_be_hit(g)) {
                block_with_barrier(g, tr->x, tr->y);
            } else {
                combat_damage_shrine(g, tr->x, tr->y, 1);
            }
            ecs_destroy(&g->ecs, id);
            continue;
        }

        if (proj->deflected) {
            /* Phase 1 warded Ryu head emits a rate-limited ping while letting shots pass through
             * to orbs behind it. */
            const EntityId ryu_head = g->ryu_body.head_id;
            if (ryu_head.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, ryu_head) &&
                g->internal.ryu_ward_ping_cd <= 0.0f) {
                const BossComp *rboss = game_boss(g, ryu_head);
                const TransformComp *rtr = game_transform(g, ryu_head);
                if (rboss != NULL && rtr != NULL && rboss->phase_idx == 0) {
                    const float rx = rtr->x - tr->x;
                    const float ry = rtr->y - tr->y;
                    if (rx * rx + ry * ry < 46.0f * 46.0f) {
                        const uint8_t m = g->ryu_body.orb_broken_mask;
                        const int32_t broken =
                            (int32_t)((m & 1u) + ((m >> 1) & 1u) + ((m >> 2) & 1u));
                        const int32_t rem = GAME_RYU_ORB_COUNT - broken;
                        (void)game_push_event(g, (GameEvent){
                                                     .type = EV_BOSS_WARD,
                                                     .x = tr->x,
                                                     .y = tr->y,
                                                     .value = rem > 0 ? rem : 0,
                                                     .flags = 0,
                                                 });
                        g->internal.ryu_ward_ping_cd = 0.5f;
                    }
                }
            }

            ensure_enemy_cache(g, hw, enemy_ids, &enemy_count, &enemy_cache_built);
            for (uint32_t k = 0; k < enemy_count; ++k) {
                const EntityId eid = enemy_ids[k];
                if (eid.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, eid)) {
                    continue;
                }
                const TransformComp *etr = game_transform(g, eid);
                const EnemyComp *een = game_enemy(g, eid);
                if (etr == NULL || een == NULL || proj_already_hit(proj, eid)) {
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
                    const bool fiery = proj->burning || proj->kind == PROJ_FIREBALL;
                    const uint32_t flags = DMG_DEFLECTED | (fiery ? DMG_FIRE : DMG_PHYSICAL);
                    const float hit_x = etr->x;
                    const float hit_y = etr->y;
                    proj_remember_hit(proj, eid);
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
                        trigger_electrified_bounce(g, eid, hit_x, hit_y, enemy_ids, enemy_count);
                    }

                    if (proj->pierce > 0) {
                        --proj->pierce;
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
