#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "roguelike/waves.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const float k_orb_arc_dist[GAME_RYU_ORB_COUNT] = {0.0f, 450.0f, 900.0f};

static int32_t ryu_popcount3(uint8_t mask)
{
    return (int32_t)((mask & 1u) + ((mask >> 1) & 1u) + ((mask >> 2) & 1u));
}

static int32_t ryu_next_alive_orb(const GameState *g, int32_t start_slot)
{
    const int32_t base = (start_slot >= 0 && start_slot < GAME_RYU_ORB_COUNT) ? start_slot : 0;
    for (int32_t off = 0; off < GAME_RYU_ORB_COUNT; ++off) {
        const int32_t slot = (base + off) % GAME_RYU_ORB_COUNT;
        if ((g->ryu_body.orb_broken_mask & (uint8_t)(1u << (unsigned)slot)) != 0u) {
            continue;
        }
        const EntityId oid = g->ryu_body.orb_ids[slot];
        if (oid.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, oid)) {
            return slot;
        }
    }
    return -1;
}

static void ryu_sample_arc_pos(const RyuBodyState *body, float head_x, float head_y,
                               float head_facing, float target_dist_px, float *ox, float *oy,
                               float *ofacing)
{
    if (target_dist_px <= 0.0f || body->trail_count <= 0) {
        *ox = head_x;
        *oy = head_y;
        *ofacing = head_facing;
        return;
    }

    float acc = 0.0f;
    GameVec2 prev = {head_x, head_y};
    float dir_x = cosf(head_facing);
    float dir_y = sinf(head_facing);

    const int32_t count =
        body->trail_count > GAME_RYU_TRAIL_CAP ? GAME_RYU_TRAIL_CAP : body->trail_count;
    for (int32_t k = 0; k < count; ++k) {
        const int32_t idx = (body->trail_head - k + GAME_RYU_TRAIL_CAP) % GAME_RYU_TRAIL_CAP;
        const GameVec2 cur = body->trail[idx];
        const float dx = cur.x - prev.x;
        const float dy = cur.y - prev.y;
        const float seg = hypotf(dx, dy);
        if (seg > 1e-6f) {
            dir_x = -dx / seg;
            dir_y = -dy / seg;
            if (acc + seg >= target_dist_px) {
                const float t = (target_dist_px - acc) / seg;
                *ox = prev.x + dx * t;
                *oy = prev.y + dy * t;
                *ofacing = atan2f(dir_y, dir_x);
                return;
            }
            acc += seg;
        }
        prev = cur;
    }

    *ox = prev.x;
    *oy = prev.y;
    *ofacing = atan2f(dir_y, dir_x);
}

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

static void enter_awakening_phase(GameState *g, const TransformComp *tr, HealthComp *hp,
                                  EnemyComp *en, BossComp *boss)
{
    boss->phase_idx = 1;
    hp->invuln_t = 1.5f;
    if (en->fire_cd < 0.90f) {
        en->fire_cd = 0.90f;
    }
    boss->gale_t = 0.0f;
    boss->telegraph = 0.0f;
    boss->telegraph_pattern = 0;
    boss->pattern = 0;
    g->ryu_body.active_orb_slot = -1;
    static const float k_phase_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.0f, 0.0f, 0.0f};
    fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 50.0f, k_phase_dye);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_BOSS_PHASE,
                                 .x = tr->x,
                                 .y = tr->y,
                                 .value = 2,
                                 .flags = 0,
                             });
}

static void enter_ascension_phase(GameState *g, const TransformComp *tr, HealthComp *hp,
                                  EnemyComp *en, BossComp *boss)
{
    boss->phase_idx = 2;
    hp->invuln_t = 1.5f;
    if (en->fire_cd < 0.90f) {
        en->fire_cd = 0.90f;
    }
    boss->gale_t = 0.0f;
    boss->telegraph = 0.0f;
    boss->telegraph_pattern = 0;
    static const float k_phase_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.0f, 0.0f, 0.0f};
    fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 50.0f, k_phase_dye);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_BOSS_PHASE,
                                 .x = tr->x,
                                 .y = tr->y,
                                 .value = 3,
                                 .flags = 0,
                             });
}

