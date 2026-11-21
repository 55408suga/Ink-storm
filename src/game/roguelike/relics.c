#include "roguelike/relics.h"

#include <stddef.h>

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
    (void)g;
    (void)mods;
}

void relics_apply_modifiers_mul(const GameState *g, RunModifiers *mods)
{
    (void)g;
    (void)mods;
}

void relics_fire_on_stroke(GameState *g, float x, float y, float vx, float vy, float dist)
{
    (void)g;
    (void)x;
    (void)y;
    (void)vx;
    (void)vy;
    (void)dist;
}

void relics_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                            float *storm_sustain_add)
{
    (void)g;
    (void)x;
    (void)y;
    (void)storm_add;
    (void)storm_sustain_add;
}

void relics_fire_on_gale(GameState *g, float x, float y, float dx, float dy)
{
    (void)g;
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
}

void relics_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags)
{
    (void)g;
    (void)target;
    (void)dmg_inout;
    (void)dmg_flags;
}

void relics_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags)
{
    (void)g;
    (void)victim;
    (void)x;
    (void)y;
    (void)dmg_flags;
}

void relics_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity)
{
    (void)g;
    (void)kind;
    (void)x;
    (void)y;
    (void)intensity;
}

void relics_fire_on_tick(GameState *g, float dt)
{
    (void)g;
    (void)dt;
}
