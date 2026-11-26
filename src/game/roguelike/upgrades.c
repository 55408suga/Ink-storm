#include "roguelike/upgrades.h"

#include "combat/damage.h"
#include "core.h"
#include "roguelike/relics.h"

#include <math.h>
#include <stddef.h>

static uint8_t clamp_meta_level(const RunConfig *cfg, MetaUpgradeKind kind)
{
    const uint8_t max_lv = meta_upgrade_max_level(kind);
    const uint8_t lv = cfg->meta_upgrade_lv[kind];
    return lv > max_lv ? max_lv : lv;
}

static void ember_tip_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv < 1 || lv > 3) {
        return;
    }
    static const float k_intervals[3] = {160.0f, 120.0f, 80.0f};
    static const float k_ember_mul[3] = {0.75f, 0.5625f, 0.421875f};
    if (mods->stroke_ember_interval > 0.0f) {
        mods->stroke_ember_interval *= k_ember_mul[lv - 1];
    } else {
        mods->stroke_ember_interval = k_intervals[lv - 1];
    }
    if (lv >= 3) {
        mods->stroke_ember_mul = 1.5f;
    }
}

static void wildfire_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_muls[3] = {1.25f, 1.50f, 1.80f};
    if (lv >= 1 && lv <= 3) {
        mods->burn_rate_mul *= k_muls[lv - 1];
    }
}

static void flame_gale_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_fire[3] = {0.25f, 0.35f, 0.50f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->gale_fire_add += k_fire[lv - 1];
    if (lv >= 3) {
        mods->gale_end_blast_dmg = 20.0f;
    }
}

static void flame_gale_on_gale(const GaleHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 3 || g->internal.mods.gale_end_blast_dmg <= 0.0f) {
        return;
    }
    const float ex = a->x + a->dx * 220.0f;
    const float ey = a->y + a->dy * 220.0f;
    static const float k_blast_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.8f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, ex, ey, 4.5f, 0.0f, 0.0f, k_blast_dye);

    const float rad2 = 48.0f * 48.0f;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX || !ecs_is_alive(&g->ecs, id)) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const EnemyComp *en = game_enemy(g, id);
        if (tr == NULL || en == NULL || tr->dissolving) {
            continue;
        }
        const float dx = tr->x - ex;
        const float dy = tr->y - ey;
        if (dx * dx + dy * dy <= rad2) {
            (void)combat_apply_damage_f(g, id, g->internal.mods.gale_end_blast_dmg,
                                        DMG_FIRE | DMG_SECONDARY, 0.0f, 0.0f);
        }
    }
}

static void inferno_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_dmg[3] = {14.0f, 18.0f, 22.0f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->burn_base_dmg = k_dmg[lv - 1];
    if (lv >= 3) {
        mods->inferno_ember_on_kill = true;
    }
}

static void inferno_on_kill(const KillHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 3 || (a->dmg_flags & DMG_FIRE) == 0) {
        return;
    }
    static const float k_inferno_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.75f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, a->x, a->y, 3.5f, 0.0f, 0.0f, k_inferno_dye);
}

static void phoenix_arrow_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const int32_t k_pierce[3] = {3, 4, 5};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->burning_proj_pierce = k_pierce[lv - 1];
    if (lv >= 3) {
        mods->burning_proj_hit_splat = true;
    }
}

static void charged_seal_on_cyclone(const CycloneHookArgs *a)
{
    static const float k_storm[3] = {0.30f, 0.45f, 0.60f};
    if (a->lv < 1 || a->lv > 3) {
        return;
    }
    if (a->storm_add != NULL) {
        *a->storm_add += k_storm[a->lv - 1];
    }
    if (a->lv >= 3 && a->storm_sustain_add != NULL) {
        *a->storm_sustain_add += 0.60f;
    }
}

static void chain_lightning_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 1 && lv <= 3) {
        mods->chain_hops_add += (int32_t)lv;
    }
}

static void chain_lightning_apply_mul(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 3) {
        mods->chain_range_mul *= 1.3f;
    }
}

static void thunder_arrow_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const int32_t k_targets[3] = {2, 3, 3};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->thunder_proj_targets = k_targets[lv - 1];
    if (lv >= 3) {
        mods->deflect_auto_electrify = true;
    }
}

