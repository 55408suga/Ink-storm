#ifndef INK_GAME_UPGRADES_H
#define INK_GAME_UPGRADES_H

#include "game.h"

#include <stdint.h>

typedef struct {
    GameState *g;
    uint8_t lv;
    float px, py;
    float x, y;
    float vx, vy;
    float dist;
} StrokeHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    float x, y;
    float *storm_add;
    float *storm_sustain_add;
} CycloneHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    float x, y;
    float dx, dy;
} GaleHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    EntityId target;
    float *dmg_inout;
    uint32_t dmg_flags;
} HitHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    EntityId victim;
    float x, y;
    uint32_t dmg_flags;
} KillHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    ReactionKind kind;
    float x, y;
    float intensity;
} ReactionHookArgs;

typedef struct {
    GameState *g;
    uint8_t lv;
    float dt;
} TickHookArgs;

typedef struct {
    UpgradeId id;
    UpgradeAxis axis;
    uint8_t max_level;
    bool on_kill_secondary;
    void (*apply_mods_add)(RunModifiers *mods, uint8_t lv);
    void (*apply_mods_mul)(RunModifiers *mods, uint8_t lv);
    void (*on_stroke)(const StrokeHookArgs *a);
    void (*on_cyclone)(const CycloneHookArgs *a);
    void (*on_gale)(const GaleHookArgs *a);
    void (*on_hit)(const HitHookArgs *a);
    void (*on_kill)(const KillHookArgs *a);
    void (*on_reaction)(const ReactionHookArgs *a);
    void (*on_tick)(const TickHookArgs *a);
} UpgradeDef;

void roguelike_recompute_modifiers(GameState *g);
bool roguelike_grant_upgrade(GameState *g, UpgradeId id);

void upgrades_fire_on_stroke(GameState *g, float px, float py, float x, float y, float vx, float vy,
                             float dist);
void upgrades_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                              float *storm_sustain_add);
void upgrades_fire_on_gale(GameState *g, float x, float y, float dx, float dy);
void upgrades_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags);
void upgrades_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags);
void upgrades_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity);
void upgrades_fire_on_tick(GameState *g, float dt);

#endif
