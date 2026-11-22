#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "roguelike/waves.h"

#include <math.h>
#include <stddef.h>

static void spawn_lightning_strike_gimmick(GameState *g, float x, float y)
{
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_GIMMICK);
    if (id.index == ECS_INVALID_INDEX) {
        return;
    }
    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = x;
        tr->y = y;
        tr->radius = 40.0f;
    }
    GimmickComp *gim = game_gimmick_mut(g, id);
    if (gim != NULL) {
        gim->kind = (int32_t)GIMMICK_LIGHTNING_STRIKE;
        gim->elem_kind = (int32_t)GIMMICK_ELEM_STORM;
        gim->timer = 0.8f;
    }
}

void boss_ryu_spawn(GameState *g, EntityId id)
{
    const TransformComp *tr = game_transform(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || en == NULL || boss == NULL) {
        return;
    }

    en->phase = 0;
    en->fire_cd = 1.4f;
    en->absorbed_ink = 6.0f; /* Summon cooldown (10s after awakening) */
    en->windup = 0.0f;

    boss->pattern = 0;
    boss->pattern_timer = 0.0f; /* Dragon Coil 15s cooldown */
    boss->phase_idx = 0;
    boss->gale_dx = 0.0f;
    boss->gale_dy = 1.0f;
    boss->gale_t = 0.0f;
    boss->part_id = 0; /* 0 = Fire Orb, 1 = Storm Orb, 2 = Wind Orb (Awakened) */
    boss->parent_id = (EntityId){ECS_INVALID_INDEX, 0};
    boss->telegraph_pattern = 0;
    boss->telegraph = 0.0f;

    g->run.boss_id = id;
    g->internal.last_boss_x = tr->x;
    g->internal.last_boss_y = tr->y;

    static const float k_swirl_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.35f, 0.35f, 0.0f};
    fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 52.0f, k_swirl_dye);
}