static void swift_thunder_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_cd[3] = {0.45f, 0.38f, 0.30f};
    if (lv >= 1 && lv <= 3) {
        mods->storm_zap_cd = k_cd[lv - 1];
    }
}

static void thunderburst_fuse_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_thresh[3] = {0.26f, 0.22f, 0.18f};
    if (lv >= 1 && lv <= 3) {
        mods->thunderburst_thresh = k_thresh[lv - 1];
    }
}

static void thunderburst_fuse_apply_mul(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 3) {
        mods->thunderburst_radius_mul *= 1.30f;
    }
}

static void great_vortex_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_dur_add[3] = {0.2f, 0.4f, 0.6f};
    if (lv >= 1 && lv <= 3) {
        mods->cyclone_duration_add += k_dur_add[lv - 1];
    }
}

static void great_vortex_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_rad_mul[3] = {1.15f, 1.30f, 1.45f};
    if (lv >= 1 && lv <= 3) {
        mods->cyclone_radius_mul *= k_rad_mul[lv - 1];
    }
}

static void slam_shockwave_on_hit(const HitHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 1 || a->lv > 3 || (a->dmg_flags & DMG_SLAM) == 0 ||
        (a->dmg_flags & DMG_SECONDARY) != 0) {
        return;
    }
    const TransformComp *origin = game_transform(g, a->target);
    if (origin == NULL) {
        return;
    }
    static const float k_radius[3] = {64.0f, 80.0f, 96.0f};
    static const int32_t k_damage[3] = {20, 30, 40};
    const float rad = k_radius[a->lv - 1];
    const float rad2 = rad * rad;
    const int32_t dmg = k_damage[a->lv - 1];
    const float ox = origin->x;
    const float oy = origin->y;

    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId other = ecs_entity_at(&g->ecs, i);
        if (other.index == ECS_INVALID_INDEX || other.index == a->target.index) {
            continue;
        }
        const TransformComp *tr = game_transform(g, other);
        const EnemyComp *en = game_enemy(g, other);
        if (tr == NULL || en == NULL || tr->dissolving) {
            continue;
        }
        const float dx = tr->x - ox;
        const float dy = tr->y - oy;
        const float d2 = dx * dx + dy * dy;
        if (d2 > rad2) {
            continue;
        }
        float dir_x = 1.0f;
        float dir_y = 0.0f;
        if (d2 > 1e-12f) {
            const float inv_d = 1.0f / sqrtf(d2);
            dir_x = dx * inv_d;
            dir_y = dy * inv_d;
        }
        if (a->lv >= 3) {
            VelocityComp *vel = game_velocity_mut(g, other);
            StatusComp *st = game_status_mut(g, other);
            if (vel != NULL && st != NULL && vel->mass_tier != MASS_TURRET &&
                vel->mass_tier != MASS_BOSS) {
                vel->vx += dir_x * 320.0f;
                vel->vy += dir_y * 320.0f;
                if (st->knockback_t < 0.90f) {
                    st->knockback_t = 0.90f;
                }
            }
        }
        (void)combat_apply_damage(g, other, dmg, DMG_PHYSICAL | DMG_SECONDARY, dir_x * 200.0f,
                                  dir_y * 200.0f);
    }
}

static void mighty_stroke_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_muls[3] = {1.15f, 1.30f, 1.45f};
    if (lv >= 1 && lv <= 3) {
        mods->brush_force_mul *= k_muls[lv - 1];
    }
}

static void maelstrom_bind_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 1 && lv <= 3) {
        mods->maelstrom_bind_level = (int32_t)lv;
    }
}

static void decay_hook_timer(float *t, float dt)
{
    if (*t > 0.0f) {
        *t -= dt;
        if (*t <= 1e-4f) {
            *t = 0.0f;
        }
    }
}

static bool maelstrom_can_collide(const GameState *g, uint32_t idx)
{
    return g->internal.maelstrom_bind_t[idx] > 0.0f && g->internal.maelstrom_hit_cd[idx] <= 0.0f;
}

