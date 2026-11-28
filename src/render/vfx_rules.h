#ifndef INK_RENDER_VFX_RULES_H
#define INK_RENDER_VFX_RULES_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

#define VFX_ENEMY_INTRO_SECONDS 3.5f
#define VFX_BOSS_BANNER_SECONDS 2.2f
#define VFX_HINT_SHOW_SECONDS 4.0f

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

typedef enum {
    VFX_BOSS_CUE_NONE = 0,
    VFX_BOSS_CUE_TENGU_P1,
    VFX_BOSS_CUE_TENGU_P2,
    VFX_BOSS_CUE_NUE_MOVING,
    VFX_BOSS_CUE_NUE_GROUNDED,
    VFX_BOSS_CUE_RYU_P1,
    VFX_BOSS_CUE_RYU_P2,
    VFX_BOSS_CUE_RYU_P3,
    VFX_BOSS_CUE_RYU_FLINCH,
    VFX_BOSS_CUE_BREAK,
    VFX_BOSS_CUE_COUNT
} VfxBossCue;

typedef struct {
    bool visible;
    float alpha;
    float scale;
    bool alert;
} VfxShrineBar;

bool vfx_event_bypasses_fx_budget(GameEventType type);
bool vfx_enemy_intro_shown_in(RunPhase phase);
int32_t vfx_rank_top_two(const VfxSealRank *seals, int32_t n, int32_t out[2]);
void vfx_thunderburst_throttle_update(VfxThunderburstThrottle *st, float dt);
VfxThunderburstFx vfx_thunderburst_throttle_consume(VfxThunderburstThrottle *st);
VfxBossCue vfx_boss_cue(const GameState *g);
VfxShrineBar vfx_shrine_bar_style(int32_t hp, int32_t max_hp, float hit_t, float time_s,
                                  bool reduce_flash);
float vfx_bottom_dock_y(float strip_w, float strip_h, int32_t relic_count, bool has_showcase_badge);
float vfx_hint_strip_alpha(float remaining_t);
float vfx_enemy_intro_alpha(float elapsed_t);
float vfx_boss_banner_alpha(float remaining_t);

#endif
