#include "vfx_rules.h"

#include <math.h>
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

void vfx_thunderburst_throttle_update(VfxThunderburstThrottle *st, float dt)
{
    if (st == NULL || !isfinite(dt) || dt <= 0.0f) {
        return;
    }
    st->fx_cd = fmaxf(0.0f, st->fx_cd - dt);
    st->shake_cd = fmaxf(0.0f, st->shake_cd - dt);
    st->streak_t = fmaxf(0.0f, st->streak_t - dt);
    st->callout_cd = fmaxf(0.0f, st->callout_cd - dt);
}

/* Bursts chained by a sweeping brush keep a small spark each, but the enso, callout and shake
 * stay spaced out, and shakes inside a streak stay soft until the sweep pauses: a full shake per
 * burst blurs the screen and the stacked callouts hide the field. */
VfxThunderburstFx vfx_thunderburst_throttle_consume(VfxThunderburstThrottle *st)
{
    if (st == NULL) {
        return (VfxThunderburstFx){
            .full_burst = true, .show_callout = true, .shake_amp = 4.2f, .shake_dur = 0.16f};
    }
    VfxThunderburstFx out = {0};
    if (st->fx_cd <= 0.0f) {
        out.full_burst = true;
        st->fx_cd = 0.42f;
    }
    if (st->callout_cd <= 0.0f) {
        out.show_callout = true;
        st->callout_cd = 1.40f;
    }
    if (st->shake_cd <= 0.0f) {
        const bool in_streak = st->streak_t > 0.0f;
        out.shake_amp = in_streak ? 2.0f : 4.2f;
        out.shake_dur = in_streak ? 0.10f : 0.16f;
        st->shake_cd = 0.65f;
    }
    st->streak_t = 1.40f;
    return out;
}
