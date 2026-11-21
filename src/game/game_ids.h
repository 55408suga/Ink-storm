#ifndef INK_GAME_IDS_H
#define INK_GAME_IDS_H

#include <stdint.h>

typedef struct {
    float x, y;
} GameVec2;

typedef enum {
    ENEMY_ARCHER = 0,
    ENEMY_LANTERN_FIRE,
    ENEMY_UMBRELLA,
    ENEMY_BOSS_TENGU,
    ENEMY_DEF_COUNT
} EnemyDefId;

typedef enum { MASS_TURRET = 0, MASS_LIGHT, MASS_MEDIUM, MASS_HEAVY, MASS_BOSS } MassTier;

typedef enum { PROJ_ARROW = 0, PROJ_FIREBALL, PROJ_STORM_ORB, PROJ_KIND_COUNT } ProjKind;

typedef enum {
    UPG_EMBER_TIP = 0,
    UPG_WILDFIRE,
    UPG_CHARGED_SEAL,
    UPG_CHAIN_LIGHTNING,
    UPG_GREAT_VORTEX,
    UPG_SLAM_SHOCKWAVE,
    UPG_RICH_INK,
    UPG_LASTING_SURGE,
    UPG_COUNT
} UpgradeId;

typedef enum {
    AXIS_BLAZE = 0,
    AXIS_THUNDER,
    AXIS_GALE,
    AXIS_INKWAY,
    AXIS_COMMON,
    AXIS_COUNT
} UpgradeAxis;

typedef enum {
    REACT_BLAZING = 0,
    REACT_TEMPEST,
    REACT_THUNDERBURST,
    REACT_CATACLYSM,
    REACT_COUNT
} ReactionKind;

typedef enum {
    HINT_STROKE = 0,
    HINT_CYCLONE_FIREBALL,
    HINT_GALE_UMBRELLA,
    HINT_INK_REGEN,
    HINT_REACT_BLAZING,
    HINT_REACT_TEMPEST,
    HINT_REACT_THUNDERBURST,
    HINT_REACT_CATACLYSM,
    HINT_COUNT
} HintId;

typedef enum { SKILL_CYCLONE = 0, SKILL_GALE, SKILL_COUNT } SkillId;

#define DMG_PHYSICAL (UINT32_C(1) << 0)
#define DMG_FIRE (UINT32_C(1) << 1)
#define DMG_STORM (UINT32_C(1) << 2)
#define DMG_SLAM (UINT32_C(1) << 3)
#define DMG_DEFLECTED (UINT32_C(1) << 4)
#define DMG_SECONDARY (UINT32_C(1) << 5)

#endif