static void maelstrom_bind_on_tick(const TickHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 1 || a->lv > 3) {
        return;
    }

    for (uint32_t idx = 0; idx < ECS_MAX_ENTITIES; ++idx) {
        const EntityId id = ecs_entity_at(&g->ecs, idx);
        const TransformComp *tr =
            id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_ENEMY)
                ? game_transform(g, id)
                : NULL;
        if (tr == NULL || tr->dissolving) {
            g->internal.maelstrom_bind_t[idx] = 0.0f;
            g->internal.maelstrom_hit_cd[idx] = 0.0f;
            continue;
        }
        decay_hook_timer(&g->internal.maelstrom_bind_t[idx], a->dt);
        decay_hook_timer(&g->internal.maelstrom_hit_cd[idx], a->dt);
    }

    const float pull_rad = 140.0f * g->internal.mods.cyclone_radius_mul;
    const float pull_rad2 = pull_rad * pull_rad;
    const float blend = ink_clampf(6.0f * a->dt, 0.0f, 1.0f);
    const uint32_t hw = g->ecs.high_water;

    for (int32_t c = 0; c < GAME_MAX_ACTIVE_CYCLONES; ++c) {
        const ActiveCyclone *ac = &g->internal.cyclones[c];
        if (!ac->active) {
            continue;
        }
        for (uint32_t i = 0; i < hw; ++i) {
            const EntityId id = ecs_entity_at(&g->ecs, i);
            if (id.index == ECS_INVALID_INDEX) {
                continue;
            }
            const TransformComp *tr = game_transform(g, id);
            VelocityComp *vel = game_velocity_mut(g, id);
            const EnemyComp *en = game_enemy(g, id);
            if (tr == NULL || vel == NULL || en == NULL || tr->dissolving) {
                continue;
            }
            const MassTier eff_tier = game_effective_mass_tier(g, vel->mass_tier);
            const bool eligible = eff_tier == MASS_LIGHT || (a->lv >= 2 && eff_tier == MASS_MEDIUM);
            if (!eligible) {
                continue;
            }
            const float dx = ac->x - tr->x;
            const float dy = ac->y - tr->y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > pull_rad2) {
                continue;
            }
            float target_vx = 0.0f;
            float target_vy = 0.0f;
            if (d2 > 1e-8f) {
                const float inv_d = 1.0f / sqrtf(d2);
                target_vx = dx * inv_d * 240.0f;
                target_vy = dy * inv_d * 240.0f;
            }
            vel->vx += (target_vx - vel->vx) * blend;
            vel->vy += (target_vy - vel->vy) * blend;
            g->internal.maelstrom_bind_t[id.index] = 1.0f;
        }
    }

    for (uint32_t i = 0; i < hw; ++i) {
        if (g->internal.maelstrom_bind_t[i] <= 0.0f) {
            continue;
        }
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        StatusComp *st = game_status_mut(g, id);
        if (st != NULL && st->stun_t < 0.15f) {
            st->stun_t = 0.15f;
        }
    }

    if (a->lv >= 3) {
        const float collide_rad2 = 32.0f * 32.0f;
        const float hit_cooldown = 0.45f;
        for (uint32_t i = 0; i < hw; ++i) {
            if (!maelstrom_can_collide(g, i)) {
                continue;
            }
            const EntityId id_a = ecs_entity_at(&g->ecs, i);
            if (id_a.index == ECS_INVALID_INDEX || !ecs_is_alive(&g->ecs, id_a)) {
                continue;
            }
            const TransformComp *tr_a = game_transform(g, id_a);
            const EnemyComp *en_a = game_enemy(g, id_a);
            if (tr_a == NULL || en_a == NULL || tr_a->dissolving) {
                continue;
            }
            for (uint32_t j = i + 1; j < hw; ++j) {
                if (!maelstrom_can_collide(g, j)) {
                    continue;
                }
                const EntityId id_b = ecs_entity_at(&g->ecs, j);
                if (id_b.index == ECS_INVALID_INDEX || !ecs_is_alive(&g->ecs, id_b)) {
                    continue;
                }
                const TransformComp *tr_b = game_transform(g, id_b);
                const EnemyComp *en_b = game_enemy(g, id_b);
                if (tr_b == NULL || en_b == NULL || tr_b->dissolving) {
                    continue;
                }
                const float dx = tr_b->x - tr_a->x;
                const float dy = tr_b->y - tr_a->y;
                if (dx * dx + dy * dy <= collide_rad2) {
                    g->internal.maelstrom_bind_t[i] = 0.0f;
                    g->internal.maelstrom_bind_t[j] = 0.0f;
                    g->internal.maelstrom_hit_cd[i] = hit_cooldown;
                    g->internal.maelstrom_hit_cd[j] = hit_cooldown;
                    if (ecs_is_alive(&g->ecs, id_a)) {
                        (void)combat_apply_damage(g, id_a, 40, DMG_PHYSICAL | DMG_SECONDARY, 0.0f,
                                                  0.0f);
                    }
                    if (ecs_is_alive(&g->ecs, id_b)) {
                        (void)combat_apply_damage(g, id_b, 40, DMG_PHYSICAL | DMG_SECONDARY, 0.0f,
                                                  0.0f);
                    }
                    break;
                }
            }
        }
    }
}

