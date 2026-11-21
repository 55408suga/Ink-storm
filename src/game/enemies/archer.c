#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"

#include <math.h>
#include <stddef.h>

void archer_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->phase = (int32_t)(ink_rng_f01(&g->rng) * 6283.0f);
    en->fire_cd = 0.55f + 0.80f * ink_rng_f01(&g->rng);
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.42f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 3.6f, 0.0f, 0.0f, k_spawn_dye);
}

void archer_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || st == NULL || en == NULL || st->stun_t > 0.0f) {
        return;
    }

    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float ph = (float)en->phase * 0.001f;
    en->phase = (en->phase + 24) % 6283;

    if (tr->x < 120.0f || tr->x > (float)INK_SCREEN_W - 120.0f) {
        tr->y = ink_clampf(tr->y + sinf(ph) * 32.0f * slow * INK_FIXED_DT, 108.0f,
                           (float)INK_SCREEN_H - 96.0f);
    } else {
        tr->x = ink_clampf(tr->x + cosf(ph) * 40.0f * slow * INK_FIXED_DT, 165.0f,
                           (float)INK_SCREEN_W - 165.0f);
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    tr->facing = atan2f(sy - tr->y, sx - tr->x);

    en->fire_cd -= INK_FIXED_DT * slow;
    if (en->fire_cd <= 0.0f) {
        en->windup = 0.0f;
        (void)projectiles_spawn_dir(g, PROJ_ARROW, tr->x, tr->y, sx - tr->x, sy - tr->y, 250.0f);
        en->fire_cd = 1.15f + 0.70f * ink_rng_f01(&g->rng);
    } else if (en->fire_cd <= 0.35f) {
        en->windup = ink_clampf((0.35f - en->fire_cd) / 0.35f, 0.0f, 1.0f);
    } else {
        en->windup = 0.0f;
    }
}
