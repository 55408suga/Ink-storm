#ifndef INK_RENDER_VFX_RULES_H
#define INK_RENDER_VFX_RULES_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    int32_t priority;
    float age;
} VfxSealRank;

typedef struct {
    float fx_cd;
    float shake_cd;
    float streak_t;
    float callout_cd;
} VfxThunderburstThrottle;

typedef struct {
    bool full_burst;
    bool show_callout;
    float shake_amp;
    float shake_dur;
} VfxThunderburstFx;

bool vfx_event_bypasses_fx_budget(GameEventType type);
bool vfx_enemy_intro_shown_in(RunPhase phase);
int32_t vfx_rank_top_two(const VfxSealRank *seals, int32_t n, int32_t out[2]);
void vfx_thunderburst_throttle_update(VfxThunderburstThrottle *st, float dt);
VfxThunderburstFx vfx_thunderburst_throttle_consume(VfxThunderburstThrottle *st);

#endif