static void tailwind_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_steer[3] = {1.30f, 1.60f, 2.00f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->homing_steer_mul *= k_steer[lv - 1];
    if (lv >= 3) {
        mods->deflected_max_speed_mul *= 1.20f;
    }
}

static void rich_ink_on_kill(const KillHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 1 || a->lv > 3) {
        return;
    }
    static const float k_chance[3] = {0.33f, 0.66f, 1.0f};
    const float roll = ink_rng_f01(&g->rng);
    if (roll < k_chance[a->lv - 1]) {
        (void)combat_spawn_orb(g, a->x, a->y);
    }
}

static void lasting_surge_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_surge_add[3] = {1.0f, 2.0f, 3.0f};
    if (lv >= 1 && lv <= 3) {
        mods->surge_duration += k_surge_add[lv - 1];
    }
}

static void ink_magnet_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_rad[3] = {170.0f, 210.0f, 250.0f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->orb_pull_radius = k_rad[lv - 1];
    if (lv >= 3) {
        mods->cyclone_pulls_all_orbs = true;
    }
}

static void chain_primer_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_grace[3] = {3.20f, 3.80f, 4.50f};
    if (lv >= 1 && lv <= 3) {
        mods->chain_grace = k_grace[lv - 1];
    }
}

static void dry_mastery_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_force[3] = {0.75f, 1.00f, 1.00f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->dry_force_override = k_force[lv - 1];
    if (lv >= 3) {
        mods->dry_zero_cost = true;
    }
}

static void dry_mastery_apply_mul(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 3) {
        mods->dry_deflect_dmg_mul *= 1.20f;
    }
}

static void shrine_ward_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 1 && lv <= 3) {
        mods->shrine_max_hp_delta += (int32_t)lv;
    }
}

static void deep_ink_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_add[3] = {15.0f, 30.0f, 45.0f};
    if (lv >= 1 && lv <= 3) {
        mods->ink_max_add += k_add[lv - 1];
    }
}

static void deep_mark_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_mul[3] = {2.20f, 2.40f, 2.60f};
    static const float k_dur[3] = {1.0f, 2.0f, 3.0f};
    if (lv >= 1 && lv <= 3) {
        mods->ink_mark_mul = k_mul[lv - 1];
        mods->ink_mark_duration += k_dur[lv - 1];
    }
}

static void barrier_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_invuln[3] = {0.30f, 0.60f, 0.90f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->shrine_invuln_add += k_invuln[lv - 1];
    if (lv >= 3) {
        mods->barrier_shield_enabled = true;
    }
}

static void barrier_on_tick(const TickHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 3 || !g->internal.mods.barrier_shield_enabled || g->internal.barrier_shield_ready) {
        return;
    }
    if (g->internal.barrier_shield_cd > 0.0f) {
        g->internal.barrier_shield_cd -= a->dt;
    }
    if (g->internal.barrier_shield_cd <= 1e-4f) {
        g->internal.barrier_shield_cd = 0.0f;
        g->internal.barrier_shield_ready = true;
    }
}

static void lingering_embers_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_damp[3] = {0.945f, 0.955f, 0.965f};
    if (lv >= 1 && lv <= 3) {
        mods->fire_damp_override = k_damp[lv - 1];
    }
}

static void storm_brush_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->chain_storm_interval = 2;
    if (lv >= 3) {
        mods->surge_stroke_storm = true;
    }
}

static void storm_brush_apply_mul(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 2 && lv <= 3) {
        mods->chain_storm_mul *= 1.5f;
    }
}

static void double_gale_apply_add(RunModifiers *mods, uint8_t lv)
{
    static const float k_cd[3] = {0.85f, 0.70f, 0.70f};
    if (lv < 1 || lv > 3) {
        return;
    }
    mods->gale_cd = k_cd[lv - 1];
    if (lv >= 3) {
        mods->gale_max_charges = 2;
    }
}

