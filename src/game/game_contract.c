#include "game.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    UpgradeAxis axis;
    uint8_t max_level;
} UpgradeMeta;

static const UpgradeMeta k_upgrade_meta[UPG_COUNT] = {
    [UPG_EMBER_TIP] = {.axis = AXIS_BLAZE, .max_level = 3},
    [UPG_WILDFIRE] = {.axis = AXIS_BLAZE, .max_level = 3},
    [UPG_CHARGED_SEAL] = {.axis = AXIS_THUNDER, .max_level = 3},
    [UPG_CHAIN_LIGHTNING] = {.axis = AXIS_THUNDER, .max_level = 3},
    [UPG_GREAT_VORTEX] = {.axis = AXIS_GALE, .max_level = 3},
    [UPG_SLAM_SHOCKWAVE] = {.axis = AXIS_GALE, .max_level = 3},
    [UPG_RICH_INK] = {.axis = AXIS_INKWAY, .max_level = 3},
    [UPG_LASTING_SURGE] = {.axis = AXIS_INKWAY, .max_level = 3},
};

bool game_push_event(GameState *g, GameEvent ev)
{
    if (g == NULL) {
        return false;
    }
    if (g->event_count >= 0 && g->event_count < GAME_MAX_EVENTS) {
        g->events[g->event_count++] = ev;
        return true;
    }
    if (g->dropped_events < UINT32_MAX) {
        ++g->dropped_events;
    }
    return false;
}

MetaRunResult game_run_result(const GameState *g)
{
    MetaRunResult res = {0};
    if (g == NULL) {
        return res;
    }
    res.acts_cleared = g->run.acts_cleared;
    res.endless_acts_cleared = 0;
    res.reached_act = g->run.act_index + 1;
    res.kills = g->run.kills;
    res.max_chain = g->run.max_chain;
    res.thunderbursts = g->run.thunderbursts;
    res.cataclysms = g->run.cataclysms;
    res.score = g->run.score;
    res.cleared = g->run.acts_cleared >= GAME_MAIN_ACT_COUNT;
    res.crystal_bonus_x15 = false;
    return res;
}

bool meta_settle_run(MetaState *meta, GameState *g, MetaSettleBreakdown *out_breakdown)
{
    if (g == NULL) {
        if (out_breakdown != NULL) {
            *out_breakdown = (MetaSettleBreakdown){0};
        }
        return false;
    }
    const MetaRunResult res = game_run_result(g);
    return meta_settle_result(meta, &res, &g->run.run_settled, out_breakdown);
}

UpgradeAxis game_upgrade_axis(UpgradeId id)
{
    if ((unsigned)id >= (unsigned)UPG_COUNT) {
        return AXIS_COMMON;
    }
    return k_upgrade_meta[id].axis;
}

uint8_t game_upgrade_max_level(UpgradeId id)
{
    if ((unsigned)id >= (unsigned)UPG_COUNT) {
        return 0;
    }
    return k_upgrade_meta[id].max_level;
}

uint8_t game_upgrade_level(const GameState *g, UpgradeId id)
{
    if (g == NULL || (unsigned)id >= (unsigned)UPG_COUNT) {
        return 0;
    }
    int32_t count = g->run.upgrade_count;
    if (count > GAME_MAX_OWNED_UPGRADES) {
        count = GAME_MAX_OWNED_UPGRADES;
    }
    for (int32_t i = 0; i < count; ++i) {
        if (g->run.upgrades[i].id == id) {
            return g->run.upgrades[i].level;
        }
    }
    return 0;
}

bool game_is_recommended_axis(BrushStyleId style, UpgradeAxis axis)
{
    switch (style) {
    case BRUSH_STYLE_HEAVY:
        return axis == AXIS_GALE || axis == AXIS_INKWAY;
    case BRUSH_STYLE_EMBER:
        return axis == AXIS_BLAZE || axis == AXIS_THUNDER;
    case BRUSH_STYLE_QUILL:
        return axis == AXIS_THUNDER || axis == AXIS_INKWAY;
    default:
        return false;
    }
}

