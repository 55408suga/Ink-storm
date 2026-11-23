#ifndef INK_GAME_H
#define INK_GAME_H

#include "ecs.h"
#include "fluid.h"
#include "game_components.h"
#include "game_events.h"
#include "game_ids.h"
#include "game_internal.h"
#include "meta.h"

#include <stdbool.h>
#include <stdint.h>

#define GAME_MAX_SHRINES 4
#define GAME_MAX_EVENTS 128
#define GAME_MAX_REWARD_CHOICES 3
#define GAME_MAX_OWNED_UPGRADES 32
#define GAME_MAX_OWNED_RELICS 16
#define GAME_MAX_PENDING_IMPULSE 8
#define GAME_MAIN_ACT_COUNT 3
#define GAME_BOSS_WAVE_INDEX 3

typedef enum {
    RUN_PHASE_PLAYING = 0,
    RUN_PHASE_WAVE_AFTERMATH,
    RUN_PHASE_REWARD,
    RUN_PHASE_ACT_TRANSITION,
    RUN_PHASE_SHRINE_BREAK,
    RUN_PHASE_VICTORY,
    RUN_PHASE_GAME_OVER
} RunPhase;

typedef struct {
    float x, y;
    float hit_radius_proj;
    float hit_radius_contact;
    int32_t hp, max_hp;
    float invuln_t;
} ShrineState;

typedef struct {
    float ink, ink_max;
    bool dry;
    bool painting;
    float stroke_speed;
    float idle_t;
    float cyclone_cd, cyclone_cd_total;
    float gale_cd, gale_cd_total;
    int32_t gale_charges;
    int32_t gale_max_charges;
    float surge_t;
    float cursor_x, cursor_y;
    float aim_x, aim_y;
} PlayerState;

typedef struct {
    UpgradeId id;
    uint8_t level;
} OwnedUpgrade;

typedef enum {
    REWARD_KIND_NONE = 0,
    REWARD_KIND_UPGRADE,
    REWARD_KIND_RELIC,
    REWARD_KIND_FALLBACK
} RewardKind;

typedef struct {
    RewardKind kind;
    int32_t choice_count;
    int32_t choice_ids[GAME_MAX_REWARD_CHOICES];
    uint8_t choice_next_level[GAME_MAX_REWARD_CHOICES];
    bool repair_available;
    int32_t repair_amount;
    int32_t crystal_reward;
    uint32_t serial;
} RewardOffer;

typedef struct {
    int32_t act_index, act_count;
    int32_t wave_index;
    int32_t wave_kills, wave_quota;
    int32_t acts_cleared;
    int32_t endless_loop;
    int32_t endless_acts_cleared;
    int32_t bonus_crystals;
    int32_t kills;
    int32_t score;
    int32_t chain, max_chain;
    float chain_t;
    int32_t ignitions, storm_zaps, thunderbursts, cataclysms;
    int32_t rerolls_left;
    RewardOffer reward;
    OwnedUpgrade upgrades[GAME_MAX_OWNED_UPGRADES];
    int32_t upgrade_count;
    RelicId relics[GAME_MAX_OWNED_RELICS];
    int32_t relic_count;
    EntityId boss_id;
    bool run_settled;
} RunProgress;

typedef struct {
    float x, y, rad_cells, peak_speed;
} PendingImpulse;

#define GAME_RYU_TRAIL_CAP 128
#define GAME_RYU_ORB_COUNT 3

typedef struct {
    EntityId head_id;
    EntityId orb_ids[GAME_RYU_ORB_COUNT];
    uint8_t orb_broken_mask;
    int32_t active_orb_slot;
    GameVec2 trail[GAME_RYU_TRAIL_CAP];
    int32_t trail_head;
    int32_t trail_count;
} RyuBodyState;

typedef struct GameState GameState;

