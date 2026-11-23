#ifndef INK_GAME_EVENTS_H
#define INK_GAME_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float mx, my, pmx, pmy;
    bool paint, cyclone, gale;
} BrushInput;

typedef enum {
    CMD_NONE = 0,
    CMD_PICK_REWARD,
    CMD_REROLL_REWARD,
    CMD_REPAIR_SHRINE,
    CMD_PICK_ENDLESS_CRYSTAL,
    CMD_START_ENDLESS,
    CMD_ABORT_RUN
} GameCommandType;

typedef struct {
    GameCommandType type;
    int32_t arg;
} GameCommand;

typedef enum {
    EV_NONE = 0,
    EV_KILL,
    EV_DEFLECT,
    EV_SLAM,
    EV_IGNITE,
    EV_STORM_ZAP,
    EV_THUNDERBURST,
    EV_CATACLYSM,
    EV_CYCLONE,
    EV_GALE,
    EV_ORB_PICKUP,
    EV_SHRINE_HIT,
    EV_SURGE_START,
    EV_BOSS_ENTER,
    EV_BOSS_PHASE,
    EV_WAVE_CLEAR,
    EV_ACT_CLEAR,
    EV_REWARD_PICK,
    EV_HINT_TRIGGER,
    EV_WIND_TELEGRAPH,
    EV_GAME_OVER,
    EV_VICTORY,
    EV_ENEMY_INTRO,
    EV_REWARD_REROLL,
    EV_SKILL_DENIED,
    EV_BOSS_PART_BREAK,
    EV_TYPE_COUNT
} GameEventType;

#define EVF_SLAM_WALL (UINT32_C(1) << 8)
#define EVF_SLAM_ENEMY (UINT32_C(1) << 9)
#define EVF_ORB_AUTO (UINT32_C(1) << 10)
#define EVF_RUN_ABORTED (UINT32_C(1) << 11)
#define EVF_REWARD_REPAIR (UINT32_C(1) << 12)
#define EVF_REWARD_RELIC (UINT32_C(1) << 13)
#define EVF_REWARD_CRYSTAL (UINT32_C(1) << 14)

typedef struct {
    GameEventType type;
    float x, y;
    float x2, y2;
    int32_t value;
    uint32_t flags;
} GameEvent;

#endif
