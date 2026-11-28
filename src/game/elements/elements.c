#include "elements/elements.h"

#include "combat/damage.h"
#include "core.h"
#include "roguelike/upgrades.h"

#include <float.h>
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

static float mass_explosion_multiplier(const GameState *g, const VelocityComp *vel,
                                       const EnemyComp *en)
{
    if (en != NULL && en->def_id == ENEMY_BOSS_NUE && en->ai_state == 1) {
        return 0.55f;
    }
    if (vel == NULL) {
        return 0.0f;
    }
    switch (game_effective_mass_tier(g, vel->mass_tier)) {
    case MASS_LIGHT:
        return 1.0f;
    case MASS_MEDIUM:
        return 0.75f;
    case MASS_HEAVY:
        return 0.55f;
    case MASS_TURRET:
    case MASS_BOSS:
    default:
        return 0.0f;
    }
}

static void decay_cd(float *cd)
{
    if (*cd > 0.0f) {
        *cd -= INK_FIXED_DT;
        if (*cd <= 1e-4f) {
            *cd = 0.0f;
        }
    }
}

typedef struct {
    float fire;
    float storm;
} ElementPeaks;

static float channel_peak(const float *ch)
{
    float peak = 0.0f;
    for (int i = 0; i < FLUID_N; ++i) {
        peak = ch[i] > peak ? ch[i] : peak;
    }
    return peak;
}

static int reaction_block_index(float px, float py)
{
    int bx = (int)(px / (float)GAME_REACTION_BLOCK_PX);
    int by = (int)(py / (float)GAME_REACTION_BLOCK_PX);
    bx = bx < 0 ? 0 : (bx >= GAME_THUNDERBURST_BLOCKS_X ? GAME_THUNDERBURST_BLOCKS_X - 1 : bx);
    by = by < 0 ? 0 : (by >= GAME_THUNDERBURST_BLOCKS_Y ? GAME_THUNDERBURST_BLOCKS_Y - 1 : by);
    return by * GAME_THUNDERBURST_BLOCKS_X + bx;
}

static void end_channel_borrows(FluidState *f, bool b0, bool b1, bool b2)
{
    if (b0) {
        fluid_end_mut_borrow(f);
    }
    if (b1) {
        fluid_end_mut_borrow(f);
    }
    if (b2) {
        fluid_end_mut_borrow(f);
    }
}

static int32_t elements_stage1_cells(GameState *g, ElementPeaks *peaks, bool *storm_dirty)
{
    float *ink_ch = fluid_channel_mut(&g->fluid, 0);
    float *fire_ch = fluid_channel_mut(&g->fluid, 1);
    float *storm_ch = fluid_channel_mut(&g->fluid, 2);
    if (ink_ch == NULL || fire_ch == NULL || storm_ch == NULL) {
        end_channel_borrows(&g->fluid, ink_ch != NULL, fire_ch != NULL, storm_ch != NULL);
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
    float peak_fire = 0.0f;
    float peak_storm = 0.0f;

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
                /* Later stages still read the finite fire/storm half of such a cell. */
                peak_fire = fire > peak_fire ? fire : peak_fire;
                peak_storm = storm > peak_storm ? storm : peak_storm;
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
            peak_fire = fire > peak_fire ? fire : peak_fire;
            peak_storm = storm > peak_storm ? storm : peak_storm;
        }
    }
    end_channel_borrows(&g->fluid, true, true, true);
    peaks->fire = peak_fire;
    peaks->storm = peak_storm;

    const bool blazing = (float)blazing_cells >= 24.0f * GAME_CELL_COUNT_SCALE;
    g->internal.blazing_active = blazing;
    if (blazing && g->internal.ignite_callout_cd <= 0.0f) {
        g->internal.ignite_callout_cd = 3.2f;
        if (g->run.ignitions < META_MAX_COUNTERS) {
            ++g->run.ignitions;
        }
        const float cx = blaze_sum_x / (float)blazing_cells;
        const float cy = blaze_sum_y / (float)blazing_cells;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_IGNITE,
                                     .x = cx,
                                     .y = cy,
                                     .value = blazing_cells,
                                     .flags = 0,
                                 });
        upgrades_fire_on_reaction(g, REACT_BLAZING, cx, cy, (float)blazing_cells);
        /* Reaction hooks may inject fire or storm, so the peaks no longer bound the field. */
        peaks->fire = FLT_MAX;
        peaks->storm = FLT_MAX;
        if (storm_dirty != NULL) {
            *storm_dirty = true;
        }
    }
    return storm_cells;
}

