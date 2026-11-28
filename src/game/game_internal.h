#ifndef INK_GAME_INTERNAL_H
#define INK_GAME_INTERNAL_H

#include "ecs.h"
#include "fluid.h"

#include <stdbool.h>
#include <stdint.h>

#define GAME_MAX_ACTIVE_CYCLONES 8
/* 反応ブロックは画面ピクセル基準（64px 四方）。格子解像度を変えても 20x12 のまま */
#define GAME_REACTION_BLOCK_PX 64
#define GAME_THUNDERBURST_BLOCKS_X                                                                 \
    ((INK_SCREEN_W + GAME_REACTION_BLOCK_PX - 1) / GAME_REACTION_BLOCK_PX)
#define GAME_THUNDERBURST_BLOCKS_Y                                                                 \
    ((INK_SCREEN_H + GAME_REACTION_BLOCK_PX - 1) / GAME_REACTION_BLOCK_PX)
#define GAME_THUNDERBURST_BLOCKS (GAME_THUNDERBURST_BLOCKS_X * GAME_THUNDERBURST_BLOCKS_Y)
/* セル数の閾値は 8px セル（160x90）で調整済み。セル面積比で換算し 160x90 では厳密に 1.0f */
#define GAME_REF_CELL_PX 8.0f
/* 流体へ渡す半径 [px] と速度 [px/s] を格子単位（セル / セル/s）へ換算する */
#define GAME_PX_TO_CELLS(px) ((px) / FLUID_CELL_PX)
#define GAME_CELL_COUNT_SCALE                                                                      \
    ((GAME_REF_CELL_PX / FLUID_CELL_PX) * (GAME_REF_CELL_PX / FLUID_CELL_PX))

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
    float stroke_ember_rad;
    float stroke_ember_amt;
    float relic_ember_interval;
    float ink_damp_override;
    float fire_damp_override;
    bool twin_brush;
    bool tengu_fan;
    bool thunder_drum;
    bool ink_dyed_shrine;
    bool rosary_swirl;
    bool wind_return;

    /* Phase 5 additions (all defaults match P1 behavior identically) */
    int32_t chain_storm_interval;
    float chain_storm_mul;
    bool surge_stroke_storm;
    int32_t gale_max_charges;
    float cataclysm_thresh;
    float cataclysm_cd_base;
    float surge_reaction_dmg_mul;
    int32_t surge_min_chain;
    bool thunderfire_gem;
    bool oni_tile;
    bool night_parade_scroll;
    int32_t bonus_kill_orbs;
    float spawn_interval_mul;
    int32_t max_alive_add;
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
    float thunderburst_hitstop_cd;
    float cataclysm_cd;
    float barrier_shield_cd;
    bool barrier_shield_ready;
    float wind_return_cd;
    uint32_t triggered_hints_mask;
    float maelstrom_bind_t[ECS_MAX_ENTITIES];
    float maelstrom_hit_cd[ECS_MAX_ENTITIES];

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
    float ryu_coil_angle;
    bool ryu_coil_active;
    float ryu_ward_ping_cd;

    /* Phase 5-A fields */
    float thunderfire_gem_cd;

    /* Lobby test brush */
    bool sandbox_infinite_ink;
} GameInternal;

#endif
