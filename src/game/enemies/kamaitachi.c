#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"

#include <math.h>
#include <stddef.h>

static void pick_vortex_target(const GameState *g, float ex, float ey, float *out_x, float *out_y,
                               float *out_spin)
{
    /* Prefer an active player cyclone so Kamaitachi actively disrupts player vortices. */
    float best_life = 0.0f;
    float best_spin = 1.0f;
    bool found = false;
    float vx = ex;
    float vy = ey;
    for (int32_t i = 0; i < GAME_MAX_ACTIVE_CYCLONES; ++i) {
        const ActiveCyclone *ac = &g->internal.cyclones[i];
        if (ac->active && ac->life > best_life && !fluid_is_solid(&g->fluid, ac->x, ac->y)) {
            best_life = ac->life;
            best_spin = ac->spin < 0.0f ? -1.0f : 1.0f;
            vx = ac->x;
            vy = ac->y;
            found = true;
        }
    }
    *out_spin = 1.0f;
    if (found) {
        *out_x = vx;
        *out_y = vy;
        *out_spin = best_spin;
        return;
    }

    /* Otherwise target the densest non-solid ink patch on the canvas (coarse stride 4). */
    const float *ink_ch = fluid_channel_const(&g->fluid, 0);
    if (ink_ch != NULL) {
        float best_ink = 0.15f;
        for (int gy = 4; gy < FLUID_H - 4; gy += 4) {
            const int row = gy * FLUID_W;
            for (int gx = 4; gx < FLUID_W - 4; gx += 4) {
                const int idx = row + gx;
                if (g->fluid.solid[idx] != 0) {
                    continue;
                }
                const float iv = ink_ch[idx];
                if (isfinite(iv) && iv > best_ink) {
                    best_ink = iv;
                    vx = ((float)gx + 0.5f) * FLUID_CELL_PX;
                    vy = ((float)gy + 0.5f) * FLUID_CELL_PX;
                    found = true;
                }
            }
        }
    }
    if (found) {
        *out_x = vx;
        *out_y = vy;
        return;
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float mx = 0.5f * (ex + sx);
    const float my = 0.5f * (ey + sy);
    if (!fluid_is_solid(&g->fluid, mx, my)) {
        *out_x = mx;
        *out_y = my;
    } else {
        *out_x = ex;
        *out_y = ey;
    }
}

void kamaitachi_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->phase = (int32_t)(ink_rng_f01(&g->rng) * 6283.0f);
    en->ai_state = (ink_rng_next(&g->rng) & UINT64_C(1)) != 0u ? 1 : -1;
    en->fire_cd = 1.2f + 0.8f * ink_rng_f01(&g->rng);
    /* absorbed_ink tracks the 4.0s reverse-vortex cooldown for Kamaitachi. */
    en->absorbed_ink = 2.0f + 1.5f * ink_rng_f01(&g->rng);
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.42f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 3.6f, 0.0f, 0.0f, k_spawn_dye);
}

void kamaitachi_step(GameState *g, EntityId id)
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
    en->phase = (en->phase + 48) % 6283;

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

    const float target_r = 240.0f + 50.0f * sinf(ph * 2.4f);
    const float radial_pull = ink_clampf((target_r - r) * 0.015f, -0.85f, 0.85f);
    float dx = tan_x + rad_x * radial_pull;
    float dy = tan_y + rad_y * radial_pull;
    const float dlen = hypotf(dx, dy);
    if (dlen > 1e-6f) {
        dx /= dlen;
        dy /= dlen;
    }

    tr->facing = atan2f(sy - tr->y, sx - tr->x);

    if (st->knockback_t <= 0.0f) {
        const float seek_sp = 160.0f * slow;
        vel->vx += (dx * seek_sp - vel->vx) * 4.5f * INK_FIXED_DT;
        vel->vy += (dy * seek_sp - vel->vy) * 4.5f * INK_FIXED_DT;
    }

    float fu = 0.0f;
    float fv = 0.0f;
    fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
    vel->vx += fu * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;
    vel->vy += fv * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;

    en->fire_cd -= INK_FIXED_DT * slow;
    if (en->fire_cd <= 0.0f) {
        en->windup = 0.0f;
        (void)projectiles_spawn_dir(g, PROJ_WIND_SICKLE, tr->x, tr->y, sx - tr->x, sy - tr->y,
                                    300.0f);
        en->fire_cd = 2.5f;
    } else if (en->fire_cd <= 0.35f) {
        en->windup = ink_clampf((0.35f - en->fire_cd) / 0.35f, 0.0f, 1.0f);
    } else {
        en->windup = 0.0f;
    }

    en->absorbed_ink -= INK_FIXED_DT * slow;
    if (en->absorbed_ink <= 0.0f) {
        en->absorbed_ink = 4.0f;
        float vx = tr->x;
        float vy = tr->y;
        float target_spin = 1.0f;
        pick_vortex_target(g, tr->x, tr->y, &vx, &vy, &target_spin);
        const EntityId vid = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
        if (vid.index != ECS_INVALID_INDEX) {
            TransformComp *vtr = game_transform_mut(g, vid);
            if (vtr != NULL) {
                vtr->x = vx;
                vtr->y = vy;
                vtr->radius = 80.0f;
            }
            GimmickComp *vgim = game_gimmick_mut(g, vid);
            if (vgim != NULL) {
                vgim->kind = (int32_t)GIMMICK_KAMAITACHI_VORTEX;
                vgim->elem_kind = (int32_t)GIMMICK_ELEM_NONE;
                vgim->timer = 1.0f;
                vgim->param_a = target_spin < 0.0f ? 1.0f : -1.0f;
            }
        }
    }
}
