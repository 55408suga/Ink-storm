#include "combat/damage.h"

#include "core.h"
#include "enemies/enemy_defs.h"
#include "roguelike/upgrades.h"

#include <math.h>
#include <stddef.h>

static const float k_two_pi = 6.2831853f;

static void add_score(GameState *g, int64_t delta)
{
    int64_t next = (int64_t)g->run.score + delta;
    if (next < 0) {
        next = 0;
    } else if (next > META_MAX_SCORE) {
        next = META_MAX_SCORE;
    }
    g->run.score = (int32_t)next;
}

static void normalize_vec2(float vx, float vy, float *ox, float *oy)
{
    const float len = hypotf(vx, vy);
    if (len > 1e-6f) {
        *ox = vx / len;
        *oy = vy / len;
    } else {
        *ox = 0.0f;
        *oy = 0.0f;
    }
}

EntityId combat_spawn_orb(GameState *g, float x, float y)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL) {
        return invalid;
    }
    const EntityId id = game_spawn_entity(g, COMP_TRANSFORM | COMP_VELOCITY | COMP_INK_ORB);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }

    const float angle = ink_rng_f01(&g->rng) * k_two_pi;
    const float speed = 35.0f + 60.0f * ink_rng_f01(&g->rng);
    const float cx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float cy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float ox = ink_clampf(x + (cx - x) * 0.10f, 95.0f, (float)INK_SCREEN_W - 95.0f);
    const float oy = ink_clampf(y + (cy - y) * 0.10f, 95.0f, (float)INK_SCREEN_H - 80.0f);

    TransformComp *tr = game_transform_mut(g, id);
    if (tr != NULL) {
        tr->x = ox;
        tr->y = oy;
        tr->radius = 8.0f;
    }
    VelocityComp *vel = game_velocity_mut(g, id);
    if (vel != NULL) {
        vel->vx = cosf(angle) * speed;
        vel->vy = sinf(angle) * speed;
        vel->drag = 0.94f;
        vel->fluid_coupling = 2.5f;
    }
    InkOrbComp *orb = game_orb_mut(g, id);
    if (orb != NULL) {
        orb->value = 16;
        orb->age = 0.0f;
    }
    return id;
}

