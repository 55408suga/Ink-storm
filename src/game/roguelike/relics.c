#include "roguelike/relics.h"

#include "combat/damage.h"
#include "core.h"
#include "player/player_skills.h"

#include <math.h>
#include <stddef.h>

typedef struct {
    RelicId id;
    void (*apply_mods_add)(RunModifiers *mods);
    void (*apply_mods_mul)(RunModifiers *mods);
    void (*on_stroke)(const StrokeHookArgs *a);
    void (*on_cyclone)(const CycloneHookArgs *a);
    void (*on_gale)(const GaleHookArgs *a);
    void (*on_hit)(const HitHookArgs *a);
    void (*on_kill)(const KillHookArgs *a);
    void (*on_reaction)(const ReactionHookArgs *a);
    void (*on_tick)(const TickHookArgs *a);
} RelicDef;

static void vermilion_inkstone_apply_add(RunModifiers *mods)
{
    mods->relic_ember_interval = 200.0f;
    mods->ink_damp_override -= 0.005f;
}

static void vermilion_inkstone_on_stroke(const StrokeHookArgs *a)
{
    GameState *g = a->g;
    const float interval = g->internal.mods.relic_ember_interval;
    if (g->player.dry || interval <= 0.0f) {
        return;
    }
    static const float k_ember_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.5f, 0.0f, 0.0f};
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;

    g->internal.relic_ember_dist += a->dist;
    int32_t bursts = 0;
    while (g->internal.relic_ember_dist >= interval && bursts < 8) {
        g->internal.relic_ember_dist -= interval;
        fluid_splat(&g->fluid, a->x, a->y, 2.5f, 0.0f, 0.0f, k_ember_dye);
        if (g->internal.mods.twin_brush) {
            fluid_splat(&g->fluid, 2.0f * sx - a->x, 2.0f * sy - a->y, 2.5f, 0.0f, 0.0f,
                        k_ember_dye);
        }
        ++bursts;
    }
    if (g->internal.relic_ember_dist >= interval) {
        g->internal.relic_ember_dist = fmodf(g->internal.relic_ember_dist, interval);
    }
}

static void thunder_drum_apply_add(RunModifiers *mods) { mods->thunder_drum = true; }

static void thunder_drum_on_cyclone(const CycloneHookArgs *a)
{
    GameState *g = a->g;
    static const float k_drum_dye[FLUID_MAX_CHANNELS] = {0.0f, 0.0f, 1.0f, 0.0f};
    fluid_splat(&g->fluid, a->x, a->y, 4.0f, 0.0f, 0.0f, k_drum_dye);

    (void)game_push_event(g, (GameEvent){
                                 .type = EV_STORM_ZAP,
                                 .x = a->x,
                                 .y = fmaxf(20.0f, a->y - 120.0f),
                                 .x2 = a->x,
                                 .y2 = a->y,
                                 .value = 0,
                                 .flags = 0,
                             });

    const float rad2 = 72.0f * 72.0f;
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
        const float dx = tr->x - a->x;
        const float dy = tr->y - a->y;
        if (dx * dx + dy * dy <= rad2) {
            (void)combat_apply_damage(g, id, 40, DMG_STORM | DMG_SECONDARY, 0.0f, 0.0f);
        }
    }
}

static void tengu_fan_apply_add(RunModifiers *mods)
{
    mods->tengu_fan = true;
    mods->gale_cost_add += 6.0f;
}

static void ink_dyed_shrine_apply_add(RunModifiers *mods) { mods->ink_dyed_shrine = true; }

static void ink_dyed_shrine_on_tick(const TickHookArgs *a)
{
    GameState *g = a->g;
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    static const float k_shrine_ink[FLUID_MAX_CHANNELS] = {0.04f, 0.0f, 0.0f, 0.0f};
    fluid_splat(&g->fluid, sx, sy, 6.0f, 0.0f, 0.0f, k_shrine_ink);
}

static void twin_brush_apply_add(RunModifiers *mods) { mods->twin_brush = true; }

