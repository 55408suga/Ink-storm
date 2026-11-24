#ifndef INK_RENDER_VFX_RULES_H
#define INK_RENDER_VFX_RULES_H

#include "game.h"
#include "game_events.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    int32_t priority;
    float age;
} VfxSealRank;

bool vfx_event_bypasses_fx_budget(GameEventType type);
bool vfx_enemy_intro_shown_in(RunPhase phase);
int32_t vfx_rank_top_two(const VfxSealRank *seals, int32_t n, int32_t out[2]);

#endif
