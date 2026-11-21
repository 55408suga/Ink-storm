#ifndef INK_GAME_INTERNAL_H
#define INK_GAME_INTERNAL_H

#include "ecs.h"

#include <stdbool.h>
#include <stdint.h>

#define GAME_MAX_ACTIVE_CYCLONES 8
#define GAME_THUNDERBURST_BLOCKS_X 20
#define GAME_THUNDERBURST_BLOCKS_Y 12
#define GAME_THUNDERBURST_BLOCKS (GAME_THUNDERBURST_BLOCKS_X * GAME_THUNDERBURST_BLOCKS_Y)

typedef struct {
    float brush_radius_mul;
    float brush_force_mul;
    float stroke_cost_mul;
    float gale_impulse_mul;
    float gale_cost_add;
    float gale_fire_add;
    float gale_end_blast_dmg;
    float cyclone_storm_add;
    float cyclone_storm_sustain;
    float ink_max_add;
    float ink_regen_mul;
    bool ink_regen_no_delay;
    float burn_rate_mul;
    float burn_base_dmg;
    bool inferno_ember_on_kill;
    float fire_dmg_mul;
    int32_t chain_hops_add;
    float chain_range_mul;
    float cyclone_radius_mul;
    float cyclone_duration_add;
    int32_t maelstrom_bind_level;
    float cyclone_cd;
    float gale_cd;
    float surge_duration;
    float chain_grace;
    float orb_pull_radius;
    bool cyclone_pulls_all_orbs;
    float dry_force_override;
    bool dry_zero_cost;
    float dry_deflect_dmg_mul;
    int32_t burning_proj_pierce;
    bool burning_proj_hit_splat;
    int32_t thunder_proj_targets;
    bool deflect_auto_electrify;
    float storm_zap_cd;
    float thunderburst_thresh;
    float thunderburst_radius_mul;
    float homing_steer_mul;
    float deflected_max_speed_mul;
    float ink_mark_mul;
    float ink_mark_duration;
    int32_t shrine_max_hp_delta;
    float shrine_invuln_add;
    bool barrier_shield_enabled;
    float stroke_ember_interval;
    float stroke_ember_mul;
    float relic_ember_interval;
    float ink_damp_override;
    float fire_damp_override;
    bool twin_brush;
    bool tengu_fan;
    bool thunder_drum;
    bool ink_dyed_shrine;
    bool rosary_swirl;
    bool wind_return;
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

    /* Phase 4-A fields */
    float relic_ember_dist;
    float thunderburst_block_cd[GAME_THUNDERBURST_BLOCKS];
    float cataclysm_cd;
    float barrier_shield_cd;
    bool barrier_shield_ready;
    float wind_return_cd;
    uint32_t triggered_hints_mask;
    float maelstrom_bind_t[ECS_MAX_ENTITIES];

    /* Phase 4-B fields */
    float wind_timer;
    float wind_telegraph_t;
    float wind_active_t;
    int32_t wind_dir;
    int32_t wave_elites_spawned;
    uint32_t last_elite_flags;
    float nue_trail_x[2][2];
    float nue_trail_y[2][2];
    float nue_trail_t[2];
    float ryu_coil_t;
} GameInternal;

#endif
