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
    if (!is_boss && g->run.wave_index < GAME_BOSS_WAVE_INDEX) {
        ++g->run.wave_kills;
    }

    const int64_t mult = g->run.chain > 1 ? (int64_t)g->run.chain : 1;
    add_score(g, (int64_t)def->score * mult);

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

    const float splash_rad = is_boss ? 11.0f : 6.2f;
    const float fire_splash =
        (def_id == ENEMY_LANTERN_FIRE || is_boss || (dmg_flags & DMG_FIRE) != 0) ? 1.35f : 0.35f;
    const float death_dye[FLUID_MAX_CHANNELS] = {1.05f, fire_splash, 0.0f, 0.0f};
    fluid_splat(&g->fluid, vx, vy, splash_rad, ink_clampf(kx * 0.06f, -30.0f, 30.0f),
                ink_clampf(ky * 0.06f, -30.0f, 30.0f), death_dye);

    for (int32_t k = 0; k < def->orb_count; ++k) {
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
    if ((dmg_flags & DMG_FIRE) != 0) {
        dmg *= mods->fire_dmg_mul;
    }
    if (g->player.dry && (dmg_flags & DMG_DEFLECTED) != 0) {
        dmg *= mods->dry_deflect_dmg_mul;
    }

    if (st->ink_mark_t > 0.0f) {
        dmg *= is_boss ? (1.5f * (mods->ink_mark_mul * 0.5f)) : mods->ink_mark_mul;
    }
    if (is_boss && (dmg_flags & DMG_STORM) != 0) {
        dmg *= 0.5f;
    }

    const float clamped = ink_clampf(dmg, 1.0f, 1000000.0f);
    const int32_t final_dmg = (int32_t)floorf(clamped + 0.5f);
    hp->hp -= final_dmg;
    hp->flash_t = 1.0f;

    if (hp->hp > 0 && en->def_id == ENEMY_BOSS_TENGU) {
        BossComp *boss = game_boss_mut(g, target);
        if (boss != NULL && boss->phase_idx == 0 && hp->hp * 2 <= hp->max_hp) {
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

static float slam_damage_for_tier(MassTier tier, int32_t max_hp)
{
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
        const StatusComp *st = game_status(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || vel == NULL || hp == NULL || st == NULL || en == NULL || tr->dissolving) {
            continue;
        }
        if (vel->mass_tier == MASS_TURRET || vel->mass_tier == MASS_BOSS) {
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
                const float slam_dmg = slam_damage_for_tier(vel->mass_tier, hp->max_hp);
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
            }
            vel->vx *= -0.4f;
            vel->vy *= -0.4f;
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
                    const float slam_dmg = slam_damage_for_tier(vel->mass_tier, hp->max_hp);
                    const float svx = vel->vx;
                    const float svy = vel->vy;
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