static int32_t elements_stage2_cataclysm(GameState *g, ElementPeaks peaks)
{
    if (g->internal.cataclysm_cd > 0.0f) {
        return -1;
    }
    const float thresh =
        g->internal.mods.cataclysm_thresh > 0.0f ? g->internal.mods.cataclysm_thresh : 0.30f;
    if (peaks.fire <= thresh || peaks.storm <= thresh) {
        return -1;
    }
    const float *ink_scan = fluid_channel_const(&g->fluid, 0);
    const float *fire_scan = fluid_channel_const(&g->fluid, 1);
    const float *storm_scan = fluid_channel_const(&g->fluid, 2);
    if (ink_scan == NULL || fire_scan == NULL || storm_scan == NULL ||
        g->fluid.cfg.active_channels < 3) {
        return -1;
    }

    const uint8_t *solid = g->fluid.solid;
    int32_t counts[GAME_THUNDERBURST_BLOCKS] = {0};
    float sum_val[GAME_THUNDERBURST_BLOCKS] = {0};
    float sum_x[GAME_THUNDERBURST_BLOCKS] = {0};
    float sum_y[GAME_THUNDERBURST_BLOCKS] = {0};

    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        const float cy = ((float)y + 0.5f) * FLUID_CELL_PX;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const int idx = row + x;
            if (solid[idx] != 0) {
                continue;
            }
            const float ink = ink_scan[idx];
            const float fire = fire_scan[idx];
            const float storm = storm_scan[idx];
            if (!isfinite(ink) || !isfinite(fire) || !isfinite(storm)) {
                continue;
            }
            if (ink > thresh && fire > thresh && storm > thresh) {
                const float cx = ((float)x + 0.5f) * FLUID_CELL_PX;
                const int b = reaction_block_index(cx, cy);
                ++counts[b];
                sum_val[b] += ink + fire + storm;
                sum_x[b] += cx;
                sum_y[b] += cy;
            }
        }
    }

    const float min_cells = 8.0f * GAME_CELL_COUNT_SCALE;
    int32_t best_b = -1;
    for (int32_t b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        if ((float)counts[b] < min_cells) {
            continue;
        }
        if (best_b < 0 || counts[b] > counts[best_b] ||
            (counts[b] == counts[best_b] && sum_val[b] > sum_val[best_b])) {
            best_b = b;
        }
    }
    if (best_b < 0) {
        return -1;
    }

    const int32_t count = counts[best_b];
    const float cx_px = sum_x[best_b] / (float)count;
    const float cy_px = sum_y[best_b] / (float)count;

    const float dye_rad_px = 96.0f;
    if (g->pending_impulse_count < GAME_MAX_PENDING_IMPULSE) {
        g->pending_impulses[g->pending_impulse_count++] = (PendingImpulse){
            .x = cx_px,
            .y = cy_px,
            .rad_cells = GAME_PX_TO_CELLS(dye_rad_px),
            .peak_speed = GAME_PX_TO_CELLS(720.0f),
        };
    }

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, id)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL) {
            continue;
        }
        const float dx = tr->x - cx_px;
        const float dy = tr->y - cy_px;
        const float d = hypotf(dx, dy);
        if (d > 140.0f) {
            continue;
        }
        const float nx = d < 1e-4f ? 1.0f : (dx / d);
        const float ny = d < 1e-4f ? 0.0f : (dy / d);
        VelocityComp *vel = game_velocity_mut(g, id);
        StatusComp *st = game_status_mut(g, id);
        const float kmul = mass_explosion_multiplier(g, vel, en);
        if (kmul > 0.0f && vel != NULL && st != NULL) {
            const float imp = 620.0f * kmul;
            vel->vx += nx * imp;
            vel->vy += ny * imp;
            if (st->knockback_t < 1.0f) {
                st->knockback_t = 1.0f;
            }
        }
        const uint32_t elite_flags = en->elite_flags;
        (void)combat_apply_damage(g, id, 80, DMG_FIRE | DMG_STORM | DMG_CATACLYSM | DMG_REACTION,
                                  nx * 620.0f, ny * 620.0f);
        if (ecs_is_alive(&g->ecs, id) && (elite_flags & ELITE_INK_REPEL) == 0) {
            StatusComp *st_after = game_status_mut(g, id);
            if (st_after != NULL) {
                st_after->ink_mark_t = g->internal.mods.ink_mark_duration;
            }
        }
    }

    typedef struct {
        EntityId id;
        float d2;
        float x;
        float y;
    } CataclysmBoltTarget;
    CataclysmBoltTarget top[4] = {0};
    int32_t top_n = 0;
    const float bolt_rad2 = 260.0f * 260.0f;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, id)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL) {
            continue;
        }
        const float dx = tr->x - cx_px;
        const float dy = tr->y - cy_px;
        const float d2 = dx * dx + dy * dy;
        if (d2 > bolt_rad2) {
            continue;
        }
        int32_t pos = top_n;
        while (pos > 0 && d2 < top[pos - 1].d2) {
            if (pos < 4) {
                top[pos] = top[pos - 1];
            }
            --pos;
        }
        if (pos < 4) {
            top[pos] = (CataclysmBoltTarget){.id = id, .d2 = d2, .x = tr->x, .y = tr->y};
            if (top_n < 4) {
                ++top_n;
            }
        }
    }

    for (int32_t k = 0; k < top_n; ++k) {
        if (!game_enemy_is_targetable(g, top[k].id)) {
            continue;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_STORM_ZAP,
                                     .x = cx_px,
                                     .y = cy_px,
                                     .x2 = top[k].x,
                                     .y2 = top[k].y,
                                     .value = 0,
                                     .flags = 0,
                                 });
        (void)combat_apply_damage(g, top[k].id, 30, DMG_STORM | DMG_CATACLYSM | DMG_REACTION, 0.0f,
                                  0.0f);
    }

    const float dye_rad_px2 = dye_rad_px * dye_rad_px;
    int gx0 = (int)floorf((cx_px - dye_rad_px) / FLUID_CELL_PX);
    int gx1 = (int)ceilf((cx_px + dye_rad_px) / FLUID_CELL_PX);
    int gy0 = (int)floorf((cy_px - dye_rad_px) / FLUID_CELL_PX);
    int gy1 = (int)ceilf((cy_px + dye_rad_px) / FLUID_CELL_PX);
    if (gx0 < 1) {
        gx0 = 1;
    }
    if (gx1 > FLUID_W - 2) {
        gx1 = FLUID_W - 2;
    }
    if (gy0 < 1) {
        gy0 = 1;
    }
    if (gy1 > FLUID_H - 2) {
        gy1 = FLUID_H - 2;
    }
    float *ink_ch = fluid_channel_mut(&g->fluid, 0);
    float *fire_ch = fluid_channel_mut(&g->fluid, 1);
    float *storm_ch = fluid_channel_mut(&g->fluid, 2);
    if (ink_ch != NULL && fire_ch != NULL && storm_ch != NULL) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
            const float dy = cell_y - cy_px;
            const int row = gy * FLUID_W;
            for (int gx = gx0; gx <= gx1; ++gx) {
                const int idx = row + gx;
                if (solid[idx] != 0) {
                    continue;
                }
                const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
                const float dx = cell_x - cx_px;
                if (dx * dx + dy * dy <= dye_rad_px2) {
                    if (isfinite(ink_ch[idx])) {
                        ink_ch[idx] *= 0.5f;
                    }
                    if (isfinite(fire_ch[idx])) {
                        fire_ch[idx] *= 0.2f;
                    }
                    if (isfinite(storm_ch[idx])) {
                        storm_ch[idx] *= 0.2f;
                    }
                }
            }
        }
    }
    end_channel_borrows(&g->fluid, ink_ch != NULL, fire_ch != NULL, storm_ch != NULL);

    g->internal.cataclysm_cd =
        g->internal.mods.cataclysm_cd_base > 0.0f ? g->internal.mods.cataclysm_cd_base : 2.0f;
    if (g->hitstop_t < 0.06f) {
        g->hitstop_t = 0.06f;
    }
    if (g->run.cataclysms < META_MAX_COUNTERS) {
        ++g->run.cataclysms;
    }
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_CATACLYSM,
                                 .x = cx_px,
                                 .y = cy_px,
                                 .x2 = dye_rad_px,
                                 .y2 = 0.0f,
                                 .value = count,
                                 .flags = 0,
                             });
    upgrades_fire_on_reaction(g, REACT_CATACLYSM, cx_px, cy_px, (float)count);
    return best_b;
}