static void inkstone_blessing_apply_add(RunModifiers *mods, uint8_t lv)
{
    if (lv >= 3) {
        mods->ink_regen_no_delay = true;
    }
}

static void inkstone_blessing_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_muls[3] = {1.20f, 1.40f, 1.60f};
    if (lv >= 1 && lv <= 3) {
        mods->ink_regen_mul *= k_muls[lv - 1];
    }
}

static const UpgradeDef k_upgrade_defs[UPG_COUNT] = {
    [UPG_EMBER_TIP] =
        {
            .id = UPG_EMBER_TIP,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_add = ember_tip_apply_add,
        },
    [UPG_WILDFIRE] =
        {
            .id = UPG_WILDFIRE,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_mul = wildfire_apply_mul,
        },
    [UPG_CHARGED_SEAL] =
        {
            .id = UPG_CHARGED_SEAL,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .on_cyclone = charged_seal_on_cyclone,
        },
    [UPG_CHAIN_LIGHTNING] =
        {
            .id = UPG_CHAIN_LIGHTNING,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .apply_mods_add = chain_lightning_apply_add,
            .apply_mods_mul = chain_lightning_apply_mul,
        },
    [UPG_GREAT_VORTEX] =
        {
            .id = UPG_GREAT_VORTEX,
            .axis = AXIS_GALE,
            .max_level = 3,
            .apply_mods_add = great_vortex_apply_add,
            .apply_mods_mul = great_vortex_apply_mul,
        },
    [UPG_SLAM_SHOCKWAVE] =
        {
            .id = UPG_SLAM_SHOCKWAVE,
            .axis = AXIS_GALE,
            .max_level = 3,
            .on_hit = slam_shockwave_on_hit,
        },
    [UPG_RICH_INK] =
        {
            .id = UPG_RICH_INK,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .on_kill = rich_ink_on_kill,
            .on_kill_secondary = true,
        },
    [UPG_LASTING_SURGE] =
        {
            .id = UPG_LASTING_SURGE,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = lasting_surge_apply_add,
        },
    [UPG_FLAME_GALE] =
        {
            .id = UPG_FLAME_GALE,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_add = flame_gale_apply_add,
            .on_gale = flame_gale_on_gale,
        },
    [UPG_INFERNO] =
        {
            .id = UPG_INFERNO,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_add = inferno_apply_add,
            .on_kill = inferno_on_kill,
            .on_kill_secondary = true,
        },
    [UPG_PHOENIX_ARROW] =
        {
            .id = UPG_PHOENIX_ARROW,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_add = phoenix_arrow_apply_add,
        },
    [UPG_THUNDER_ARROW] =
        {
            .id = UPG_THUNDER_ARROW,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .apply_mods_add = thunder_arrow_apply_add,
        },
    [UPG_SWIFT_THUNDER] =
        {
            .id = UPG_SWIFT_THUNDER,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .apply_mods_add = swift_thunder_apply_add,
        },
    [UPG_THUNDERBURST_FUSE] =
        {
            .id = UPG_THUNDERBURST_FUSE,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .apply_mods_add = thunderburst_fuse_apply_add,
            .apply_mods_mul = thunderburst_fuse_apply_mul,
        },
    [UPG_MIGHTY_STROKE] =
        {
            .id = UPG_MIGHTY_STROKE,
            .axis = AXIS_GALE,
            .max_level = 3,
            .apply_mods_mul = mighty_stroke_apply_mul,
        },
    [UPG_MAELSTROM_BIND] =
        {
            .id = UPG_MAELSTROM_BIND,
            .axis = AXIS_GALE,
            .max_level = 3,
            .apply_mods_add = maelstrom_bind_apply_add,
            .on_tick = maelstrom_bind_on_tick,
        },
    [UPG_TAILWIND] =
        {
            .id = UPG_TAILWIND,
            .axis = AXIS_GALE,
            .max_level = 3,
            .apply_mods_mul = tailwind_apply_mul,
        },
    [UPG_INK_MAGNET] =
        {
            .id = UPG_INK_MAGNET,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = ink_magnet_apply_add,
        },
    [UPG_CHAIN_PRIMER] =
        {
            .id = UPG_CHAIN_PRIMER,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = chain_primer_apply_add,
        },
    [UPG_DRY_MASTERY] =
        {
            .id = UPG_DRY_MASTERY,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = dry_mastery_apply_add,
            .apply_mods_mul = dry_mastery_apply_mul,
        },
    [UPG_SHRINE_WARD] =
        {
            .id = UPG_SHRINE_WARD,
            .axis = AXIS_COMMON,
            .max_level = 3,
            .apply_mods_add = shrine_ward_apply_add,
        },
    [UPG_DEEP_INK] =
        {
            .id = UPG_DEEP_INK,
            .axis = AXIS_COMMON,
            .max_level = 3,
            .apply_mods_add = deep_ink_apply_add,
        },
    [UPG_DEEP_MARK] =
        {
            .id = UPG_DEEP_MARK,
            .axis = AXIS_COMMON,
            .max_level = 3,
            .apply_mods_add = deep_mark_apply_add,
        },
    [UPG_BARRIER] =
        {
            .id = UPG_BARRIER,
            .axis = AXIS_COMMON,
            .max_level = 3,
            .apply_mods_add = barrier_apply_add,
            .on_tick = barrier_on_tick,
        },
    [UPG_LINGERING_EMBERS] =
        {
            .id = UPG_LINGERING_EMBERS,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .apply_mods_add = lingering_embers_apply_add,
        },
    [UPG_STORM_BRUSH] =
        {
            .id = UPG_STORM_BRUSH,
            .axis = AXIS_THUNDER,
            .max_level = 3,
            .apply_mods_add = storm_brush_apply_add,
            .apply_mods_mul = storm_brush_apply_mul,
        },
    [UPG_DOUBLE_GALE] =
        {
            .id = UPG_DOUBLE_GALE,
            .axis = AXIS_GALE,
            .max_level = 3,
            .apply_mods_add = double_gale_apply_add,
        },
    [UPG_INKSTONE_BLESSING] =
        {
            .id = UPG_INKSTONE_BLESSING,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = inkstone_blessing_apply_add,
            .apply_mods_mul = inkstone_blessing_apply_mul,
        },
};