static void kill_enemy(GameState *g, EntityId victim, uint32_t dmg_flags, float kx, float ky)
{
    const TransformComp *tr = game_transform(g, victim);
    const EnemyComp *en = game_enemy(g, victim);
    if (tr == NULL || en == NULL) {
        return;
    }
    const float vx = tr->x;
    const float vy = tr->y;
    const EnemyDefId def_id = en->def_id;
    const bool is_elite = en->is_elite;
    const uint32_t elite_flags = en->elite_flags;
    const float absorbed_ink = en->absorbed_ink;
    const EnemyDef *def = enemy_def_get(def_id);
    if (def == NULL) {
        return;
    }

    ecs_destroy(&g->ecs, victim);

    const bool is_boss = game_is_boss_enemy(def_id);
    if (is_boss) {
        g->internal.last_boss_x = vx;
        g->internal.last_boss_y = vy;
    }

    if (g->run.chain_t > 0.0f) {
        ++g->run.chain;
    } else {
        g->run.chain = 1;
    }
    g->run.chain_t = g->internal.mods.chain_grace;
    if (g->run.chain > g->run.max_chain) {
        g->run.max_chain = g->run.chain;
    }

    if (g->run.kills < META_MAX_COUNTERS) {
        ++g->run.kills;
    }
    if (!is_boss && (elite_flags & ENEMY_FLAG_SUMMONED) == 0u &&
        g->run.wave_index < GAME_BOSS_WAVE_INDEX) {
        ++g->run.wave_kills;
    }

    const int64_t mult = g->run.chain > 1 ? (int64_t)g->run.chain : 1;
    const int64_t base_score = is_elite ? ((int64_t)def->score * 3) : (int64_t)def->score;
    add_score(g, base_score * mult);

    float ix = 0.0f;
    float iy = 0.0f;
    normalize_vec2(kx, ky, &ix, &iy);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_KILL,
                                 .x = vx,
                                 .y = vy,
                                 .x2 = ix,
                                 .y2 = iy,
                                 .value = (int32_t)def_id,
                                 .flags = dmg_flags,
                             });

    if (g->run.chain >= 2) {
        const int32_t capped = g->run.chain > 4 ? 4 : g->run.chain;
        g->player.ink = ink_clampf(g->player.ink + 8.0f * (float)capped, 0.0f, g->player.ink_max);
        g->player.dry = g->player.ink < 20.0f;
        const bool was_surging = g->player.surge_t > 0.0f;
        g->player.surge_t = g->internal.mods.surge_duration;
        if (!was_surging) {
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_SURGE_START,
                                         .x = g->player.cursor_x,
                                         .y = g->player.cursor_y,
                                         .value = g->run.chain,
                                         .flags = 0,
                                     });
        }
        if (g->run.chain % 3 == 0) {
            static const float k_chain_dye[FLUID_MAX_CHANNELS] = {0.55f, 0.0f, 1.15f, 0.0f};
            fluid_splat(&g->fluid, vx, vy, 6.2f, 0.0f, 0.0f, k_chain_dye);
        }
    }

    const float splat_fx = ink_clampf(kx * 0.06f, -30.0f, 30.0f);
    const float splat_fy = ink_clampf(ky * 0.06f, -30.0f, 30.0f);
    if (def_id == ENEMY_LANTERN_STORM) {
        static const float k_storm_death[FLUID_MAX_CHANNELS] = {1.05f, 0.0f, 1.35f, 0.0f};
        fluid_splat(&g->fluid, vx, vy, 6.2f, splat_fx, splat_fy, k_storm_death);
    } else if (def_id == ENEMY_INK_LEECH) {
        const float leech_ink = 0.8f + fminf(1.7f, absorbed_ink * 0.6f);
        const float leech_fire = (dmg_flags & DMG_FIRE) != 0u ? 1.35f : 0.0f;
        const float leech_dye[FLUID_MAX_CHANNELS] = {leech_ink, leech_fire, 0.0f, 0.0f};
        fluid_splat(&g->fluid, vx, vy, 8.5f, splat_fx, splat_fy, leech_dye);
    } else {
        const float splash_rad = is_boss ? 11.0f : 6.2f;
        const float fire_splash =
            (def_id == ENEMY_LANTERN_FIRE || is_boss || (dmg_flags & DMG_FIRE) != 0u) ? 1.35f
                                                                                      : 0.35f;
        const float death_dye[FLUID_MAX_CHANNELS] = {1.05f, fire_splash, 0.0f, 0.0f};
        fluid_splat(&g->fluid, vx, vy, splash_rad, splat_fx, splat_fy, death_dye);
    }

    const int32_t orb_count = is_elite ? 5 : def->orb_count;
    for (int32_t k = 0; k < orb_count; ++k) {
        (void)combat_spawn_orb(g, vx, vy);
    }

    upgrades_fire_on_kill(g, victim, vx, vy, dmg_flags);
}

