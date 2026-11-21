#ifndef INK_GAME_INTERNAL_H
#define INK_GAME_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_MAX_ACTIVE_CYCLONES 8

typedef struct {
    float brush_radius_mul;
    float brush_force_mul;
    float stroke_cost_mul;
    float gale_impulse_mul;
    float ink_max_add;
    float ink_regen_mul;
    float burn_rate_mul;
    int32_t chain_hops_add;
    float chain_range_mul;
    float cyclone_radius_mul;
    float cyclone_duration_add;
    float cyclone_cd;
    float gale_cd;
    float surge_duration;
} RunModifiers;

typedef struct {
    bool active;
    float x, y;
    float rad_cells;
    float life, max_life;
    float storm_sustain;
} ActiveCyclone;

typedef struct {
    RunModifiers mods;
    ActiveCyclone cyclones[GAME_MAX_ACTIVE_CYCLONES];
    float ignite_callout_cd;
    bool blazing_active;
    float storm_zap_cd;
    float ember_tip_dist;
    float spawn_cd;
    uint32_t seen_enemies_mask;
    bool act_prepared;
    bool first_reward_offered;
    bool wave_clear_is_boss;
    float last_boss_x, last_boss_y;
    float boss_vortex_t;
    float boss_vortex_x, boss_vortex_y;
} GameInternal;

#endif
