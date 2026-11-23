#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"

#include <math.h>
#include <stddef.h>

static EntityId pick_barrier_target(const GameState *g, EntityId self, float fx, float fy)
{
    EntityId best_unshielded = {ECS_INVALID_INDEX, 0};
    float best_unshielded_d2 = 360.0f * 360.0f + 1.0f;
    EntityId best_any = {ECS_INVALID_INDEX, 0};
    float best_any_d2 = 360.0f * 360.0f + 1.0f;

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId cand = ecs_entity_at(&g->ecs, i);
        if (cand.index == ECS_INVALID_INDEX || cand.index == self.index) {
            continue;
        }
        if (!game_enemy_is_targetable(g, cand)) {
            continue;
        }
        const TransformComp *ctr = game_transform(g, cand);
        const EnemyComp *cen = game_enemy(g, cand);
        const StatusComp *cst = game_status(g, cand);
        if (ctr == NULL || cen == NULL || cst == NULL) {
            continue;
        }
        if (game_is_boss_enemy(cen->def_id) || game_is_boss_part(cen->def_id)) {
            continue;
        }
        const float dx = ctr->x - fx;
        const float dy = ctr->y - fy;
        const float d2 = dx * dx + dy * dy;
        if (d2 > 360.0f * 360.0f) {
            continue;
        }
        if (cst->barrier_t <= 0.0f) {
            if (d2 < best_unshielded_d2) {
                best_unshielded_d2 = d2;
                best_unshielded = cand;
            }
        }
        if (d2 < best_any_d2) {
            best_any_d2 = d2;
            best_any = cand;
        }
    }

    return best_unshielded.index != ECS_INVALID_INDEX ? best_unshielded : best_any;
}

void ofuda_fox_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->phase = (int32_t)(ink_rng_f01(&g->rng) * 6283.0f);
    en->ai_state = (ink_rng_next(&g->rng) & UINT64_C(1)) != 0u ? 1 : -1;
    en->fire_cd = 1.8f;
    /* absorbed_ink tracks the 6.0s barrier-grant cooldown for Ofuda Fox. */
    en->absorbed_ink = 2.5f;
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.42f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 3.6f, 0.0f, 0.0f, k_spawn_dye);
}

void ofuda_fox_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || vel == NULL || st == NULL || en == NULL || st->stun_t > 0.0f) {
        return;
    }

    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float ph = (float)en->phase * 0.001f;
    en->phase = (en->phase + 24) % 6283;

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float rx = tr->x - sx;
    const float ry = tr->y - sy;
    const float r = hypotf(rx, ry);
    const float rad_x = r > 1e-6f ? rx / r : 1.0f;
    const float rad_y = r > 1e-6f ? ry / r : 0.0f;
    const float orbit_dir = en->ai_state >= 0 ? 1.0f : -1.0f;
    const float tan_x = -rad_y * orbit_dir;
    const float tan_y = rad_x * orbit_dir;

    /* Maintain outer orbit ring 260..340px around the shrine at 90px/s. */
    const float target_r = 300.0f + 40.0f * sinf(ph * 1.2f);
    const float radial_pull = ink_clampf((target_r - r) * 0.02f, -0.90f, 0.90f);
    float dx = tan_x + rad_x * radial_pull;
    float dy = tan_y + rad_y * radial_pull;
    const float dlen = hypotf(dx, dy);
    if (dlen > 1e-6f) {
        dx /= dlen;
        dy /= dlen;
    }

    tr->facing = atan2f(sy - tr->y, sx - tr->x);

    if (st->knockback_t <= 0.0f) {
        const float seek_sp = 90.0f * slow;
        vel->vx += (dx * seek_sp - vel->vx) * 4.5f * INK_FIXED_DT;
        vel->vy += (dy * seek_sp - vel->vy) * 4.5f * INK_FIXED_DT;
    }

    float fu = 0.0f;
    float fv = 0.0f;
    fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
    vel->vx += fu * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;
    vel->vy += fv * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;

    /* Ofuda Barrier: every 6.0s grant 4.0s barrier (-50% dmg) to nearest ally within 360px. */
    en->absorbed_ink -= INK_FIXED_DT * slow;
    if (en->absorbed_ink <= 0.0f) {
        const EntityId ally = pick_barrier_target(g, id, tr->x, tr->y);
        if (ally.index != ECS_INVALID_INDEX) {
            StatusComp *target_st = game_status_mut(g, ally);
            if (target_st != NULL) {
                target_st->barrier_t = 4.0f;
            }
            en->absorbed_ink = 6.0f;
        } else {
            en->absorbed_ink = 1.0f;
        }
    }

    /* Homing Ofuda shot: every 3.0s (0.4s windup). */
    en->fire_cd -= INK_FIXED_DT * slow;
    if (en->fire_cd <= 0.0f) {
        en->windup = 0.0f;
        (void)projectiles_spawn_dir(g, PROJ_HOMING_OFUDA, tr->x, tr->y, sx - tr->x, sy - tr->y,
                                    140.0f);
        en->fire_cd = 3.0f;
    } else if (en->fire_cd <= 0.4f) {
        en->windup = ink_clampf((0.4f - en->fire_cd) / 0.4f, 0.0f, 1.0f);
    } else {
        en->windup = 0.0f;
    }
}