int32_t combat_apply_damage_f(GameState *g, EntityId target, float base_dmg, uint32_t dmg_flags,
                              float kx, float ky)
{
    if (g == NULL || !ecs_is_alive(&g->ecs, target)) {
        return 0;
    }
    const TransformComp *tr = game_transform(g, target);
    HealthComp *hp = game_health_mut(g, target);
    const StatusComp *st = game_status(g, target);
    const EnemyComp *en = game_enemy(g, target);
    if (tr == NULL || hp == NULL || st == NULL || en == NULL || tr->dissolving ||
        hp->invuln_t > 0.0f) {
        return 0;
    }

    const RunModifiers *mods = &g->internal.mods;
    const bool is_boss = game_is_boss_enemy(en->def_id);
    float dmg = base_dmg;
    upgrades_fire_on_hit(g, target, &dmg, dmg_flags);
    if ((dmg_flags & DMG_FIRE) != 0u) {
        dmg *= mods->fire_dmg_mul;
        if (en->def_id == ENEMY_INK_LEECH) {
            dmg *= 1.0f + fminf(1.0f, en->absorbed_ink * 0.25f);
        }
    }
    if (g->player.dry && (dmg_flags & DMG_DEFLECTED) != 0u) {
        dmg *= mods->dry_deflect_dmg_mul;
    }

    if (st->ink_mark_t > 0.0f) {
        dmg *= is_boss ? (1.5f * (mods->ink_mark_mul * 0.5f)) : mods->ink_mark_mul;
    }

    /* Elite & Boss resistances; dual-element (FIRE | STORM) uses the lower resistance. */
    if ((en->elite_flags & ELITE_RIGID) != 0u && (dmg_flags & DMG_SLAM) != 0u) {
        dmg *= 0.5f;
    }
    const bool has_fire = (dmg_flags & DMG_FIRE) != 0u;
    const bool has_storm = (dmg_flags & DMG_STORM) != 0u;
    const float m_fire = (en->elite_flags & ELITE_FIREPROOF) != 0u ? 0.3f : 1.0f;
    const float m_storm =
        (en->elite_flags & ELITE_STORMPROOF) != 0u ? 0.3f : (is_boss ? 0.5f : 1.0f);
    if (has_fire && has_storm) {
        dmg *= fmaxf(m_fire, m_storm);
    } else if (has_fire) {
        dmg *= m_fire;
    } else if (has_storm) {
        dmg *= m_storm;
    }

    if (en->def_id == ENEMY_BOSS_RYU && (dmg_flags & DMG_CATACLYSM) != 0u) {
        const BossComp *boss_c = game_boss(g, target);
        if (boss_c != NULL && boss_c->phase_idx >= 1) {
            dmg *= 2.0f;
        }
    }

    const float clamped = ink_clampf(dmg, 1.0f, 1000000.0f);
    const int32_t final_dmg = (int32_t)floorf(clamped + 0.5f);
    hp->hp -= final_dmg;
    hp->flash_t = 1.0f;

    if (hp->hp > 0 && is_boss) {
        BossComp *boss = game_boss_mut(g, target);
        if (boss != NULL) {
            if (en->def_id == ENEMY_BOSS_TENGU && boss->phase_idx == 0 &&
                hp->hp * 2 <= hp->max_hp) {
                boss->phase_idx = 1;
                hp->invuln_t = 1.5f;
                boss->gale_t = 0.0f;
                boss->telegraph = 0.0f;
                boss->telegraph_pattern = 0;
                static const float k_phase_dye[FLUID_MAX_CHANNELS] = {0.8f, 0.4f, 0.0f, 0.0f};
                fluid_swirl(&g->fluid, tr->x, tr->y, 14.0f, 45.0f, k_phase_dye);
                (void)game_push_event(g, (GameEvent){
                                             .type = EV_BOSS_PHASE,
                                             .x = tr->x,
                                             .y = tr->y,
                                             .value = 2,
                                             .flags = 0,
                                         });
            } else if (en->def_id == ENEMY_BOSS_NUE && boss->phase_idx == 0 &&
                       hp->hp * 2 <= hp->max_hp) {
                boss->phase_idx = 1;
                hp->invuln_t = 1.5f;
                boss->telegraph = 0.0f;
                boss->telegraph_pattern = 0;
                static const float k_nue_dye[FLUID_MAX_CHANNELS] = {0.8f, 0.0f, 0.8f, 0.0f};
                fluid_swirl(&g->fluid, tr->x, tr->y, 14.0f, 45.0f, k_nue_dye);
                (void)game_push_event(g, (GameEvent){
                                             .type = EV_BOSS_PHASE,
                                             .x = tr->x,
                                             .y = tr->y,
                                             .value = 2,
                                             .flags = 0,
                                         });
            } else if (en->def_id == ENEMY_BOSS_RYU) {
                if (hp->hp * 3 <= hp->max_hp) {
                    boss->part_id = 2;
                    if (boss->phase_idx == 0) {
                        boss->phase_idx = 1;
                        hp->invuln_t = 1.5f;
                        boss->gale_t = 0.0f;
                        boss->telegraph = 0.0f;
                        boss->telegraph_pattern = 0;
                        static const float k_ryu_dye[FLUID_MAX_CHANNELS] = {0.9f, 0.0f, 0.0f, 0.0f};
                        fluid_swirl(&g->fluid, tr->x, tr->y, 16.0f, 50.0f, k_ryu_dye);
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
            }
        }
    }

    if (hp->hp <= 0) {
        hp->hp = 0;
        kill_enemy(g, target, dmg_flags, kx, ky);
    }
    return final_dmg;
}

int32_t combat_apply_damage(GameState *g, EntityId target, int32_t base_dmg, uint32_t dmg_flags,
                            float kx, float ky)
{
    return combat_apply_damage_f(g, target, (float)base_dmg, dmg_flags, kx, ky);
}

void combat_damage_shrine(GameState *g, float source_x, float source_y, int32_t dmg)
{
    if (g == NULL || g->shrine_count <= 0 || dmg <= 0) {
        return;
    }
    ShrineState *sh = &g->shrines[0];
    if (sh->invuln_t > 0.0f || sh->hp <= 0) {
        return;
    }
    sh->hp -= dmg;
    if (sh->hp < 0) {
        sh->hp = 0;
    }
    sh->invuln_t = 0.95f + g->internal.mods.shrine_invuln_add;
    g->run.chain = 0;
    g->run.chain_t = 0.0f;

    static const float k_shrine_dye[FLUID_MAX_CHANNELS] = {0.2f, 0.0f, 0.0f, 0.0f};
    fluid_swirl(&g->fluid, sh->x, sh->y, 11.0f, 34.0f, k_shrine_dye);

    float dx = 0.0f;
    float dy = 0.0f;
    normalize_vec2(source_x - sh->x, source_y - sh->y, &dx, &dy);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_SHRINE_HIT,
                                 .x = sh->x,
                                 .y = sh->y,
                                 .x2 = dx,
                                 .y2 = dy,
                                 .value = sh->hp,
                                 .flags = 0,
                             });
}