void roguelike_recompute_modifiers(GameState *g)
{
    if (g == NULL) {
        return;
    }
    RunModifiers mods = {
        .brush_radius_mul = 1.0f,
        .brush_force_mul = 1.0f,
        .stroke_cost_mul = 1.0f,
        .gale_impulse_mul = 1.0f,
        .ink_regen_mul = 1.0f,
        .burn_rate_mul = 1.0f,
        .burn_base_dmg = 10.0f,
        .fire_dmg_mul = 1.0f,
        .chain_range_mul = 1.0f,
        .cyclone_radius_mul = 1.0f,
        .cyclone_cd = 1.4f,
        .gale_cd = 1.0f,
        .surge_duration = 3.6f,
        .chain_grace = 2.65f,
        .orb_pull_radius = 130.0f,
        .dry_deflect_dmg_mul = 1.0f,
        .burning_proj_pierce = 2,
        .thunder_proj_targets = 1,
        .storm_zap_cd = 0.54f,
        .thunderburst_thresh = 0.30f,
        .thunderburst_radius_mul = 1.0f,
        .homing_steer_mul = 1.0f,
        .deflected_max_speed_mul = 1.0f,
        .ink_mark_mul = 2.0f,
        .ink_mark_duration = 3.0f,
        .stroke_ember_mul = 1.0f,
        .stroke_ember_rad = 2.5f,
        .stroke_ember_amt = 0.5f,
        .ink_damp_override = 0.985f,
        .fire_damp_override = 0.935f,
        .chain_storm_interval = 3,
        .chain_storm_mul = 1.0f,
        .surge_stroke_storm = false,
        .gale_max_charges = 1,
        .cataclysm_thresh = 0.30f,
        .cataclysm_cd_base = 2.0f,
        .surge_reaction_dmg_mul = 1.0f,
        .surge_min_chain = 2,
        .thunderfire_gem = false,
        .oni_tile = false,
        .night_parade_scroll = false,
        .bonus_kill_orbs = 0,
        .spawn_interval_mul = 1.0f,
        .max_alive_add = 0,
    };

    int32_t owned_n = g->run.upgrade_count;
    if (owned_n > GAME_MAX_OWNED_UPGRADES) {
        owned_n = GAME_MAX_OWNED_UPGRADES;
    }

    /* Pass 1: brush base/additions -> meta -> owned upgrades -> owned relics. */
    switch (g->config.brush_style) {
    case BRUSH_STYLE_EMBER:
        mods.stroke_ember_interval = 72.0f;
        mods.stroke_ember_mul = 1.0f;
        mods.stroke_ember_rad = 3.0f;
        mods.stroke_ember_amt = 0.64f;
        mods.gale_fire_add += 0.35f;
        break;
    case BRUSH_STYLE_QUILL:
        mods.cyclone_cd = 1.10f;
        mods.cyclone_storm_add += 0.72f;
        mods.cyclone_storm_sustain += 0.18f;
        break;
    case BRUSH_STYLE_HEAVY:
    default:
        break;
    }

    const uint8_t ink_max_lv = clamp_meta_level(&g->config, META_UPG_INK_MAX);
    mods.ink_max_add += 10.0f * (float)ink_max_lv;
    for (int32_t i = 0; i < owned_n; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        const uint8_t lv = g->run.upgrades[i].level;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].apply_mods_add != NULL) {
            k_upgrade_defs[id].apply_mods_add(&mods, lv);
        }
    }
    relics_apply_modifiers_add(g, &mods);

    /* Pass 2: all multiplications (brush -> meta -> owned upgrades -> owned relics). */
    switch (g->config.brush_style) {
    case BRUSH_STYLE_EMBER:
        mods.stroke_cost_mul *= 1.12f;
        break;
    case BRUSH_STYLE_QUILL:
        mods.brush_radius_mul *= 0.80f;
        mods.brush_force_mul *= 0.90f;
        mods.stroke_cost_mul *= 0.90f;
        mods.orb_pull_radius *= 1.30f;
        break;
    case BRUSH_STYLE_HEAVY:
    default:
        mods.brush_radius_mul *= 1.25f;
        mods.brush_force_mul *= 1.20f;
        mods.stroke_cost_mul *= 1.15f;
        mods.gale_impulse_mul *= 1.25f;
        break;
    }

    const uint8_t regen_lv = clamp_meta_level(&g->config, META_UPG_INK_REGEN);
    mods.ink_regen_mul *= 1.0f + 0.10f * (float)regen_lv;

    for (int32_t i = 0; i < owned_n; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        const uint8_t lv = g->run.upgrades[i].level;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].apply_mods_mul != NULL) {
            k_upgrade_defs[id].apply_mods_mul(&mods, lv);
        }
    }
    relics_apply_modifiers_mul(g, &mods);

    g->internal.mods = mods;
    g->player.ink_max = fmaxf(30.0f, 100.0f + mods.ink_max_add);
    if (g->player.ink > g->player.ink_max) {
        g->player.ink = g->player.ink_max;
        g->player.dry = g->player.ink < 20.0f;
    }
    g->player.cyclone_cd_total = mods.cyclone_cd;
    g->player.gale_cd_total = mods.gale_cd;
    g->player.cyclone_cd = fminf(g->player.cyclone_cd, mods.cyclone_cd);
    g->player.gale_cd = fminf(g->player.gale_cd, mods.gale_cd);
    const int32_t prev_max_charges = g->player.gale_max_charges;
    const int32_t new_max_charges = mods.gale_max_charges > 0 ? mods.gale_max_charges : 1;
    g->player.gale_max_charges = new_max_charges;
    if (prev_max_charges > 0 && new_max_charges > prev_max_charges) {
        g->player.gale_charges += new_max_charges - prev_max_charges;
    }
    if (g->player.gale_charges > new_max_charges) {
        g->player.gale_charges = new_max_charges;
    }

    if (g->shrine_count > 0) {
        const uint8_t hp_lv = clamp_meta_level(&g->config, META_UPG_SHRINE_HP);
        int32_t max_hp = 8 + (int32_t)hp_lv + mods.shrine_max_hp_delta;
        if (max_hp < 1) {
            max_hp = 1;
        }
        g->shrines[0].max_hp = max_hp;
        if (g->shrines[0].hp > max_hp) {
            g->shrines[0].hp = max_hp;
        }
    }

    g->fluid.cfg.dye_damp[0] = ink_clampf(mods.ink_damp_override, 0.90f, 0.999f);
    g->fluid.cfg.dye_damp[1] = ink_clampf(mods.fire_damp_override, 0.90f, 0.999f);
}