static void step_orb_breaths(GameState *g)
{
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        const EntityId oid = g->ryu_body.orb_ids[s];
        if (oid.index == ECS_INVALID_INDEX || !ecs_is_alive(&g->ecs, oid)) {
            continue;
        }
        const TransformComp *otr = game_transform(g, oid);
        BossComp *oboss = game_boss_mut(g, oid);
        if (otr == NULL || oboss == NULL || oboss->gale_t <= 0.0f) {
            continue;
        }
        oboss->gale_t -= INK_FIXED_DT;
        if (oboss->gale_t <= 1e-4f) {
            oboss->gale_t = 0.0f;
        }
        if (oboss->part_id == 0) {
            static const float k_fire_breath[FLUID_MAX_CHANNELS] = {0.0f, 0.45f, 0.0f, 0.0f};
            for (int k = 1; k <= 8; ++k) {
                const float dist = (float)k * 36.0f;
                fluid_splat(&g->fluid, otr->x + oboss->gale_dx * dist,
                            otr->y + oboss->gale_dy * dist, 4.5f, oboss->gale_dx * 38.0f,
                            oboss->gale_dy * 38.0f, k_fire_breath);
            }
        } else if (oboss->part_id == 2) {
            static const float k_wind_gust[FLUID_MAX_CHANNELS] = {0.08f, 0.0f, 0.0f, 0.0f};
            for (int k = 1; k <= 9; ++k) {
                const float dist = (float)k * 38.0f;
                fluid_splat(&g->fluid, otr->x + oboss->gale_dx * dist,
                            otr->y + oboss->gale_dy * dist, 5.2f, oboss->gale_dx * 46.0f,
                            oboss->gale_dy * 46.0f, k_wind_gust);
            }
        }
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
    boss->part_id = 0;
    boss->parent_id = (EntityId){ECS_INVALID_INDEX, 0};
    boss->telegraph_pattern = 0;
    boss->telegraph = 0.0f;

    /* Initialize 80-point trail extending leftward at 12px spacing from the spawn head position. */
    memset(&g->ryu_body, 0, sizeof g->ryu_body);
    g->ryu_body.head_id = id;
    g->ryu_body.orb_broken_mask = 0u;
    g->ryu_body.active_orb_slot = 0;
    const int32_t init_pts = 80;
    for (int32_t i = 0; i < init_pts; ++i) {
        const float back = (float)(init_pts - 1 - i) * 12.0f;
        g->ryu_body.trail[i] = (GameVec2){tr->x - back, tr->y};
    }
    g->ryu_body.trail_head = init_pts - 1;
    g->ryu_body.trail_count = init_pts;

    /* Spawn 3 Dragon Orb child entities (0=Fire 0px, 1=Storm 450px, 2=Wind 900px). */
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        float ox = tr->x;
        float oy = tr->y;
        float ofacing = 0.0f;
        ryu_sample_arc_pos(&g->ryu_body, tr->x, tr->y, 0.0f, k_orb_arc_dist[s], &ox, &oy, &ofacing);
        const EntityId orb_id = enemy_spawn(g, ENEMY_BOSS_RYU_ORB, ox, oy);
        g->ryu_body.orb_ids[s] = orb_id;
        if (orb_id.index == ECS_INVALID_INDEX) {
            g->ryu_body.orb_broken_mask |= (uint8_t)(1u << (unsigned)s);
            continue;
        }
        TransformComp *otr = game_transform_mut(g, orb_id);
        if (otr != NULL) {
            otr->facing = ofacing;
        }
        BossComp *oboss = game_boss_mut(g, orb_id);
        if (oboss != NULL) {
            oboss->parent_id = id;
            oboss->part_id = s;
            oboss->phase_idx = 0;
            oboss->telegraph = 0.0f;
            oboss->telegraph_pattern = s;
        }
    }

    g->run.boss_id = id;
    g->internal.last_boss_x = tr->x;
    g->internal.last_boss_y = tr->y;

    static const float k_swirl_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.35f, 0.0f, 0.0f};
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

    /* Safety phase sync in case orbs were cleared or HP was adjusted externally. */
    if (boss->phase_idx == 0 && (g->ryu_body.orb_broken_mask & 0x7u) == 0x7u) {
        enter_awakening_phase(g, tr, hp, en, boss);
    } else if (boss->phase_idx == 1 && hp->hp > 0 && hp->hp * 5 <= hp->max_hp * 2) {
        enter_ascension_phase(g, tr, hp, en, boss);
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

    /* Push head position into the 128-point trail ring buffer whenever it moves >= 8px. */
    if (g->ryu_body.trail_count <= 0) {
        g->ryu_body.trail_head = 0;
        g->ryu_body.trail[0] = (GameVec2){tr->x, tr->y};
        g->ryu_body.trail_count = 1;
    } else {
        const GameVec2 last = g->ryu_body.trail[g->ryu_body.trail_head];
        if (hypotf(tr->x - last.x, tr->y - last.y) >= 8.0f) {
            g->ryu_body.trail_head = (g->ryu_body.trail_head + 1) % GAME_RYU_TRAIL_CAP;
            g->ryu_body.trail[g->ryu_body.trail_head] = (GameVec2){tr->x, tr->y};
            if (g->ryu_body.trail_count < GAME_RYU_TRAIL_CAP) {
                ++g->ryu_body.trail_count;
            }
        }
    }

    /* Position living Dragon Orbs at 0/450/900px arc lengths and inject body breeze along body. */
    static const float k_breeze_dye[FLUID_MAX_CHANNELS] = {0.06f, 0.0f, 0.0f, 0.0f};
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        float px = tr->x;
        float py = tr->y;
        float pf = tr->facing;
        ryu_sample_arc_pos(&g->ryu_body, tr->x, tr->y, tr->facing, k_orb_arc_dist[s], &px, &py,
                           &pf);
        if (g->internal.ryu_coil_t <= 0.0f) {
            const float bdx = px - sx;
            const float bdy = py - sy;
            const float bd = hypotf(bdx, bdy);
            if (bd < 200.0f && bd > 1e-6f) {
                px = sx + (bdx / bd) * 200.0f;
                py = sy + (bdy / bd) * 200.0f;
            }
        }
        const EntityId oid = g->ryu_body.orb_ids[s];
        if (oid.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, oid)) {
            TransformComp *otr = game_transform_mut(g, oid);
            if (otr != NULL) {
                otr->x = px;
                otr->y = py;
                otr->facing = pf;
            }
        }
        if (px >= 0.0f && px < (float)INK_SCREEN_W && py >= 0.0f && py < (float)INK_SCREEN_H) {
            fluid_splat(&g->fluid, px, py, 4.2f, cosf(pf) * 16.0f, sinf(pf) * 16.0f, k_breeze_dye);
        }
    }

    /* Step active Fire Orb breath / Wind Orb headwind from their respective orb positions. */
    step_orb_breaths(g);

    if (hp->invuln_t > 0.0f) {
        boss->telegraph = 0.0f;
        for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
            const EntityId oid = g->ryu_body.orb_ids[s];
            BossComp *oboss = (oid.index != ECS_INVALID_INDEX) ? game_boss_mut(g, oid) : NULL;
            if (oboss != NULL) {
                oboss->telegraph = 0.0f;
            }
        }
        return;
    }

    const float endless_cd_mul = enemy_endless_boss_cd_mul(g);

    if (boss->phase_idx == 0) {
        /* Phase 1: Three Dragon Orbs take turns attacking from their own coordinates. */
        boss->telegraph = 0.0f;
        int32_t slot = g->ryu_body.active_orb_slot;
        if (slot < 0 || slot >= GAME_RYU_ORB_COUNT ||
            (g->ryu_body.orb_broken_mask & (uint8_t)(1u << (unsigned)slot)) != 0u ||
            !ecs_is_alive(&g->ecs, g->ryu_body.orb_ids[slot])) {
            slot = ryu_next_alive_orb(g, slot >= 0 ? slot : 0);
            g->ryu_body.active_orb_slot = slot;
        }
        if (slot < 0) {
            return;
        }

        for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
            if (s == slot) {
                continue;
            }
            const EntityId oid = g->ryu_body.orb_ids[s];
            BossComp *oboss = (oid.index != ECS_INVALID_INDEX) ? game_boss_mut(g, oid) : NULL;
            if (oboss != NULL) {
                oboss->telegraph = 0.0f;
            }
        }

        const EntityId active_orb = g->ryu_body.orb_ids[slot];
        const TransformComp *otr = game_transform(g, active_orb);
        BossComp *oboss = game_boss_mut(g, active_orb);
        if (otr == NULL || oboss == NULL) {
            return;
        }

        const float tele_dur = 0.65f;
        en->fire_cd -= INK_FIXED_DT * slow;
        if (en->fire_cd <= 0.0f) {
            oboss->telegraph = 0.0f;
            const float dx = sx - otr->x;
            const float dy = sy - otr->y;
            const float d = hypotf(dx, dy);
            const float dir_x = d > 1e-6f ? dx / d : 0.0f;
            const float dir_y = d > 1e-6f ? dy / d : 1.0f;
            const float base_ang = atan2f(dir_y, dir_x);
            oboss->gale_dx = dir_x;
            oboss->gale_dy = dir_y;

            if (slot == 0) {
                /* Fire Orb: 5-way Fireball fan + 0.7s Fire Breath. */
                for (int k = -2; k <= 2; ++k) {
                    const float ang = base_ang + (float)k * 0.18f;
                    (void)projectiles_spawn_dir(g, PROJ_FIREBALL, otr->x, otr->y, cosf(ang),
                                                sinf(ang), 205.0f);
                }
                oboss->gale_t = 0.7f;
            } else if (slot == 1) {
                /* Storm Orb: 3-way Storm Orbs + 3 Lightning Strike telegraphs (1 inside shrine). */
                for (int k = -1; k <= 1; ++k) {
                    const float ang = base_ang + (float)k * 0.22f;
                    (void)projectiles_spawn_dir(g, PROJ_STORM_ORB, otr->x, otr->y, cosf(ang),
                                                sinf(ang), 215.0f);
                }
                const float a0 = ink_rng_f01(&g->rng) * 6.2831853f;
                const float r0 = 8.0f + 16.0f * ink_rng_f01(&g->rng);
                spawn_lightning_strike_gimmick(g, sx + cosf(a0) * r0, sy + sinf(a0) * r0);
                for (int k = 1; k <= 2; ++k) {
                    const float sa = a0 + (float)k * 2.0943951f;
                    const float sr = 72.0f + 56.0f * ink_rng_f01(&g->rng);
                    const float lx = sx + cosf(sa) * sr;
                    const float ly = sy + sinf(sa) * sr;
                    if (!fluid_is_solid(&g->fluid, lx, ly)) {
                        spawn_lightning_strike_gimmick(g, lx, ly);
                    }
                }
            } else {
                /* Wind Orb: 1.5s headwind from tail + 3 Wind Sickles. */
                oboss->gale_t = 1.5f;
                for (int k = -1; k <= 1; ++k) {
                    const float ang = base_ang + (float)k * 0.22f;
                    (void)projectiles_spawn_dir(g, PROJ_WIND_SICKLE, otr->x, otr->y, cosf(ang),
                                                sinf(ang), 300.0f);
                }
            }

            g->ryu_body.active_orb_slot = ryu_next_alive_orb(g, (slot + 1) % GAME_RYU_ORB_COUNT);
            const int32_t broken = ryu_popcount3(g->ryu_body.orb_broken_mask);
            const float raw_cd = 2.2f * powf(0.85f, (float)broken) * endless_cd_mul;
            en->fire_cd = fmaxf(raw_cd, tele_dur + 0.10f);
        } else if (en->fire_cd <= tele_dur) {
            oboss->telegraph_pattern = slot;
            oboss->telegraph = ink_clampf((tele_dur - en->fire_cd) / tele_dur, 0.0f, 1.0f);
        } else {
            oboss->telegraph = 0.0f;
        }
        return;
    }

    /* Phase 2 (Awakening) & Phase 3 (Ascension): Summons every 10s, Thunder-Fire Breath & Coil. */
    if (en->absorbed_ink <= 0.0f) {
        en->absorbed_ink = 10.0f;
        const int32_t max_escorts = 4 + g->internal.mods.max_alive_add;
        if (waves_count_alive_escorts(g) < max_escorts) {
            const bool spawn_oni = (ink_rng_next(&g->rng) & UINT64_C(1)) == 0u;
            if (spawn_oni) {
                const float ox = ink_clampf(tr->x, 120.0f, (float)INK_SCREEN_W - 120.0f);
                const float oy = 110.0f;
                (void)waves_spawn_summoned_escort(
                    g, ENEMY_STONE_ONI, fluid_is_solid(&g->fluid, ox, oy) ? 200.0f : ox, oy);
            } else {
                for (int s = 0; s < 2 && waves_count_alive_escorts(g) < max_escorts; ++s) {
                    const float lx = s == 0 ? 180.0f : (float)INK_SCREEN_W - 180.0f;
                    const float ly = 140.0f;
                    (void)waves_spawn_summoned_escort(g, ENEMY_INK_LEECH, lx, ly);
                }
            }
        }
    }

    const bool do_coil = (boss->pattern == 1 && boss->pattern_timer <= 0.0f);
    const int32_t next_tele_pat = do_coil ? 3 : 1;
    const float tele_dur = do_coil ? 0.80f : 0.65f;

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

        if (do_coil) {
            /* Dragon Coil: 2.0s reverse vortex around the shrine, 15s cooldown. */
            g->internal.ryu_coil_t = 2.0f;
            boss->pattern_timer = 15.0f;
            boss->pattern = 0;
        } else {
            /* Thunder-Fire Breath: 3 Fireballs + 3 Storm Orbs interleaved in fan. */
            for (int k = 0; k < 6; ++k) {
                const float ang = base_ang + ((float)k - 2.5f) * 0.17f;
                const ProjKind pk = (k % 2 == 0) ? PROJ_FIREBALL : PROJ_STORM_ORB;
                const float sp = (pk == PROJ_FIREBALL) ? 205.0f : 215.0f;
                (void)projectiles_spawn_dir(g, pk, tr->x, tr->y, cosf(ang), sinf(ang), sp);
            }
            boss->pattern = (boss->pattern_timer <= 0.0f) ? 1 : 0;
        }

        const float next_tele = (boss->pattern == 1 && boss->pattern_timer <= 0.0f) ? 0.80f : 0.65f;
        const float phase_mul = boss->phase_idx >= 2 ? 0.8f : 1.0f;
        en->fire_cd = fmaxf(2.2f * phase_mul * endless_cd_mul, next_tele + 0.10f);
    } else if (en->fire_cd <= tele_dur) {
        boss->telegraph_pattern = next_tele_pat;
        boss->telegraph = ink_clampf((tele_dur - en->fire_cd) / tele_dur, 0.0f, 1.0f);
    } else {
        boss->telegraph = 0.0f;
    }
}