static float slam_damage_for_enemy(EnemyDefId def_id, MassTier tier, int32_t max_hp)
{
    if (def_id == ENEMY_BOSS_NUE) {
        return 60.0f;
    }
    switch (tier) {
    case MASS_LIGHT:
        return (float)(max_hp > 0 ? max_hp * 4 : 1000);
    case MASS_MEDIUM:
        return 60.0f;
    case MASS_HEAVY:
        return 80.0f;
    default:
        return 0.0f;
    }
}

void combat_physics_and_slams_step(GameState *g)
{
    if (g == NULL) {
        return;
    }
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float contact_r = g->shrine_count > 0 ? g->shrines[0].hit_radius_contact : 42.0f;

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TransformComp *tr = game_transform_mut(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        const HealthComp *hp = game_health(g, id);
        StatusComp *st = game_status_mut(g, id);
        EnemyComp *en = game_enemy_mut(g, id);
        if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || tr->dissolving) {
            continue;
        }
        const bool is_nue = (en->def_id == ENEMY_BOSS_NUE);
        const bool nue_slam_active = (is_nue && en->ai_state == 1 && st->knockback_t > 0.0f);
        if (vel->mass_tier == MASS_TURRET ||
            ((vel->mass_tier == MASS_BOSS || is_nue) && !nue_slam_active)) {
            continue;
        }

        const float prev_x = tr->x;
        const float prev_y = tr->y;
        tr->x += vel->vx * INK_FIXED_DT;
        tr->y += vel->vy * INK_FIXED_DT;

        const float esp = hypotf(vel->vx, vel->vy);
        const bool hit_border = tr->x < 46.0f || tr->x > (float)INK_SCREEN_W - 46.0f ||
                                tr->y < 76.0f || tr->y > (float)INK_SCREEN_H - 52.0f;
        const bool hit_solid = fluid_is_solid(&g->fluid, tr->x, tr->y);

        if (hit_border || hit_solid) {
            float nx = 0.0f;
            float ny = 0.0f;
            if (hit_border) {
                if (tr->x < 46.0f) {
                    nx += 1.0f;
                } else if (tr->x > (float)INK_SCREEN_W - 46.0f) {
                    nx -= 1.0f;
                }
                if (tr->y < 76.0f) {
                    ny += 1.0f;
                } else if (tr->y > (float)INK_SCREEN_H - 52.0f) {
                    ny -= 1.0f;
                }
                normalize_vec2(nx, ny, &nx, &ny);
                tr->x = ink_clampf(tr->x, 46.0f, (float)INK_SCREEN_W - 46.0f);
                tr->y = ink_clampf(tr->y, 76.0f, (float)INK_SCREEN_H - 52.0f);
            } else {
                (void)fluid_solid_normal(&g->fluid, tr->x, tr->y, &nx, &ny);
                tr->x = prev_x;
                tr->y = prev_y;
            }

            if (st->knockback_t > 0.0f && esp > 215.0f) {
                const float slam_dmg =
                    slam_damage_for_enemy(en->def_id, vel->mass_tier, hp->max_hp);
                if (vel->mass_tier == MASS_HEAVY || nue_slam_active) {
                    if (st->stun_t < 1.0f) {
                        st->stun_t = 1.0f;
                    }
                }
                st->knockback_t = 0.0f;
                (void)game_push_event(g, (GameEvent){
                                             .type = EV_SLAM,
                                             .x = tr->x,
                                             .y = tr->y,
                                             .x2 = nx,
                                             .y2 = ny,
                                             .value = (int32_t)en->def_id,
                                             .flags = EVF_SLAM_WALL,
                                         });
                if (g->hitstop_t < 0.05f) {
                    g->hitstop_t = 0.05f;
                }
                (void)combat_apply_damage_f(g, id, slam_dmg, DMG_PHYSICAL | DMG_SLAM, vel->vx,
                                            vel->vy);
                if (!ecs_is_alive(&g->ecs, id)) {
                    continue;
                }
                vel->vx *= -0.4f;
                vel->vy *= -0.4f;
            } else if (hit_solid && !hit_border) {
                /* Smooth tangent slide around solid pillars with deterministic tie-breaking. */
                const float vn = vel->vx * nx + vel->vy * ny;
                float tvx = vel->vx;
                float tvy = vel->vy;
                if (vn < 0.0f) {
                    tvx -= vn * nx;
                    tvy -= vn * ny;
                }
                if (hypotf(tvx, tvy) < 15.0f && (nx != 0.0f || ny != 0.0f)) {
                    const float sign = (id.index & 1u) != 0u ? 1.0f : -1.0f;
                    const float sp = fmaxf(esp, 40.0f);
                    tvx = -ny * sign * sp;
                    tvy = nx * sign * sp;
                }
                const float slide_x =
                    ink_clampf(prev_x + tvx * INK_FIXED_DT, 46.0f, (float)INK_SCREEN_W - 46.0f);
                const float slide_y =
                    ink_clampf(prev_y + tvy * INK_FIXED_DT, 76.0f, (float)INK_SCREEN_H - 52.0f);
                if (!fluid_is_solid(&g->fluid, slide_x, slide_y)) {
                    tr->x = slide_x;
                    tr->y = slide_y;
                }
                vel->vx = tvx;
                vel->vy = tvy;
            } else {
                vel->vx *= -0.4f;
                vel->vy *= -0.4f;
            }
        }

        if (nue_slam_active) {
            continue;
        }

        if (st->knockback_t > 0.0f && esp > 205.0f) {
            for (uint32_t j = 0; j < hw; ++j) {
                if (j == i) {
                    continue;
                }
                const EntityId other = ecs_entity_at(&g->ecs, j);
                if (other.index == ECS_INVALID_INDEX) {
                    continue;
                }
                const TransformComp *otr = game_transform(g, other);
                const EnemyComp *oen = game_enemy(g, other);
                if (otr == NULL || oen == NULL || otr->dissolving) {
                    continue;
                }
                const float hit_rad = game_is_boss_enemy(oen->def_id) ? 54.0f : 34.0f;
                const float dx = otr->x - tr->x;
                const float dy = otr->y - tr->y;
                if (dx * dx + dy * dy < hit_rad * hit_rad) {
                    float dir_x = 0.0f;
                    float dir_y = 0.0f;
                    normalize_vec2(dx, dy, &dir_x, &dir_y);
                    const float slam_dmg =
                        slam_damage_for_enemy(en->def_id, vel->mass_tier, hp->max_hp);
                    const float svx = vel->vx;
                    const float svy = vel->vy;
                    if (vel->mass_tier == MASS_HEAVY && st->stun_t < 1.0f) {
                        st->stun_t = 1.0f;
                    }
                    st->knockback_t = 0.0f;
                    (void)game_push_event(g, (GameEvent){
                                                 .type = EV_SLAM,
                                                 .x = 0.5f * (tr->x + otr->x),
                                                 .y = 0.5f * (tr->y + otr->y),
                                                 .x2 = dir_x,
                                                 .y2 = dir_y,
                                                 .value = (int32_t)en->def_id,
                                                 .flags = EVF_SLAM_ENEMY,
                                             });
                    if (g->hitstop_t < 0.05f) {
                        g->hitstop_t = 0.05f;
                    }
                    (void)combat_apply_damage(g, other, 40, DMG_PHYSICAL, svx, svy);
                    (void)combat_apply_damage_f(g, id, slam_dmg, DMG_PHYSICAL | DMG_SLAM, svx, svy);
                    break;
                }
            }
            if (!ecs_is_alive(&g->ecs, id)) {
                continue;
            }
        }

        const float dx = sx - tr->x;
        const float dy = sy - tr->y;
        const float dist = hypotf(dx, dy);
        if (dist < contact_r) {
            if (en->def_id == ENEMY_STONE_ONI) {
                combat_damage_shrine(g, tr->x, tr->y, 2);
                const float out_x = dist > 1e-6f ? (-dx / dist) : 1.0f;
                const float out_y = dist > 1e-6f ? (-dy / dist) : 0.0f;
                tr->x = sx + out_x * (contact_r + 4.0f);
                tr->y = sy + out_y * (contact_r + 4.0f);
                en->ai_state = 1;
                en->fire_cd = 1.2f;
                vel->vx = out_x * 55.0f;
                vel->vy = out_y * 55.0f;
            } else {
                combat_damage_shrine(g, tr->x, tr->y, 1);
                if (dist > 1e-6f) {
                    vel->vx = -(dx / dist) * 260.0f;
                    vel->vy = -(dy / dist) * 260.0f;
                } else {
                    vel->vx = 260.0f;
                    vel->vy = 0.0f;
                }
            }
        }
    }
}