static bool elements_stage3_thunderburst(GameState *g, int32_t cataclysm_block, ElementPeaks peaks)
{
    const float thresh = g->internal.mods.thunderburst_thresh;
    if (peaks.fire <= thresh || peaks.storm <= thresh) {
        return false;
    }
    const float *fire_scan = fluid_channel_const(&g->fluid, 1);
    const float *storm_scan = fluid_channel_const(&g->fluid, 2);
    if (fire_scan == NULL || storm_scan == NULL || g->fluid.cfg.active_channels < 3) {
        return false;
    }

    const uint8_t *solid = g->fluid.solid;
    const float rmul = g->internal.mods.thunderburst_radius_mul > 0.0f
                           ? g->internal.mods.thunderburst_radius_mul
                           : 1.0f;

    int32_t counts[GAME_THUNDERBURST_BLOCKS] = {0};
    float sum_x[GAME_THUNDERBURST_BLOCKS] = {0};
    float sum_y[GAME_THUNDERBURST_BLOCKS] = {0};

    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        const float cy = ((float)y + 0.5f) * FLUID_CELL_PX;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const int idx = row + x;
            if (solid[idx] != 0) {
                continue;
            }
            const float fire = fire_scan[idx];
            const float storm = storm_scan[idx];
            if (!isfinite(fire) || !isfinite(storm)) {
                continue;
            }
            if (fire > thresh && storm > thresh) {
                const float cx = ((float)x + 0.5f) * FLUID_CELL_PX;
                const int b = reaction_block_index(cx, cy);
                ++counts[b];
                sum_x[b] += cx;
                sum_y[b] += cy;
            }
        }
    }

    typedef struct {
        int32_t b;
        int32_t count;
    } BurstBlock;
    BurstBlock top[3] = {0};
    int32_t top_n = 0;

    const float min_cells = 6.0f * GAME_CELL_COUNT_SCALE;
    for (int32_t b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        if (b == cataclysm_block || g->internal.thunderburst_block_cd[b] > 0.0f ||
            (float)counts[b] < min_cells) {
            continue;
        }
        int32_t pos = top_n;
        while (pos > 0 && counts[b] > top[pos - 1].count) {
            if (pos < 3) {
                top[pos] = top[pos - 1];
            }
            --pos;
        }
        if (pos < 3) {
            top[pos] = (BurstBlock){.b = b, .count = counts[b]};
            if (top_n < 3) {
                ++top_n;
            }
        }
    }

    const float dmg_rad_px = 96.0f * rmul;
    const float dye_rad_px = 72.0f * rmul;
    const float dye_rad_cells = GAME_PX_TO_CELLS(dye_rad_px);
    const float dye_rad_px2 = dye_rad_px * dye_rad_px;
    const uint32_t hw = g->ecs.high_water;

    for (int32_t t = 0; t < top_n; ++t) {
        const int32_t b = top[t].b;
        const int32_t count = top[t].count;
        const float cx_px = sum_x[b] / (float)count;
        const float cy_px = sum_y[b] / (float)count;

        if (g->pending_impulse_count < GAME_MAX_PENDING_IMPULSE) {
            g->pending_impulses[g->pending_impulse_count++] = (PendingImpulse){
                .x = cx_px,
                .y = cy_px,
                .rad_cells = dye_rad_cells,
                .peak_speed = GAME_PX_TO_CELLS(480.0f),
            };
        }

        for (uint32_t i = 0; i < hw; ++i) {
            const EntityId id = ecs_entity_at(&g->ecs, i);
            if (id.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, id)) {
                continue;
            }
            const TransformComp *tr = game_transform(g, id);
            const EnemyComp *en = game_enemy(g, id);
            if (tr == NULL || en == NULL) {
                continue;
            }
            const float dx = tr->x - cx_px;
            const float dy = tr->y - cy_px;
            const float d = hypotf(dx, dy);
            if (d > dmg_rad_px) {
                continue;
            }
            const float nx = d < 1e-4f ? 1.0f : (dx / d);
            const float ny = d < 1e-4f ? 0.0f : (dy / d);
            VelocityComp *vel = game_velocity_mut(g, id);
            StatusComp *st = game_status_mut(g, id);
            const float kmul = mass_explosion_multiplier(g, vel, en);
            if (kmul > 0.0f && vel != NULL && st != NULL) {
                const float imp = 520.0f * kmul;
                vel->vx += nx * imp;
                vel->vy += ny * imp;
                if (st->knockback_t < 1.0f) {
                    st->knockback_t = 1.0f;
                }
            }
            (void)combat_apply_damage(g, id, 30, DMG_FIRE | DMG_STORM | DMG_REACTION, nx * 520.0f,
                                      ny * 520.0f);
        }

        int gx0 = (int)floorf((cx_px - dye_rad_px) / FLUID_CELL_PX);
        int gx1 = (int)ceilf((cx_px + dye_rad_px) / FLUID_CELL_PX);
        int gy0 = (int)floorf((cy_px - dye_rad_px) / FLUID_CELL_PX);
        int gy1 = (int)ceilf((cy_px + dye_rad_px) / FLUID_CELL_PX);
        if (gx0 < 1) {
            gx0 = 1;
        }
        if (gx1 > FLUID_W - 2) {
            gx1 = FLUID_W - 2;
        }
        if (gy0 < 1) {
            gy0 = 1;
        }
        if (gy1 > FLUID_H - 2) {
            gy1 = FLUID_H - 2;
        }
        float *fire_ch = fluid_channel_mut(&g->fluid, 1);
        float *storm_ch = fluid_channel_mut(&g->fluid, 2);
        if (fire_ch != NULL && storm_ch != NULL) {
            for (int gy = gy0; gy <= gy1; ++gy) {
                const float cell_y = ((float)gy + 0.5f) * FLUID_CELL_PX;
                const float dy = cell_y - cy_px;
                const int row = gy * FLUID_W;
                for (int gx = gx0; gx <= gx1; ++gx) {
                    const int idx = row + gx;
                    if (solid[idx] != 0) {
                        continue;
                    }
                    const float cell_x = ((float)gx + 0.5f) * FLUID_CELL_PX;
                    const float dx = cell_x - cx_px;
                    if (dx * dx + dy * dy <= dye_rad_px2) {
                        if (isfinite(fire_ch[idx])) {
                            fire_ch[idx] *= 0.3f;
                        }
                        if (isfinite(storm_ch[idx])) {
                            storm_ch[idx] *= 0.2f;
                        }
                    }
                }
            }
        }
        end_channel_borrows(&g->fluid, false, fire_ch != NULL, storm_ch != NULL);

        g->internal.thunderburst_block_cd[b] = 0.8f;
        /* Sweeping a fire + storm brush chains bursts across neighbouring blocks every few ticks;
         * a freeze per burst would stutter the whole sweep, so bursts share one freeze per 0.55s
         * while each keeps its full damage, impulse and event. */
        if (g->internal.thunderburst_hitstop_cd <= 0.0f) {
            if (g->hitstop_t < 0.03f) {
                g->hitstop_t = 0.03f;
            }
            g->internal.thunderburst_hitstop_cd = 0.55f;
        }
        if (g->run.thunderbursts < META_MAX_COUNTERS) {
            ++g->run.thunderbursts;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_THUNDERBURST,
                                     .x = cx_px,
                                     .y = cy_px,
                                     .x2 = dye_rad_px,
                                     .y2 = 0.0f,
                                     .value = count,
                                     .flags = 0,
                                 });
        upgrades_fire_on_reaction(g, REACT_THUNDERBURST, cx_px, cy_px, (float)count);
    }
    return top_n > 0;
}