struct GameState {
    FluidState fluid;
    EcsWorld ecs;
    GameComponents comps;
    RunConfig config;
    uint64_t rng;
    uint64_t ticks;
    RunPhase phase;
    float phase_t;
    float hitstop_t;
    PlayerState player;
    ShrineState shrines[GAME_MAX_SHRINES];
    int32_t shrine_count;
    RunProgress run;
    GameEvent events[GAME_MAX_EVENTS];
    int32_t event_count;
    uint32_t dropped_events;
    PendingImpulse pending_impulses[GAME_MAX_PENDING_IMPULSE];
    int32_t pending_impulse_count;
    RyuBodyState ryu_body;
    GameInternal internal;
};

typedef enum {
    GAME_HASH_CORE = 0,
    GAME_HASH_FLUID,
    GAME_HASH_ECS,
    GAME_HASH_COMPONENTS,
    GAME_HASH_RUN,
    GAME_HASH_EVENTS,
    GAME_HASH_INTERNAL,
    GAME_HASH_SECTION_COUNT
} GameHashSection;

/* The caller owns the returned state and releases it with game_destroy. */
GameState *game_create(const RunConfig *cfg);
void game_destroy(GameState *g);
void game_init(GameState *g, const RunConfig *cfg);
void game_step(GameState *g, const BrushInput *in, const GameCommand *cmd);
void game_hash_sections(const GameState *g, uint64_t out[GAME_HASH_SECTION_COUNT]);
uint64_t game_hash(const GameState *g);
bool game_push_event(GameState *g, GameEvent ev);

MetaRunResult game_run_result(const GameState *g);
bool meta_settle_run(MetaState *meta, GameState *g, MetaSettleBreakdown *out_breakdown);

UpgradeAxis game_upgrade_axis(UpgradeId id);
uint8_t game_upgrade_max_level(UpgradeId id);
uint8_t game_upgrade_level(const GameState *g, UpgradeId id);
bool game_is_recommended_axis(BrushStyleId style, UpgradeAxis axis);
bool game_has_relic(const GameState *g, RelicId id);
bool game_is_boss_enemy(EnemyDefId id);
bool game_is_boss_part(EnemyDefId id);
bool game_enemy_is_targetable(const GameState *g, EntityId id);
int32_t game_relic_unlock_slot(RelicId id);
bool game_is_relic_unlocked(const GameState *g, RelicId id);
MassTier game_effective_mass_tier(const GameState *g, MassTier raw);

EntityId game_spawn_entity(GameState *g, uint64_t comp_mask);
bool game_add_components(GameState *g, EntityId id, uint64_t comp_mask);

const TransformComp *game_transform(const GameState *g, EntityId id);
TransformComp *game_transform_mut(GameState *g, EntityId id);
const VelocityComp *game_velocity(const GameState *g, EntityId id);
VelocityComp *game_velocity_mut(GameState *g, EntityId id);
const HealthComp *game_health(const GameState *g, EntityId id);
HealthComp *game_health_mut(GameState *g, EntityId id);
const StatusComp *game_status(const GameState *g, EntityId id);
StatusComp *game_status_mut(GameState *g, EntityId id);
const EnemyComp *game_enemy(const GameState *g, EntityId id);
EnemyComp *game_enemy_mut(GameState *g, EntityId id);
const BossComp *game_boss(const GameState *g, EntityId id);
BossComp *game_boss_mut(GameState *g, EntityId id);
const ProjComp *game_proj(const GameState *g, EntityId id);
ProjComp *game_proj_mut(GameState *g, EntityId id);
const InkOrbComp *game_orb(const GameState *g, EntityId id);
InkOrbComp *game_orb_mut(GameState *g, EntityId id);
const GimmickComp *game_gimmick(const GameState *g, EntityId id);
GimmickComp *game_gimmick_mut(GameState *g, EntityId id);

EntityId game_debug_spawn_enemy(GameState *g, EnemyDefId def, float x, float y);
EntityId game_debug_spawn_projectile(GameState *g, ProjKind kind, float x, float y, float vx,
                                     float vy);
EntityId game_debug_spawn_orb(GameState *g, float x, float y);
void game_debug_start_act(GameState *g, int32_t act_index);
void game_debug_start_wave(GameState *g, int32_t wave_index);
void game_debug_force_wave_clear(GameState *g);
BrushInput game_autoplay_input(const GameState *g);
GameCommand game_autoplay_command(const GameState *g);

#endif