bool roguelike_grant_upgrade(GameState *g, UpgradeId id)
{
    if (g == NULL || (unsigned)id >= (unsigned)UPG_COUNT) {
        return false;
    }
    const uint8_t max_lv = game_upgrade_max_level(id);
    int32_t owned_n = g->run.upgrade_count;
    if (owned_n > GAME_MAX_OWNED_UPGRADES) {
        owned_n = GAME_MAX_OWNED_UPGRADES;
    }
    bool granted = false;
    for (int32_t i = 0; i < owned_n; ++i) {
        if (g->run.upgrades[i].id == id) {
            if (g->run.upgrades[i].level >= max_lv) {
                return false;
            }
            ++g->run.upgrades[i].level;
            granted = true;
            break;
        }
    }
    if (!granted) {
        if (owned_n >= GAME_MAX_OWNED_UPGRADES) {
            return false;
        }
        g->run.upgrades[owned_n] = (OwnedUpgrade){.id = id, .level = 1};
        g->run.upgrade_count = owned_n + 1;
    }
    roguelike_recompute_modifiers(g);
    if (id == UPG_SHRINE_WARD && g->shrine_count > 0 && g->shrines[0].hp < g->shrines[0].max_hp) {
        ++g->shrines[0].hp;
    }
    return true;
}

