#ifndef INK_GAME_COMPONENTS_H
#define INK_GAME_COMPONENTS_H

#include "ecs.h"
#include "game_ids.h"

#include <stdbool.h>
#include <stdint.h>

#define PROJ_TRAIL_LEN 20
#define PROJ_MAX_HIT_IDS 6

#define COMP_TRANSFORM (UINT64_C(1) << 0)
#define COMP_VELOCITY (UINT64_C(1) << 1)
#define COMP_HEALTH (UINT64_C(1) << 2)
#define COMP_STATUS (UINT64_C(1) << 3)
#define COMP_ENEMY (UINT64_C(1) << 4)
#define COMP_BOSS (UINT64_C(1) << 5)
#define COMP_PROJ (UINT64_C(1) << 6)
#define COMP_INK_ORB (UINT64_C(1) << 7)
#define COMP_GIMMICK (UINT64_C(1) << 8)
#define COMP_ALL_MASK                                                                              \
    (COMP_TRANSFORM | COMP_VELOCITY | COMP_HEALTH | COMP_STATUS | COMP_ENEMY | COMP_BOSS |         \
     COMP_PROJ | COMP_INK_ORB | COMP_GIMMICK)

typedef struct {
    float x, y, radius, facing;
    bool dissolving;
    float dissolve_t;
} TransformComp;

typedef struct {
    float vx, vy;
    MassTier mass_tier;
    float drag, fluid_coupling;
} VelocityComp;

typedef struct {
    int32_t hp, max_hp;
    float flash_t, invuln_t;
} HealthComp;

typedef struct {
    float ink_mark_t, burn_acc, charge_t, knockback_t, stun_t, barrier_t;
} StatusComp;

typedef struct {
    EnemyDefId def_id;
    bool is_elite;
    uint32_t elite_flags;
    int32_t phase;
    float fire_cd;
    int32_t ai_state;
    float absorbed_ink;
    float windup;
} EnemyComp;

typedef struct {
    int32_t pattern;
    float pattern_timer;
    int32_t phase_idx;
    float gale_dx, gale_dy, gale_t;
    int32_t part_id;
    EntityId parent_id;
    int32_t telegraph_pattern;
    float telegraph;
} BossComp;

typedef struct {
    ProjKind kind;
    bool deflected, burning, electrified, accelerated;
    bool fluid_pushed;
    int32_t pierce, bounces_left;
    float age;
    GameVec2 trail[PROJ_TRAIL_LEN];
    int32_t trail_n;
    EntityId hit_ids[PROJ_MAX_HIT_IDS];
    int32_t hit_n;
} ProjComp;

typedef struct {
    int32_t value;
    float age;
} InkOrbComp;

typedef struct {
    int32_t kind, elem_kind;
    float timer, param_a, param_b;
} GimmickComp;

typedef struct {
    TransformComp transform[ECS_MAX_ENTITIES];
    VelocityComp velocity[ECS_MAX_ENTITIES];
    HealthComp health[ECS_MAX_ENTITIES];
    StatusComp status[ECS_MAX_ENTITIES];
    EnemyComp enemy[ECS_MAX_ENTITIES];
    BossComp boss[ECS_MAX_ENTITIES];
    ProjComp proj[ECS_MAX_ENTITIES];
    InkOrbComp orb[ECS_MAX_ENTITIES];
    GimmickComp gimmick[ECS_MAX_ENTITIES];
} GameComponents;

#endif
