#include "core.h"
#include "enemies/enemy_defs.h"

#include <math.h>
#include <stddef.h>

void stone_oni_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->ai_state = 0;
    en->fire_cd = 0.0f;
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.55f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 4.2f, 0.0f, 0.0f, k_spawn_dye);
}

void stone_oni_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || vel == NULL || st == NULL || en == NULL) {
        return;
    }
    if (st->stun_t > 0.0f) {
        if (st->knockback_t <= 0.0f) {
            vel->vx *= 0.85f;
            vel->vy *= 0.85f;
        }
        return;
    }

    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float dx = sx - tr->x;
    const float dy = sy - tr->y;
    const float d = hypotf(dx, dy);
    const float dir_x = d > 1e-6f ? dx / d : 1.0f;
    const float dir_y = d > 1e-6f ? dy / d : 0.0f;
    tr->facing = atan2f(dir_y, dir_x);

    if (st->knockback_t <= 0.0f) {
        if (en->ai_state == 1) {
            en->fire_cd -= INK_FIXED_DT;
            const float retreat_sp = 55.0f * slow;
            vel->vx += (-dir_x * retreat_sp - vel->vx) * 5.0f * INK_FIXED_DT;
            vel->vy += (-dir_y * retreat_sp - vel->vy) * 5.0f * INK_FIXED_DT;
            if (en->fire_cd <= 1e-4f) {
                en->ai_state = 0;
                en->fire_cd = 0.0f;
            }
        } else {
            const float walk_sp = 40.0f * slow;
            vel->vx += (dir_x * walk_sp - vel->vx) * 4.0f * INK_FIXED_DT;
            vel->vy += (dir_y * walk_sp - vel->vy) * 4.0f * INK_FIXED_DT;
        }
    }

    float fu = 0.0f;
    float fv = 0.0f;
    fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
    vel->vx += fu * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;
    vel->vy += fv * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;
}
