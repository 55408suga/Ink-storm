#include "core.h"
#include "enemies/enemy_defs.h"

#include <math.h>
#include <stddef.h>

void umbrella_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->phase = (int32_t)(ink_rng_f01(&g->rng) * 6283.0f);
    en->fire_cd = 0.0f;
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.42f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 3.6f, 0.0f, 0.0f, k_spawn_dye);
}

void umbrella_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    StatusComp *st = game_status_mut(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || vel == NULL || st == NULL || en == NULL || st->stun_t > 0.0f) {
        return;
    }

    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float ph = (float)en->phase * 0.001f;
    en->phase = (en->phase + 24) % 6283;

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float dx = sx - tr->x;
    const float dy = sy - tr->y;
    const float d = hypotf(dx, dy);
    const float dir_x = d > 1e-6f ? dx / d : 1.0f;
    const float dir_y = d > 1e-6f ? dy / d : 0.0f;
    tr->facing = atan2f(dir_y, dir_x);

    const float hop = 0.65f + 0.55f * fabsf(sinf(ph * 3.1f));
    const float seek_sp = 96.0f * slow * hop;
    vel->vx += (dir_x * seek_sp - vel->vx) * 2.1f * INK_FIXED_DT;
    vel->vy += (dir_y * seek_sp - vel->vy) * 2.1f * INK_FIXED_DT;

    float fu = 0.0f;
    float fv = 0.0f;
    fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
    const float fu_px = fu * FLUID_CELL_PX;
    const float fv_px = fv * FLUID_CELL_PX;
    vel->vx += fu_px * vel->fluid_coupling * INK_FIXED_DT;
    vel->vy += fv_px * vel->fluid_coupling * INK_FIXED_DT;

    if (vel->mass_tier == MASS_LIGHT && hypotf(fu_px, fv_px) > 105.0f && st->knockback_t < 0.90f) {
        st->knockback_t = 0.90f;
    }
}