void upgrades_fire_on_stroke(GameState *g, float px, float py, float x, float y, float vx, float vy,
                             float dist)
{
    if (g == NULL) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_stroke != NULL) {
            const StrokeHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .px = px,
                .py = py,
                .x = x,
                .y = y,
                .vx = vx,
                .vy = vy,
                .dist = dist,
            };
            k_upgrade_defs[id].on_stroke(&args);
        }
    }
    relics_fire_on_stroke(g, px, py, x, y, vx, vy, dist);
}

void upgrades_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                              float *storm_sustain_add)
{
    if (g == NULL) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_cyclone != NULL) {
            const CycloneHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .x = x,
                .y = y,
                .storm_add = storm_add,
                .storm_sustain_add = storm_sustain_add,
            };
            k_upgrade_defs[id].on_cyclone(&args);
        }
    }
    relics_fire_on_cyclone(g, x, y, storm_add, storm_sustain_add);
}

void upgrades_fire_on_gale(GameState *g, float x, float y, float dx, float dy)
{
    if (g == NULL) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_gale != NULL) {
            const GaleHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .x = x,
                .y = y,
                .dx = dx,
                .dy = dy,
            };
            k_upgrade_defs[id].on_gale(&args);
        }
    }
    relics_fire_on_gale(g, x, y, dx, dy);
}

void upgrades_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags)
{
    if (g == NULL || (dmg_flags & DMG_SECONDARY) != 0) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_hit != NULL) {
            const HitHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .target = target,
                .dmg_inout = dmg_inout,
                .dmg_flags = dmg_flags,
            };
            k_upgrade_defs[id].on_hit(&args);
        }
    }
    relics_fire_on_hit(g, target, dmg_inout, dmg_flags);
}

void upgrades_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags)
{
    if (g == NULL) {
        return;
    }
    /* Secondary kills skip damaging hooks so chained blasts cannot recurse without bound. */
    const bool secondary = (dmg_flags & DMG_SECONDARY) != 0;
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_kill != NULL &&
            (!secondary || k_upgrade_defs[id].on_kill_secondary)) {
            const KillHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .victim = victim,
                .x = x,
                .y = y,
                .dmg_flags = dmg_flags,
            };
            k_upgrade_defs[id].on_kill(&args);
        }
    }
    relics_fire_on_kill(g, victim, x, y, dmg_flags);
}

void upgrades_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity)
{
    if (g == NULL) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_reaction != NULL) {
            const ReactionHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .kind = kind,
                .x = x,
                .y = y,
                .intensity = intensity,
            };
            k_upgrade_defs[id].on_reaction(&args);
        }
    }
    relics_fire_on_reaction(g, kind, x, y, intensity);
}

void upgrades_fire_on_tick(GameState *g, float dt)
{
    if (g == NULL) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_tick != NULL) {
            const TickHookArgs args = {
                .g = g,
                .lv = g->run.upgrades[i].level,
                .dt = dt,
            };
            k_upgrade_defs[id].on_tick(&args);
        }
    }
    relics_fire_on_tick(g, dt);
}
