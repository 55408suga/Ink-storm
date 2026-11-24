#include "vfx_rules.h"

#include <stddef.h>

/* Milestones bypass the major-effect budget: a busy screen must never swallow the moment that
 * tells the player the wave or act is won, or that the boss has arrived. */
bool vfx_event_bypasses_fx_budget(GameEventType type)
{
    return type == EV_WAVE_CLEAR || type == EV_ACT_CLEAR || type == EV_BOSS_ENTER;
}

/* The intro strip is hidden during the purification, so its timer must not run there either, or
 * an intro queued near the end of a wave would expire unseen. */
bool vfx_enemy_intro_shown_in(RunPhase phase) { return phase == RUN_PHASE_PLAYING; }

/* Only two seals fit on screen at once; higher priority wins, then the newest (smallest age),
 * instead of whichever happened to sit at the lowest array slot. */
int32_t vfx_rank_top_two(const VfxSealRank *seals, int32_t n, int32_t out[2])
{
    out[0] = -1;
    out[1] = -1;
    if (seals == NULL) {
        return 0;
    }
    for (int32_t i = 0; i < n; ++i) {
        const VfxSealRank *s = &seals[i];
        if (!s->active) {
            continue;
        }
        for (int32_t slot = 0; slot < 2; ++slot) {
            const VfxSealRank *cur = out[slot] >= 0 ? &seals[out[slot]] : NULL;
            const bool better = cur == NULL || s->priority > cur->priority ||
                                (s->priority == cur->priority && s->age < cur->age);
            if (better) {
                if (slot == 0) {
                    out[1] = out[0];
                }
                out[slot] = i;
                break;
            }
        }
    }
    return (out[0] >= 0 ? 1 : 0) + (out[1] >= 0 ? 1 : 0);
}
