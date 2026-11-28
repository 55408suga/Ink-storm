#include "vfx_rules.h"

#include "core.h"
#include "enemies/boss_break.h"

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

VfxBossCue vfx_boss_cue(const GameState *g)
{
    if (g == NULL || g->run.wave_index != GAME_BOSS_WAVE_INDEX || g->phase != RUN_PHASE_PLAYING) {
        return VFX_BOSS_CUE_NONE;
    }
    const EntityId bid = g->run.boss_id;
    if (bid.index == ECS_INVALID_INDEX || !ecs_is_alive(&g->ecs, bid)) {
        return VFX_BOSS_CUE_NONE;
    }
    const EnemyComp *en = game_enemy(g, bid);
    const BossComp *boss = game_boss(g, bid);
    const HealthComp *hp = game_health(g, bid);
    if (en == NULL || boss == NULL || hp == NULL || hp->hp <= 0) {
        return VFX_BOSS_CUE_NONE;
    }
    if (boss->break_t > 0.0f) {
        if (en->def_id == ENEMY_BOSS_RYU && boss->break_kind == (int32_t)BOSS_BREAK_FLINCH) {
            return VFX_BOSS_CUE_RYU_FLINCH;
        }
        return VFX_BOSS_CUE_BREAK;
    }
    if (en->def_id == ENEMY_BOSS_TENGU) {
        return boss->phase_idx >= 1 ? VFX_BOSS_CUE_TENGU_P2 : VFX_BOSS_CUE_TENGU_P1;
    }
    if (en->def_id == ENEMY_BOSS_NUE) {
        return en->ai_state == 1 ? VFX_BOSS_CUE_NUE_GROUNDED : VFX_BOSS_CUE_NUE_MOVING;
    }
    if (en->def_id == ENEMY_BOSS_RYU) {
        if (boss->phase_idx >= 2) {
            return VFX_BOSS_CUE_RYU_P3;
        }
        return boss->phase_idx == 1 ? VFX_BOSS_CUE_RYU_P2 : VFX_BOSS_CUE_RYU_P1;
    }
    return VFX_BOSS_CUE_NONE;
}

VfxShrineBar vfx_shrine_bar_style(int32_t hp, int32_t max_hp, float hit_t, float time_s,
                                  bool reduce_flash)
{
    if (hp <= 0 || max_hp <= 0) {
        return (VfxShrineBar){.visible = false, .alpha = 0.0f, .scale = 1.0f, .alert = false};
    }
    const float safe_hit = (isfinite(hit_t) && hit_t > 0.0f) ? hit_t : 0.0f;
    const float safe_t = isfinite(time_s) ? time_s : 0.0f;
    const bool alert = hp <= 3;

    if (safe_hit > 0.0f) {
        const float u = ink_clampf(safe_hit / 0.45f, 0.0f, 1.0f);
        return (VfxShrineBar){
            .visible = true,
            .alpha = 0.95f,
            .scale = reduce_flash ? 1.0f : (1.0f + 0.25f * u),
            .alert = alert,
        };
    }
    if (alert) {
        static const float k_two_pi = 6.2831853f;
        const float wave = 0.5f + 0.5f * sinf(k_two_pi * 2.2f * safe_t);
        const float amp = reduce_flash ? 0.08f : 0.20f;
        return (VfxShrineBar){
            .visible = true,
            .alpha = 0.80f + amp * wave,
            .scale = reduce_flash ? 1.0f : (1.0f + 0.04f * wave),
            .alert = true,
        };
    }
    return (VfxShrineBar){
        .visible = true,
        .alpha = hp < max_hp ? 0.80f : 0.50f,
        .scale = 1.0f,
        .alert = false,
    };
}

/* Compute top-Y for a bottom-centered explanation strip, lifting above the bottom-left Brush
 * Badge (or showcase page badge) and bottom-right Owned Relics whenever horizontal clearance
 * would be violated. */
float vfx_bottom_dock_y(float strip_w, float strip_h, int32_t relic_count, bool has_showcase_badge)
{
    const float safe_w = (isfinite(strip_w) && strip_w > 0.0f) ? strip_w : 0.0f;
    const float safe_h = (isfinite(strip_h) && strip_h > 0.0f) ? strip_h : 0.0f;
    const float cx = (float)INK_SCREEN_W * 0.5f;
    const float strip_left = cx - safe_w * 0.5f;
    const float strip_right = cx + safe_w * 0.5f;
    const int32_t shown_relics = relic_count < 0 ? 0 : (relic_count > 6 ? 6 : relic_count);
    const float left_limit = has_showcase_badge ? 218.0f : 214.0f;
    const bool hits_left = strip_left < left_limit;
    const float relic_limit = (float)INK_SCREEN_W - 18.0f - (float)shown_relics * 120.0f - 6.0f;
    const bool hits_right = shown_relics > 0 && strip_right > relic_limit;
    const float dock_bottom =
        (hits_left || hits_right) ? ((float)INK_SCREEN_H - 56.0f) : ((float)INK_SCREEN_H - 18.0f);
    return dock_bottom - safe_h;
}

/* First-time hint counts down from 4.0s to 0.0s: 0.25s fade-in, 3.35s solid, 0.40s fade-out. */
float vfx_hint_strip_alpha(float remaining_t)
{
    if (!isfinite(remaining_t) || remaining_t <= 0.0f) {
        return 0.0f;
    }
    const float fade_in_start = VFX_HINT_SHOW_SECONDS - 0.25f;
    if (remaining_t > fade_in_start) {
        return ink_clampf((VFX_HINT_SHOW_SECONDS - remaining_t) / 0.25f, 0.0f, 1.0f);
    }
    if (remaining_t < 0.40f) {
        return ink_clampf(remaining_t / 0.40f, 0.0f, 1.0f);
    }
    return 1.0f;
}

/* Enemy intro advances from 0.0s to 3.5s: 0.25s fade-in, 2.85s solid, 0.40s fade-out. */
float vfx_enemy_intro_alpha(float elapsed_t)
{
    if (!isfinite(elapsed_t) || elapsed_t <= 0.0f || elapsed_t >= VFX_ENEMY_INTRO_SECONDS) {
        return 0.0f;
    }
    if (elapsed_t < 0.25f) {
        return ink_clampf(elapsed_t / 0.25f, 0.0f, 1.0f);
    }
    const float fade_out_start = VFX_ENEMY_INTRO_SECONDS - 0.40f;
    if (elapsed_t > fade_out_start) {
        return ink_clampf((VFX_ENEMY_INTRO_SECONDS - elapsed_t) / 0.40f, 0.0f, 1.0f);
    }
    return 1.0f;
}

/* Boss entrance banner counts down from 2.2s to 0.0s: 0.20s fade-in, 1.60s solid, 0.40s fade-out.
 */
float vfx_boss_banner_alpha(float remaining_t)
{
    if (!isfinite(remaining_t) || remaining_t <= 0.0f) {
        return 0.0f;
    }
    const float fade_in_start = VFX_BOSS_BANNER_SECONDS - 0.20f;
    if (remaining_t > fade_in_start) {
        return ink_clampf((VFX_BOSS_BANNER_SECONDS - remaining_t) / 0.20f, 0.0f, 1.0f);
    }
    if (remaining_t < 0.40f) {
        return ink_clampf(remaining_t / 0.40f, 0.0f, 1.0f);
    }
    return 1.0f;
}
