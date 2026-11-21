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

static void ember_tip_on_stroke(const StrokeHookArgs *a)
{
    GameState *g = a->g;
    if (g->player.dry || a->lv == 0 || a->lv > 3) {
        return;
    }
    static const float k_intervals[3] = {160.0f, 120.0f, 80.0f};
    static const float k_mults[3] = {1.0f, 1.0f, 1.5f};
    const float interval = k_intervals[a->lv - 1];
    const float fire_amt = 0.5f * k_mults[a->lv - 1];
    const float dye[FLUID_MAX_CHANNELS] = {0.0f, fire_amt, 0.0f, 0.0f};

    g->internal.ember_tip_dist += a->dist;
    int32_t bursts = 0;
    while (g->internal.ember_tip_dist >= interval && bursts < 8) {
        g->internal.ember_tip_dist -= interval;
        fluid_splat(&g->fluid, a->x, a->y, 2.5f, 0.0f, 0.0f, dye);
        ++bursts;
    }
    if (g->internal.ember_tip_dist >= interval) {
        g->internal.ember_tip_dist = fmodf(g->internal.ember_tip_dist, interval);
    }
}

static void wildfire_apply_mul(RunModifiers *mods, uint8_t lv)
{
    static const float k_muls[3] = {1.25f, 1.50f, 1.80f};
    if (lv >= 1 && lv <= 3) {
        mods->burn_rate_mul *= k_muls[lv - 1];
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
        *a->storm_sustain_add += k_storm[2];
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

static void rich_ink_on_kill(const KillHookArgs *a)
{
    GameState *g = a->g;
    if (a->lv < 1 || a->lv > 3 || (a->dmg_flags & DMG_SECONDARY) != 0) {
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

static const UpgradeDef k_upgrade_defs[UPG_COUNT] = {
    [UPG_EMBER_TIP] =
        {
            .id = UPG_EMBER_TIP,
            .axis = AXIS_BLAZE,
            .max_level = 3,
            .on_stroke = ember_tip_on_stroke,
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
        },
    [UPG_LASTING_SURGE] =
        {
            .id = UPG_LASTING_SURGE,
            .axis = AXIS_INKWAY,
            .max_level = 3,
            .apply_mods_add = lasting_surge_apply_add,
        },
    [UPG_FLAME_GALE] = {.id = UPG_FLAME_GALE, .axis = AXIS_BLAZE, .max_level = 3},
    [UPG_INFERNO] = {.id = UPG_INFERNO, .axis = AXIS_BLAZE, .max_level = 3},
    [UPG_PHOENIX_ARROW] = {.id = UPG_PHOENIX_ARROW, .axis = AXIS_BLAZE, .max_level = 3},
    [UPG_THUNDER_ARROW] = {.id = UPG_THUNDER_ARROW, .axis = AXIS_THUNDER, .max_level = 3},
    [UPG_SWIFT_THUNDER] = {.id = UPG_SWIFT_THUNDER, .axis = AXIS_THUNDER, .max_level = 3},
    [UPG_THUNDERBURST_FUSE] = {.id = UPG_THUNDERBURST_FUSE, .axis = AXIS_THUNDER, .max_level = 3},
    [UPG_MIGHTY_STROKE] = {.id = UPG_MIGHTY_STROKE, .axis = AXIS_GALE, .max_level = 3},
    [UPG_MAELSTROM_BIND] = {.id = UPG_MAELSTROM_BIND, .axis = AXIS_GALE, .max_level = 3},
    [UPG_TAILWIND] = {.id = UPG_TAILWIND, .axis = AXIS_GALE, .max_level = 3},
    [UPG_INK_MAGNET] = {.id = UPG_INK_MAGNET, .axis = AXIS_INKWAY, .max_level = 3},
    [UPG_CHAIN_PRIMER] = {.id = UPG_CHAIN_PRIMER, .axis = AXIS_INKWAY, .max_level = 3},
    [UPG_DRY_MASTERY] = {.id = UPG_DRY_MASTERY, .axis = AXIS_INKWAY, .max_level = 3},
    [UPG_SHRINE_WARD] = {.id = UPG_SHRINE_WARD, .axis = AXIS_COMMON, .max_level = 3},
    [UPG_DEEP_INK] = {.id = UPG_DEEP_INK, .axis = AXIS_COMMON, .max_level = 3},
    [UPG_DEEP_MARK] = {.id = UPG_DEEP_MARK, .axis = AXIS_COMMON, .max_level = 3},
    [UPG_BARRIER] = {.id = UPG_BARRIER, .axis = AXIS_COMMON, .max_level = 3},
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
        .ink_damp_override = 0.985f,
        .fire_damp_override = 0.935f,
    };

    int32_t owned_n = g->run.upgrade_count;
    if (owned_n > GAME_MAX_OWNED_UPGRADES) {
        owned_n = GAME_MAX_OWNED_UPGRADES;
    }

    /* Pass 1: all additions (meta -> owned upgrades -> owned relics). */
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
    mods.brush_radius_mul *= 1.25f;
    mods.brush_force_mul *= 1.20f;
    mods.stroke_cost_mul *= 1.15f;
    mods.gale_impulse_mul *= 1.25f;

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
    g->player.ink_max = 100.0f + mods.ink_max_add;
    if (g->player.ink_max < 1.0f) {
        g->player.ink_max = 1.0f;
    }
    if (g->player.ink > g->player.ink_max) {
        g->player.ink = g->player.ink_max;
    }
    g->player.cyclone_cd_total = mods.cyclone_cd;
    g->player.gale_cd_total = mods.gale_cd;

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
    for (int32_t i = 0; i < owned_n; ++i) {
        if (g->run.upgrades[i].id == id) {
            if (g->run.upgrades[i].level >= max_lv) {
                return false;
            }
            ++g->run.upgrades[i].level;
            roguelike_recompute_modifiers(g);
            return true;
        }
    }
    if (owned_n >= GAME_MAX_OWNED_UPGRADES) {
        return false;
    }
    g->run.upgrades[owned_n] = (OwnedUpgrade){.id = id, .level = 1};
    g->run.upgrade_count = owned_n + 1;
    roguelike_recompute_modifiers(g);
    return true;
}

void upgrades_fire_on_stroke(GameState *g, float x, float y, float vx, float vy, float dist)
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
                .x = x,
                .y = y,
                .vx = vx,
                .vy = vy,
                .dist = dist,
            };
            k_upgrade_defs[id].on_stroke(&args);
        }
    }
    relics_fire_on_stroke(g, x, y, vx, vy, dist);
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
    if (g == NULL || (dmg_flags & DMG_SECONDARY) != 0) {
        return;
    }
    const int32_t count = g->run.upgrade_count <= GAME_MAX_OWNED_UPGRADES ? g->run.upgrade_count
                                                                          : GAME_MAX_OWNED_UPGRADES;
    for (int32_t i = 0; i < count; ++i) {
        const UpgradeId id = g->run.upgrades[i].id;
        if ((unsigned)id < (unsigned)UPG_COUNT && k_upgrade_defs[id].on_kill != NULL) {
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