void combat_orbs_step(GameState *g)
{
    if (g == NULL) {
        return;
    }
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const uint32_t hw = g->ecs.high_water;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TransformComp *tr = game_transform_mut(g, id);
        VelocityComp *vel = game_velocity_mut(g, id);
        InkOrbComp *orb = game_orb_mut(g, id);
        if (tr == NULL || vel == NULL || orb == NULL) {
            continue;
        }
        orb->age += INK_FIXED_DT;

        float fu = 0.0f;
        float fv = 0.0f;
        fluid_sample(&g->fluid, tr->x, tr->y, &fu, &fv, NULL);
        vel->vx = vel->vx * 0.94f + fu * FLUID_CELL_PX * 2.5f * INK_FIXED_DT;
        vel->vy = vel->vy * 0.94f + fv * FLUID_CELL_PX * 2.5f * INK_FIXED_DT;

        const double bdx = (double)g->player.cursor_x - (double)tr->x;
        const double bdy = (double)g->player.cursor_y - (double)tr->y;
        const double bd = hypot(bdx, bdy);
        const double pull_r = (double)g->internal.mods.orb_pull_radius;
        if (bd < pull_r && bd > 1e-6) {
            const float pull = (float)((pull_r - bd) * 5.2);
            vel->vx += (float)(bdx / bd) * pull * INK_FIXED_DT;
            vel->vy += (float)(bdy / bd) * pull * INK_FIXED_DT;
        }

        const float nx =
            ink_clampf(tr->x + vel->vx * INK_FIXED_DT, 40.0f, (float)INK_SCREEN_W - 40.0f);
        const float ny =
            ink_clampf(tr->y + vel->vy * INK_FIXED_DT, 78.0f, (float)INK_SCREEN_H - 52.0f);
        if (!fluid_is_solid(&g->fluid, nx, ny)) {
            tr->x = nx;
            tr->y = ny;
        } else {
            float snx = 0.0f;
            float sny = 0.0f;
            if (fluid_solid_normal(&g->fluid, nx, ny, &snx, &sny)) {
                const float vn = vel->vx * snx + vel->vy * sny;
                if (vn < 0.0f) {
                    vel->vx -= vn * snx;
                    vel->vy -= vn * sny;
                }
            }
        }

        const float sd = hypotf(sx - tr->x, sy - tr->y);
        if (bd < 46.0 || sd < 44.0f) {
            g->player.ink = ink_clampf(g->player.ink + (float)orb->value, 0.0f, g->player.ink_max);
            g->player.dry = g->player.ink < 20.0f;
            add_score(g, 25);
            if (g->internal.mods.rosary_swirl) {
                static const float k_rosary_dye[FLUID_MAX_CHANNELS] = {0.25f, 0.0f, 0.0f, 0.0f};
                fluid_swirl(&g->fluid, tr->x, tr->y, 6.0f, 36.0f, k_rosary_dye);
            }
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_ORB_PICKUP,
                                         .x = tr->x,
                                         .y = tr->y,
                                         .value = orb->value,
                                         .flags = 0,
                                     });
            ecs_destroy(&g->ecs, id);
        } else if (orb->age > 11.5f) {
            ecs_destroy(&g->ecs, id);
        }
    }
}