static bool has_min_storm_cells(const FluidState *f, int32_t min_cells)
{
    const float *storm_ch = fluid_channel_const(f, 2);
    if (storm_ch == NULL) {
        return false;
    }
    const uint8_t *solid = f->solid;
    int32_t count = 0;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const int idx = row + x;
            if (solid[idx] != 0) {
                continue;
            }
            const float v = storm_ch[idx];
            if (isfinite(v) && v > 0.24f) {
                ++count;
                if (count >= min_cells) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void elements_stage4_enemies(GameState *g, int32_t storm_cells, bool storm_dirty)
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

        decay_cd(&st->ink_mark_t);
        decay_cd(&st->charge_t);
        decay_cd(&st->knockback_t);
        decay_cd(&st->stun_t);
        decay_cd(&st->barrier_t);
        if (hp->flash_t > 0.0f) {
            hp->flash_t -= INK_FIXED_DT * 2.6f;
            if (hp->flash_t <= 1e-4f) {
                hp->flash_t = 0.0f;
            }
        }
        decay_cd(&hp->invuln_t);

        if (!game_enemy_is_targetable(g, id)) {
            continue;
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
            const float interval =
                (game_is_boss_enemy(en->def_id) || game_is_boss_part(en->def_id)) ? 0.55f : 0.36f;
            if (st->burn_acc + 1e-4f >= interval) {
                st->burn_acc = 0.0f;
                const uint64_t fluid_rev_before = g->fluid.mutation_rev;
                (void)combat_apply_damage_f(g, id, g->internal.mods.burn_base_dmg,
                                            DMG_FIRE | DMG_REACTION, fu * FLUID_CELL_PX * 0.2f,
                                            fv * FLUID_CELL_PX * 0.2f);
                if (g->fluid.mutation_rev != fluid_rev_before) {
                    storm_dirty = true;
                }
            }
        } else if (st->burn_acc > 0.0f) {
            st->burn_acc -= INK_FIXED_DT * 0.5f;
            if (st->burn_acc < 0.0f) {
                st->burn_acc = 0.0f;
            }
        }
    }

    if (g->internal.storm_zap_cd > 0.0f) {
        return;
    }
    const int32_t min_storm_cells = (int32_t)ceilf(10.0f * GAME_CELL_COUNT_SCALE);
    if (storm_dirty) {
        if (!has_min_storm_cells(&g->fluid, min_storm_cells)) {
            return;
        }
    } else if (storm_cells < min_storm_cells) {
        return;
    }

    const float *storm_ch = fluid_channel_const(&g->fluid, 2);
    if (storm_ch == NULL) {
        return;
    }
    const uint8_t *solid = g->fluid.solid;
    const float zap_reach_px = 195.0f;
    const int reach = fluid_cell_reach(zap_reach_px);

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, id)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const StatusComp *st = game_status(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || st == NULL || en == NULL) {
            continue;
        }
        const bool first_stormproof = (en->elite_flags & ELITE_STORMPROOF) != 0;

        const int cx = (int)(tr->x / FLUID_CELL_PX);
        const int cy = (int)(tr->y / FLUID_CELL_PX);
        const int x0 = cx - reach < 1 ? 1 : cx - reach;
        const int x1 = cx + reach > FLUID_W - 2 ? FLUID_W - 2 : cx + reach;
        const int y0 = cy - reach < 1 ? 1 : cy - reach;
        const int y1 = cy + reach > FLUID_H - 2 ? FLUID_H - 2 : cy + reach;

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
                if (d2 <= zap_reach_px * zap_reach_px) {
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
                                     .flags = EVF_STORM_FLUID_TEMPEST,
                                 });
        (void)combat_apply_damage_f(g, id, dmg, DMG_STORM | DMG_REACTION, 0.0f, 0.0f);

        int32_t max_hops = first_stormproof ? 0 : (2 + g->internal.mods.chain_hops_add);
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
            bool next_stormproof = false;

            for (uint32_t j = 0; j < hw; ++j) {
                if (index_in_list(visited, visited_n, j)) {
                    continue;
                }
                const EntityId cand = ecs_entity_at(&g->ecs, j);
                if (cand.index == ECS_INVALID_INDEX || !game_enemy_is_targetable(g, cand)) {
                    continue;
                }
                const TransformComp *ctr = game_transform(g, cand);
                const StatusComp *cst = game_status(g, cand);
                const EnemyComp *cen = game_enemy(g, cand);
                if (ctr == NULL || cst == NULL || cen == NULL) {
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
                    next_stormproof = (cen->elite_flags & ELITE_STORMPROOF) != 0;
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
                                         .flags = EVF_STORM_FLUID_TEMPEST,
                                     });
            (void)combat_apply_damage_f(g, next_id, dmg, DMG_STORM | DMG_REACTION, 0.0f, 0.0f);
            cur_x = next_x;
            cur_y = next_y;
            if (next_stormproof) {
                break;
            }
        }
        upgrades_fire_on_reaction(g, REACT_TEMPEST, cur_x, cur_y, (float)visited_n);
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

        const bool can_ignite_proj = !(proj->kind == PROJ_HOMING_OFUDA && !proj->deflected);
        if (can_ignite_proj && !proj->burning && fire > 0.36f && (proj->deflected || ink > 0.16f)) {
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

void elements_step_idle_cooldowns(GameState *g)
{
    if (g == NULL) {
        return;
    }
    decay_cd(&g->internal.ignite_callout_cd);
    decay_cd(&g->internal.storm_zap_cd);
    decay_cd(&g->internal.cataclysm_cd);
    decay_cd(&g->internal.thunderburst_hitstop_cd);
    for (int32_t b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        decay_cd(&g->internal.thunderburst_block_cd[b]);
    }
    g->internal.blazing_active = false;
}

static void elements_step_impl(GameState *g, bool use_solver_peaks)
{
    if (g == NULL) {
        return;
    }
    elements_step_idle_cooldowns(g);

    /* Stage 1: cell reactions (Blazing Inkstorm + Tempest Lightning growth). Its rules need fire or
     * storm above 0.14 and stages 2-3 compare the same peaks with their own thresholds, so quiet
     * frames skip the full-grid scans without changing any result. Immediate post-solve callers
     * reuse the solver's fresh max_dye peaks instead of re-scanning 2x14,400 floats. */
    ElementPeaks peaks = {0.0f, 0.0f};
    if (use_solver_peaks) {
        peaks.fire = g->fluid.cfg.active_channels > 1 ? g->fluid.stats.max_dye[1] : 0.0f;
        peaks.storm = g->fluid.cfg.active_channels > 2 ? g->fluid.stats.max_dye[2] : 0.0f;
    } else {
        const float *fire_ch = fluid_channel_const(&g->fluid, 1);
        const float *storm_ch = fluid_channel_const(&g->fluid, 2);
        peaks.fire = fire_ch != NULL ? channel_peak(fire_ch) : 0.0f;
        peaks.storm = storm_ch != NULL ? channel_peak(storm_ch) : 0.0f;
    }
    int32_t storm_cells = 0;
    bool storm_dirty = false;
    if (peaks.fire > 0.14f || peaks.storm > 0.14f) {
        storm_cells = elements_stage1_cells(g, &peaks, &storm_dirty);
    }

    /* Stage 2: Cataclysm (ink x fire x storm, 64px block, global 2.0s CD). */
    const int32_t cataclysm_block = elements_stage2_cataclysm(g, peaks);
    if (cataclysm_block >= 0) {
        /* Cataclysm damage and hooks may inject or consume fire/storm before later stages. */
        peaks = (ElementPeaks){.fire = FLT_MAX, .storm = FLT_MAX};
        storm_dirty = true;
    }

    /* Stage 3: Thunderburst (fire x storm, 64px block, max 3/tick, excludes cataclysm block). */
    if (elements_stage3_thunderburst(g, cataclysm_block, peaks)) {
        storm_dirty = true;
    }

    /* Stage 4: enemy statuses, burn ticks, and Tempest Lightning chains. */
    elements_stage4_enemies(g, storm_cells, storm_dirty);

    /* Stage 5: projectile elemental upgrades and pre-deflect storm acceleration. */
    elements_stage5_projectiles(g);
}

void elements_step(GameState *g) { elements_step_impl(g, false); }

void elements_step_post_solve(GameState *g) { elements_step_impl(g, true); }