static void twin_brush_apply_mul(RunModifiers *mods) { mods->stroke_cost_mul *= 1.30f; }

static void rosary_swirl_apply_add(RunModifiers *mods) { mods->rosary_swirl = true; }

static void pact_of_fire_apply_add(RunModifiers *mods) { mods->shrine_max_hp_delta -= 2; }

static void pact_of_fire_apply_mul(RunModifiers *mods) { mods->fire_dmg_mul *= 2.0f; }

static void lingering_ink_apply_add(RunModifiers *mods)
{
    mods->ink_damp_override += 0.008f;
    mods->ink_mark_duration += 2.0f;
}

static void wind_return_apply_add(RunModifiers *mods) { mods->wind_return = true; }

static void wind_return_on_tick(const TickHookArgs *a)
{
    GameState *g = a->g;
    if (g->internal.wind_return_cd > 0.0f) {
        g->internal.wind_return_cd -= a->dt;
        if (g->internal.wind_return_cd <= 1e-4f) {
            g->internal.wind_return_cd = 0.0f;
        }
    }
    if (g->internal.wind_return_cd > 0.0f) {
        return;
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    const float trig_rad2 = 54.0f * 54.0f;
    const uint32_t hw = g->ecs.high_water;

    bool found = false;
    float best_d2 = trig_rad2 + 1.0f;
    float best_x = 0.0f;
    float best_y = 0.0f;

    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        const TransformComp *tr = game_transform(g, id);
        const ProjComp *proj = game_proj(g, id);
        if (tr == NULL || proj == NULL || tr->dissolving || proj->deflected) {
            continue;
        }
        const float dx = tr->x - sx;
        const float dy = tr->y - sy;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= trig_rad2 && d2 < best_d2) {
            best_d2 = d2;
            best_x = tr->x;
            best_y = tr->y;
            found = true;
        }
    }

    if (found) {
        player_skills_cast_free_gale(g, sx, sy, best_x - sx, best_y - sy);
        g->internal.wind_return_cd = 12.0f;
    }
}

static const RelicDef k_relic_defs[RELIC_COUNT] = {
    [RELIC_VERMILION_INKSTONE] =
        {
            .id = RELIC_VERMILION_INKSTONE,
            .apply_mods_add = vermilion_inkstone_apply_add,
            .on_stroke = vermilion_inkstone_on_stroke,
        },
    [RELIC_THUNDER_DRUM] =
        {
            .id = RELIC_THUNDER_DRUM,
            .apply_mods_add = thunder_drum_apply_add,
            .on_cyclone = thunder_drum_on_cyclone,
        },
    [RELIC_TENGU_FAN] =
        {
            .id = RELIC_TENGU_FAN,
            .apply_mods_add = tengu_fan_apply_add,
        },
    [RELIC_INK_DYED_SHRINE] =
        {
            .id = RELIC_INK_DYED_SHRINE,
            .apply_mods_add = ink_dyed_shrine_apply_add,
            .on_tick = ink_dyed_shrine_on_tick,
        },
    [RELIC_TWIN_BRUSH] =
        {
            .id = RELIC_TWIN_BRUSH,
            .apply_mods_add = twin_brush_apply_add,
            .apply_mods_mul = twin_brush_apply_mul,
        },
    [RELIC_ROSARY_SWIRL] =
        {
            .id = RELIC_ROSARY_SWIRL,
            .apply_mods_add = rosary_swirl_apply_add,
        },
    [RELIC_PACT_OF_FIRE] =
        {
            .id = RELIC_PACT_OF_FIRE,
            .apply_mods_add = pact_of_fire_apply_add,
            .apply_mods_mul = pact_of_fire_apply_mul,
        },
    [RELIC_LINGERING_INK] =
        {
            .id = RELIC_LINGERING_INK,
            .apply_mods_add = lingering_ink_apply_add,
        },
    [RELIC_WIND_RETURN] =
        {
            .id = RELIC_WIND_RETURN,
            .apply_mods_add = wind_return_apply_add,
            .on_tick = wind_return_on_tick,
        },
};