void boss_ryu_step(GameState *g, EntityId id)
{
    TransformComp *tr = game_transform_mut(g, id);
    VelocityComp *vel = game_velocity_mut(g, id);
    HealthComp *hp = game_health_mut(g, id);
    const StatusComp *st = game_status(g, id);
    EnemyComp *en = game_enemy_mut(g, id);
    BossComp *boss = game_boss_mut(g, id);
    if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || boss == NULL) {
        return;
    }

    /* Keep orb phase synchronized with current HP thresholds (66% / 33%). */
    if (hp->hp * 3 <= hp->max_hp) {
        boss->part_id = 2;
        if (boss->phase_idx == 0) {
            boss->phase_idx = 1;
            hp->invuln_t = 1.5f;
            boss->telegraph = 0.0f;
            boss->telegraph_pattern = 0;
            boss->gale_t = 0.0f;
            static const float k_phase_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.5f, 0.5f, 0.0f};
            fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 50.0f, k_phase_dye);
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_BOSS_PHASE,
                                         .x = tr->x,
                                         .y = tr->y,
                                         .value = 2,
                                         .flags = 0,
                                     });
        }
    } else if (hp->hp * 3 <= hp->max_hp * 2 && boss->part_id < 1) {
        boss->part_id = 1;
    }

    if (boss->pattern_timer > 0.0f) {
        boss->pattern_timer -= INK_FIXED_DT;
        if (boss->pattern_timer <= 1e-4f) {
            boss->pattern_timer = 0.0f;
        }
    }
    if (en->absorbed_ink > 0.0f) {
        en->absorbed_ink -= INK_FIXED_DT;
        if (en->absorbed_ink <= 1e-4f) {
            en->absorbed_ink = 0.0f;
        }
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float slow = st->ink_mark_t > 0.0f ? 0.66f : 1.0f;
    const float speed_mul = boss->phase_idx >= 1 ? 1.3f : 1.0f;

    const float ph = (float)en->phase * 0.001f;
    const int32_t ph_step = (int32_t)floorf(22.0f * speed_mul * slow + 0.5f);
    en->phase = (en->phase + (ph_step > 0 ? ph_step : 1)) % 6283;

    float tx;
    float ty;
    if (g->internal.ryu_coil_t > 0.0f) {
        tx = sx + cosf(ph * 3.0f) * 240.0f;
        ty = sy + sinf(ph * 3.0f) * 240.0f;
    } else {
        tx = sx + sinf(ph * 0.70f) * 430.0f;
        ty = 210.0f + sinf(ph * 1.40f) * 110.0f;
        const float rdx = tx - sx;
        const float rdy = ty - sy;
        const float rd = hypotf(rdx, rdy);
        if (rd < 215.0f) {
            if (rd > 1e-6f) {
                tx = sx + (rdx / rd) * 215.0f;
                ty = sy + (rdy / rd) * 215.0f;
            } else {
                ty = sy - 215.0f;
            }
        }
    }
    tx = ink_clampf(tx, 110.0f, (float)INK_SCREEN_W - 110.0f);
    ty = ink_clampf(ty, 96.0f, 510.0f);

    const float prev_x = tr->x;
    const float prev_y = tr->y;
    vel->vx = (vel->vx + (tx - tr->x) * 2.8f * speed_mul * INK_FIXED_DT * slow) * 0.90f;
    vel->vy = (vel->vy + (ty - tr->y) * 2.8f * speed_mul * INK_FIXED_DT * slow) * 0.90f;
    tr->x = ink_clampf(tr->x + vel->vx * INK_FIXED_DT, 110.0f, (float)INK_SCREEN_W - 110.0f);
    tr->y = ink_clampf(tr->y + vel->vy * INK_FIXED_DT, 96.0f, 510.0f);
    if (g->internal.ryu_coil_t <= 0.0f) {
        const float sdx = tr->x - sx;
        const float sdy = tr->y - sy;
        const float sd = hypotf(sdx, sdy);
        if (sd < 200.0f && sd > 1e-6f) {
            tr->x = sx + (sdx / sd) * 200.0f;
            tr->y = sy + (sdy / sd) * 200.0f;
        }
    }

    const float mvx = tr->x - prev_x;
    const float mvy = tr->y - prev_y;
    const float mlen = hypotf(mvx, mvy);
    const float mdir_x = mlen > 1e-6f ? mvx / mlen : 1.0f;
    const float mdir_y = mlen > 1e-6f ? mvy / mlen : 0.0f;
    tr->facing = atan2f(mdir_y, mdir_x);

    /* Body breeze injection along swimming direction. */
    static const float k_breeze_dye[FLUID_MAX_CHANNELS] = {0.06f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, tr->x, tr->y, 4.2f, mdir_x * 16.0f, mdir_y * 16.0f, k_breeze_dye);

    /* Active Fire Breath (part_id == 0) or Wind Orb Headwind (part_id == 2). */
    if (boss->gale_t > 0.0f) {
        boss->gale_t -= INK_FIXED_DT;
        if (boss->gale_t <= 1e-4f) {
            boss->gale_t = 0.0f;
        }
        if (boss->part_id == 0) {
            static const float k_fire_breath[FLUID_MAX_CHANNELS] = {0.0f, 0.45f, 0.0f, 0.0f};
            for (int k = 1; k <= 8; ++k) {
                const float dist = (float)k * 36.0f;
                fluid_splat(&g->fluid, tr->x + boss->gale_dx * dist, tr->y + boss->gale_dy * dist,
                            4.5f, boss->gale_dx * 38.0f, boss->gale_dy * 38.0f, k_fire_breath);
            }
        } else {
            static const float k_wind_gust[FLUID_MAX_CHANNELS] = {0.08f, 0.0f, 0.0f, 0.0f};
            for (int k = 1; k <= 9; ++k) {
                const float dist = (float)k * 38.0f;
                fluid_splat(&g->fluid, tr->x + boss->gale_dx * dist, tr->y + boss->gale_dy * dist,
                            5.2f, boss->gale_dx * 46.0f, boss->gale_dy * 46.0f, k_wind_gust);
            }
        }
    }

    if (hp->invuln_t > 0.0f) {
        boss->telegraph = 0.0f;
        return;
    }

    /* Post-awakening summons (every 10s, shared 4-escort cap). */
    if (boss->phase_idx >= 1 && en->absorbed_ink <= 0.0f) {
        en->absorbed_ink = 10.0f;
        if (waves_count_alive_escorts(g) < 4) {
            const bool spawn_oni = (ink_rng_next(&g->rng) & UINT64_C(1)) == 0u;
            if (spawn_oni) {
                const float ox = ink_clampf(tr->x, 120.0f, (float)INK_SCREEN_W - 120.0f);
                const float oy = 110.0f;
                (void)waves_spawn_summoned_escort(
                    g, ENEMY_STONE_ONI, fluid_is_solid(&g->fluid, ox, oy) ? 200.0f : ox, oy);
            } else {
                for (int s = 0; s < 2 && waves_count_alive_escorts(g) < 4; ++s) {
                    const float lx = s == 0 ? 180.0f : (float)INK_SCREEN_W - 180.0f;
                    const float ly = 140.0f;
                    (void)waves_spawn_summoned_escort(g, ENEMY_INK_LEECH, lx, ly);
                }
            }
        }
    }

    int32_t next_tele_pat = boss->part_id;
    float tele_dur = 0.65f;
    if (boss->phase_idx >= 1) {
        if (boss->pattern == 2 && boss->pattern_timer <= 0.0f) {
            next_tele_pat = 3;
            tele_dur = 0.80f;
        } else if (boss->pattern == 1) {
            next_tele_pat = 1;
            tele_dur = 0.65f;
        } else {
            next_tele_pat = 0;
            tele_dur = 0.65f;
        }
    }

    en->fire_cd -= INK_FIXED_DT * slow;
    if (en->fire_cd <= 0.0f) {
        boss->telegraph = 0.0f;
        const float dx = sx - tr->x;
        const float dy = sy - tr->y;
        const float d = hypotf(dx, dy);
        const float dir_x = d > 1e-6f ? dx / d : 0.0f;
        const float dir_y = d > 1e-6f ? dy / d : 1.0f;
        const float base_ang = atan2f(dir_y, dir_x);
        boss->gale_dx = dir_x;
        boss->gale_dy = dir_y;

        if (boss->phase_idx == 0 && boss->part_id == 0) {
            /* Fire Orb: 5-way Fireball fan + 0.7s Fire Breath. */
            for (int k = -2; k <= 2; ++k) {
                const float ang = base_ang + (float)k * 0.18f;
                (void)projectiles_spawn_dir(g, PROJ_FIREBALL, tr->x, tr->y, cosf(ang), sinf(ang),
                                            205.0f);
            }
            boss->gale_t = 0.7f;
        } else if (boss->phase_idx == 0 && boss->part_id == 1) {
            /* Storm Orb: 3-way Storm Orbs + 3 Lightning Strike telegraphs (1 inside shrine). */
            for (int k = -1; k <= 1; ++k) {
                const float ang = base_ang + (float)k * 0.22f;
                (void)projectiles_spawn_dir(g, PROJ_STORM_ORB, tr->x, tr->y, cosf(ang), sinf(ang),
                                            215.0f);
            }
            const float a0 = ink_rng_f01(&g->rng) * 6.2831853f;
            const float r0 = 8.0f + 16.0f * ink_rng_f01(&g->rng);
            spawn_lightning_strike_gimmick(g, sx + cosf(a0) * r0, sy + sinf(a0) * r0);
            for (int s = 1; s <= 2; ++s) {
                const float sa = a0 + (float)s * 2.0943951f;
                const float sr = 72.0f + 56.0f * ink_rng_f01(&g->rng);
                const float lx = sx + cosf(sa) * sr;
                const float ly = sy + sinf(sa) * sr;
                if (!fluid_is_solid(&g->fluid, lx, ly)) {
                    spawn_lightning_strike_gimmick(g, lx, ly);
                }
            }
        } else {
            /* Awakened (phase_idx >= 1, part_id == 2): Wind / Thunder-Fire / Dragon Coil. */
            if (next_tele_pat == 3) {
                g->internal.ryu_coil_t = 2.0f;
                boss->pattern_timer = 15.0f;
                boss->pattern = 0;
            } else if (next_tele_pat == 1) {
                /* Thunder-Fire Breath: 3 Fireballs + 3 Storm Orbs interleaved in fan. */
                for (int k = 0; k < 6; ++k) {
                    const float ang = base_ang + ((float)k - 2.5f) * 0.17f;
                    const ProjKind pk = (k % 2 == 0) ? PROJ_FIREBALL : PROJ_STORM_ORB;
                    const float sp = (pk == PROJ_FIREBALL) ? 205.0f : 215.0f;
                    (void)projectiles_spawn_dir(g, pk, tr->x, tr->y, cosf(ang), sinf(ang), sp);
                }
                boss->pattern = (boss->pattern_timer <= 0.0f) ? 2 : 0;
            } else {
                /* Wind Orb: 1.5s headwind + 3 Wind Sickles. */
                boss->gale_t = 1.5f;
                for (int k = -1; k <= 1; ++k) {
                    const float ang = base_ang + (float)k * 0.22f;
                    (void)projectiles_spawn_dir(g, PROJ_WIND_SICKLE, tr->x, tr->y, cosf(ang),
                                                sinf(ang), 300.0f);
                }
                boss->pattern = 1;
            }
        }

        en->fire_cd = 2.2f * (boss->phase_idx >= 1 ? 0.8f : 1.0f);
    } else if (en->fire_cd <= tele_dur) {
        boss->telegraph_pattern = next_tele_pat;
        boss->telegraph = ink_clampf((tele_dur - en->fire_cd) / tele_dur, 0.0f, 1.0f);
    } else {
        boss->telegraph = 0.0f;
    }
}