static void zero_component_slots(GameState *g, uint32_t index, uint64_t mask)
{
    if ((mask & COMP_TRANSFORM) != 0) {
        memset(&g->comps.transform[index], 0, sizeof g->comps.transform[index]);
    }
    if ((mask & COMP_VELOCITY) != 0) {
        memset(&g->comps.velocity[index], 0, sizeof g->comps.velocity[index]);
    }
    if ((mask & COMP_HEALTH) != 0) {
        memset(&g->comps.health[index], 0, sizeof g->comps.health[index]);
    }
    if ((mask & COMP_STATUS) != 0) {
        memset(&g->comps.status[index], 0, sizeof g->comps.status[index]);
    }
    if ((mask & COMP_ENEMY) != 0) {
        memset(&g->comps.enemy[index], 0, sizeof g->comps.enemy[index]);
    }
    if ((mask & COMP_BOSS) != 0) {
        memset(&g->comps.boss[index], 0, sizeof g->comps.boss[index]);
    }
    if ((mask & COMP_PROJ) != 0) {
        memset(&g->comps.proj[index], 0, sizeof g->comps.proj[index]);
    }
    if ((mask & COMP_INK_ORB) != 0) {
        memset(&g->comps.orb[index], 0, sizeof g->comps.orb[index]);
    }
    if ((mask & COMP_GIMMICK) != 0) {
        memset(&g->comps.gimmick[index], 0, sizeof g->comps.gimmick[index]);
    }
}

EntityId game_spawn_entity(GameState *g, uint64_t comp_mask)
{
    const EntityId invalid = {ECS_INVALID_INDEX, 0};
    if (g == NULL || comp_mask == 0 || (comp_mask & ~COMP_ALL_MASK) != 0) {
        return invalid;
    }
    const EntityId id = ecs_spawn(&g->ecs);
    if (id.index == ECS_INVALID_INDEX) {
        return invalid;
    }
    zero_component_slots(g, id.index, comp_mask);
    if (!ecs_add_mask(&g->ecs, id, comp_mask)) {
        ecs_destroy(&g->ecs, id);
        return invalid;
    }
    return id;
}

bool game_add_components(GameState *g, EntityId id, uint64_t comp_mask)
{
    if (g == NULL || comp_mask == 0 || (comp_mask & ~COMP_ALL_MASK) != 0 ||
        !ecs_is_alive(&g->ecs, id)) {
        return false;
    }
    const uint64_t newly_added = comp_mask & ~g->ecs.comp_mask[id.index];
    zero_component_slots(g, id.index, newly_added);
    return ecs_add_mask(&g->ecs, id, comp_mask);
}

const TransformComp *game_transform(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_TRANSFORM) ? &g->comps.transform[id.index]
                                                                  : NULL;
}

TransformComp *game_transform_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_TRANSFORM) ? &g->comps.transform[id.index]
                                                                  : NULL;
}

const VelocityComp *game_velocity(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_VELOCITY) ? &g->comps.velocity[id.index]
                                                                 : NULL;
}

VelocityComp *game_velocity_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_VELOCITY) ? &g->comps.velocity[id.index]
                                                                 : NULL;
}

const HealthComp *game_health(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_HEALTH) ? &g->comps.health[id.index] : NULL;
}

HealthComp *game_health_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_HEALTH) ? &g->comps.health[id.index] : NULL;
}

const StatusComp *game_status(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_STATUS) ? &g->comps.status[id.index] : NULL;
}

StatusComp *game_status_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_STATUS) ? &g->comps.status[id.index] : NULL;
}

const EnemyComp *game_enemy(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_ENEMY) ? &g->comps.enemy[id.index] : NULL;
}

EnemyComp *game_enemy_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_ENEMY) ? &g->comps.enemy[id.index] : NULL;
}

const BossComp *game_boss(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_BOSS) ? &g->comps.boss[id.index] : NULL;
}

BossComp *game_boss_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_BOSS) ? &g->comps.boss[id.index] : NULL;
}

const ProjComp *game_proj(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_PROJ) ? &g->comps.proj[id.index] : NULL;
}

ProjComp *game_proj_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_PROJ) ? &g->comps.proj[id.index] : NULL;
}

const InkOrbComp *game_orb(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_INK_ORB) ? &g->comps.orb[id.index] : NULL;
}

InkOrbComp *game_orb_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_INK_ORB) ? &g->comps.orb[id.index] : NULL;
}

const GimmickComp *game_gimmick(const GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_GIMMICK) ? &g->comps.gimmick[id.index]
                                                                : NULL;
}

GimmickComp *game_gimmick_mut(GameState *g, EntityId id)
{
    return g != NULL && ecs_has_mask(&g->ecs, id, COMP_GIMMICK) ? &g->comps.gimmick[id.index]
                                                                : NULL;
}