static int32_t clamped_relic_count(const GameState *g)
{
    if (g == NULL || g->run.relic_count <= 0) {
        return 0;
    }
    return g->run.relic_count <= GAME_MAX_OWNED_RELICS ? g->run.relic_count : GAME_MAX_OWNED_RELICS;
}

bool roguelike_grant_relic(GameState *g, RelicId id)
{
    if (g == NULL || (unsigned)id >= (unsigned)RELIC_COUNT || game_has_relic(g, id) ||
        g->run.relic_count < 0 || g->run.relic_count >= GAME_MAX_OWNED_RELICS) {
        return false;
    }
    g->run.relics[g->run.relic_count++] = id;
    roguelike_recompute_modifiers(g);
    return true;
}

void relics_apply_modifiers_add(const GameState *g, RunModifiers *mods)
{
    if (g == NULL || mods == NULL) {
        return;
    }
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].apply_mods_add != NULL) {
            k_relic_defs[id].apply_mods_add(mods);
        }
    }
}

void relics_apply_modifiers_mul(const GameState *g, RunModifiers *mods)
{
    if (g == NULL || mods == NULL) {
        return;
    }
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].apply_mods_mul != NULL) {
            k_relic_defs[id].apply_mods_mul(mods);
        }
    }
}

void relics_fire_on_stroke(GameState *g, float x, float y, float vx, float vy, float dist)
{
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_stroke != NULL) {
            const StrokeHookArgs args = {
                .g = g,
                .lv = 1,
                .x = x,
                .y = y,
                .vx = vx,
                .vy = vy,
                .dist = dist,
            };
            k_relic_defs[id].on_stroke(&args);
        }
    }
}

void relics_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                            float *storm_sustain_add)
{
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_cyclone != NULL) {
            const CycloneHookArgs args = {
                .g = g,
                .lv = 1,
                .x = x,
                .y = y,
                .storm_add = storm_add,
                .storm_sustain_add = storm_sustain_add,
            };
            k_relic_defs[id].on_cyclone(&args);
        }
    }
}

void relics_fire_on_gale(GameState *g, float x, float y, float dx, float dy)
{
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_gale != NULL) {
            const GaleHookArgs args = {
                .g = g,
                .lv = 1,
                .x = x,
                .y = y,
                .dx = dx,
                .dy = dy,
            };
            k_relic_defs[id].on_gale(&args);
        }
    }
}

void relics_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags)
{
    if ((dmg_flags & DMG_SECONDARY) != 0) {
        return;
    }
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_hit != NULL) {
            const HitHookArgs args = {
                .g = g,
                .lv = 1,
                .target = target,
                .dmg_inout = dmg_inout,
                .dmg_flags = dmg_flags,
            };
            k_relic_defs[id].on_hit(&args);
        }
    }
}

void relics_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags)
{
    if ((dmg_flags & DMG_SECONDARY) != 0) {
        return;
    }
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_kill != NULL) {
            const KillHookArgs args = {
                .g = g,
                .lv = 1,
                .victim = victim,
                .x = x,
                .y = y,
                .dmg_flags = dmg_flags,
            };
            k_relic_defs[id].on_kill(&args);
        }
    }
}

void relics_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity)
{
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_reaction != NULL) {
            const ReactionHookArgs args = {
                .g = g,
                .lv = 1,
                .kind = kind,
                .x = x,
                .y = y,
                .intensity = intensity,
            };
            k_relic_defs[id].on_reaction(&args);
        }
    }
}

void relics_fire_on_tick(GameState *g, float dt)
{
    const int32_t count = clamped_relic_count(g);
    for (int32_t i = 0; i < count; ++i) {
        const RelicId id = g->run.relics[i];
        if ((unsigned)id < (unsigned)RELIC_COUNT && k_relic_defs[id].on_tick != NULL) {
            const TickHookArgs args = {
                .g = g,
                .lv = 1,
                .dt = dt,
            };
            k_relic_defs[id].on_tick(&args);
        }
    }
}
