#include "core.h"
#include "enemies/enemy_defs.h"

#include <math.h>
#include <stddef.h>

static float sample_ink_at(const GameState *g, float x, float y)
{
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, x, y, NULL, NULL, dye);
    return isfinite(dye[0]) ? dye[0] : 0.0f;
}

void ink_leech_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || en == NULL) {
        return;
    }
    en->absorbed_ink = 0.0f;
    en->fire_cd = 0.0f;
    en->windup = 0.0f;

    static const float k_spawn_dye[FLUID_MAX_CHANNELS] = {0.42f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 3.6f, 0.0f, 0.0f, k_spawn_dye);
}

void ink_leech_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    HealthComp *hp = game_health_mut(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || st->stun_t > 0.0f) {
        return;
    }

    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;

    /* Drift along ink density gradient (+/- 48px), falling back to shrine direction. */
    const float ink_r = sample_ink_at(g, tr->x + 48.0f, tr->y);
    const float ink_l = sample_ink_at(g, tr->x - 48.0f, tr->y);
    const float ink_d = sample_ink_at(g, tr->x, tr->y + 48.0f);
    const float ink_u = sample_ink_at(g, tr->x, tr->y - 48.0f);
    const float gx = ink_r - ink_l;
    const float gy = ink_d - ink_u;
    const float glen = hypotf(gx, gy);

    float dir_x = 0.0f;
    float dir_y = 0.0f;
    if (glen > 0.03f) {
        dir_x = gx / glen;
        dir_y = gy / glen;
    } else {
        const float dx = sx - tr->x;
        const float dy = sy - tr->y;
        const float d = hypotf(dx, dy);
        dir_x = d > 1e-6f ? dx / d : 1.0f;
        dir_y = d > 1e-6f ? dy / d : 0.0f;
    }
    tr->facing = atan2f(dir_y, dir_x);

    if (st->knockback_t <= 0.0f) {
        const float move_sp = 60.0f * slow;
        vel->vx += (dir_x * move_sp - vel->vx) * 3.5f * INK_FIXED_DT;
        vel->vy += (dir_y * move_sp - vel->vy) * 3.5f * INK_FIXED_DT;
    }

    float fu = 0.0f;
    float fv = 0.0f;
    fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
    vel->vx += fu * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;
    vel->vy += fv * FLUID_CELL_PX * vel->fluid_coupling * INK_FIXED_DT;

    /* Absorb 30%/s of ink (ch0) from non-solid cells within radius 64px (8 cells). */
    float *ink_ch = fluid_channel_mut(&g->fluid, 0);
    if (ink_ch != NULL) {
        const int cx = (int)(tr->x / FLUID_CELL_PX);
        const int cy = (int)(tr->y / FLUID_CELL_PX);
        const int x0 = cx - 8 < 1 ? 1 : cx - 8;
        const int x1 = cx + 8 > FLUID_W - 2 ? FLUID_W - 2 : cx + 8;
        const int y0 = cy - 8 < 1 ? 1 : cy - 8;
        const int y1 = cy + 8 > FLUID_H - 2 ? FLUID_H - 2 : cy + 8;
        const float absorb_frac = 0.30f * INK_FIXED_DT;
        float sum_taken = 0.0f;
        int32_t cells = 0;

        for (int gy_i = y0; gy_i <= y1; ++gy_i) {
            const float cell_y = ((float)gy_i + 0.5f) * FLUID_CELL_PX;
            const float dy = cell_y - tr->y;
            const int row = gy_i * FLUID_W;
            for (int gx_i = x0; gx_i <= x1; ++gx_i) {
                const int idx = row + gx_i;
                if (g->fluid.solid[idx] != 0) {
                    continue;
                }
                const float cell_x = ((float)gx_i + 0.5f) * FLUID_CELL_PX;
                const float dx = cell_x - tr->x;
                if (dx * dx + dy * dy > 64.0f * 64.0f) {
                    continue;
                }
                ++cells;
                const float cur = ink_ch[idx];
                if (isfinite(cur) && cur > 0.0f) {
                    const float taken = cur * absorb_frac;
                    ink_ch[idx] = cur - taken;
                    sum_taken += taken;
                }
            }
        }

        if (cells > 0 && sum_taken > 0.0f) {
            const float gain = (sum_taken / (float)cells) * 4.0f;
            en->absorbed_ink = ink_clampf(en->absorbed_ink + gain, 0.0f, 10.0f);
            en->fire_cd += gain;
            while (en->fire_cd >= 1.0f) {
                en->fire_cd -= 1.0f;
                hp->hp += 5;
                if (hp->hp > hp->max_hp) {
                    hp->hp = hp->max_hp;
                }
            }
            const float base_r = en->is_elite ? (19.0f * 1.2f) : 19.0f;
            tr->radius = base_r + fminf(10.0f, en->absorbed_ink * 2.5f);
        }
    }
}