void combat_begin_wave_aftermath(GameState *g)
{
    if (g == NULL) {
        return;
    }
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TransformComp *tr = game_transform_mut(g, id);
        if (tr == NULL) {
            continue;
        }
        if (ecs_has_mask(&g->ecs, id, COMP_ENEMY) || ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            tr->dissolving = true;
            tr->dissolve_t = 0.0f;
        } else if (ecs_has_mask(&g->ecs, id, COMP_INK_ORB)) {
            const InkOrbComp *orb = game_orb(g, id);
            if (orb != NULL) {
                g->player.ink =
                    ink_clampf(g->player.ink + (float)orb->value, 0.0f, g->player.ink_max);
                g->player.dry = g->player.ink < 20.0f;
                add_score(g, 25);
                (void)game_push_event(g, (GameEvent){
                                             .type = EV_ORB_PICKUP,
                                             .x = tr->x,
                                             .y = tr->y,
                                             .value = orb->value,
                                             .flags = EVF_ORB_AUTO,
                                         });
            }
            ecs_destroy(&g->ecs, id);
        } else if (ecs_has_mask(&g->ecs, id, COMP_GIMMICK)) {
            const GimmickComp *gim = game_gimmick(g, id);
            if (gim != NULL && gim->kind != (int32_t)GIMMICK_SPIRIT_PILLAR) {
                ecs_destroy(&g->ecs, id);
            }
        }
    }
    ecs_flush_destroy(&g->ecs);
    g->phase = RUN_PHASE_WAVE_AFTERMATH;
    g->phase_t = 0.0f;
}
