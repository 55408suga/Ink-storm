#include "input_latch.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const double k_min_slice_px = 2.5;

void input_latch_init(InputLatch *l, float x, float y)
{
    if (l == NULL) {
        return;
    }
    memset(l, 0, sizeof *l);
    l->prev_x = x;
    l->prev_y = y;
    l->cur_x = x;
    l->cur_y = y;
    l->pick_pending = -1;
}

void input_latch_reset_for_ui_transition(InputLatch *l, float x, float y, bool left_down)
{
    if (l == NULL) {
        return;
    }
    input_latch_init(l, x, y);
    l->left_down = left_down;
    l->paint_suppressed = left_down;
}

void input_latch_anchor(InputLatch *l, float x, float y)
{
    if (l == NULL) {
        return;
    }
    l->prev_x = x;
    l->prev_y = y;
    l->cur_x = x;
    l->cur_y = y;
    l->hitstop_dx = 0.0f;
    l->hitstop_dy = 0.0f;
}

void input_latch_feed(InputLatch *l, const RawInput *raw)
{
    if (l == NULL || raw == NULL) {
        return;
    }
    l->cur_x = raw->mouse_x;
    l->cur_y = raw->mouse_y;
    l->left_down = raw->left_down;

    if (raw->ui_click && raw->left_down) {
        l->paint_suppressed = true;
    } else if (!raw->left_down) {
        l->paint_suppressed = false;
    }

    if (raw->cyclone_pressed) {
        l->cyclone_pending = true;
    }
    if (raw->gale_pressed) {
        l->gale_pending = true;
    }
    if (raw->pick_pressed >= 0 && raw->pick_pressed < GAME_MAX_REWARD_CHOICES &&
        l->pick_pending < 0) {
        l->pick_pending = raw->pick_pressed;
    }
    if (raw->repair_pressed) {
        l->repair_pending = true;
    }
    if (raw->crystal_pressed) {
        l->crystal_pending = true;
    }
    if (raw->reroll_pressed) {
        l->reroll_pending = true;
    }
    if (raw->confirm_pressed) {
        l->confirm_pending = true;
    }
}

static void record_hitstop_motion(InputLatch *l, const BrushInput *in)
{
    const double step_dx = (double)in->mx - (double)in->pmx;
    const double step_dy = (double)in->my - (double)in->pmy;
    const double step_len = hypot(step_dx, step_dy);
    if (!(step_len > 0.0)) {
        return;
    }
    const double prev_dot = (double)l->hitstop_dx * step_dx + (double)l->hitstop_dy * step_dy;
    if (step_len > 2.0 && prev_dot < 0.0) {
        l->hitstop_dx = (float)step_dx;
        l->hitstop_dy = (float)step_dy;
    } else {
        l->hitstop_dx += (float)step_dx;
        l->hitstop_dy += (float)step_dy;
    }
}

static void apply_hitstop_motion(InputLatch *l, BrushInput *in)
{
    const double hs_dx = (double)l->hitstop_dx;
    const double hs_dy = (double)l->hitstop_dy;
    l->hitstop_dx = 0.0f;
    l->hitstop_dy = 0.0f;
    const double hs_len = hypot(hs_dx, hs_dy);
    if (!(hs_len > 0.0)) {
        return;
    }
    const double live_len =
        hypot((double)in->mx - (double)in->pmx, (double)in->my - (double)in->pmy);
    if (live_len > 2.0) {
        return;
    }
    if (in->paint && hs_len > 3.0) {
        const double scale = 3.0 / hs_len;
        in->pmx -= (float)(hs_dx * scale);
        in->pmy -= (float)(hs_dy * scale);
    } else {
        in->pmx -= (float)hs_dx;
        in->pmy -= (float)hs_dy;
    }
}

/* A catch-up frame that runs several ticks hands each one an equal slice of the frame's pointer
 * motion; giving it all to the first tick left the rest with zero stroke speed, which chopped the
 * brush sound on every slow frame. A tick that consumes a skill press still takes the whole
 * remaining motion: a slice would cast Cyclone and Gale behind the sampled cursor. A small move
 * also stays whole, because its slices would all fall under Gale's 2 px aim step and leave the
 * next Gale aimed the old way; the brush stays down over the still ticks after it, so its sound
 * carries on as it does when the pointer pauses mid-stroke. */
static BrushInput take_subtick_impl(InputLatch *l, int32_t remaining_ticks, bool keep_skills)
{
    BrushInput in = {0};
    if (l == NULL) {
        return in;
    }
    const bool has_skill_edge = !keep_skills && (l->cyclone_pending || l->gale_pending);
    if (remaining_ticks <= 1 || has_skill_edge) {
        in.mx = l->cur_x;
        in.my = l->cur_y;
        in.pmx = l->prev_x;
        in.pmy = l->prev_y;
        in.paint = l->left_down && !l->paint_suppressed;
        l->prev_x = l->cur_x;
        l->prev_y = l->cur_y;
        if (keep_skills) {
            record_hitstop_motion(l, &in);
        } else {
            apply_hitstop_motion(l, &in);
            in.cyclone = l->cyclone_pending;
            in.gale = l->gale_pending;
            l->cyclone_pending = false;
            l->gale_pending = false;
        }
        return in;
    }
    const double move =
        hypot((double)l->cur_x - (double)l->prev_x, (double)l->cur_y - (double)l->prev_y);
    if (!(move > k_min_slice_px * (double)remaining_ticks)) {
        return take_subtick_impl(l, 1, keep_skills);
    }
    const float share = 1.0f / (float)remaining_ticks;
    const float x = l->prev_x + (l->cur_x - l->prev_x) * share;
    const float y = l->prev_y + (l->cur_y - l->prev_y) * share;
    in = (BrushInput){
        .mx = x,
        .my = y,
        .pmx = l->prev_x,
        .pmy = l->prev_y,
        .paint = l->left_down && !l->paint_suppressed,
        .cyclone = false,
        .gale = false,
    };
    l->prev_x = x;
    l->prev_y = y;
    if (keep_skills) {
        record_hitstop_motion(l, &in);
    } else {
        apply_hitstop_motion(l, &in);
        l->cyclone_pending = false;
        l->gale_pending = false;
    }
    return in;
}

BrushInput input_latch_take_tick(InputLatch *l) { return take_subtick_impl(l, 1, false); }

BrushInput input_latch_take_subtick(InputLatch *l, int32_t remaining_ticks)
{
    return take_subtick_impl(l, remaining_ticks, false);
}

BrushInput input_latch_take_subtick_for_game(InputLatch *l, int32_t remaining_ticks,
                                             const GameState *g)
{
    const bool keep_skills = (g != NULL && g->phase == RUN_PHASE_PLAYING && g->hitstop_t > 0.0f);
    return take_subtick_impl(l, remaining_ticks, keep_skills);
}

void input_latch_clear_skills(InputLatch *l)
{
    if (l == NULL) {
        return;
    }
    l->cyclone_pending = false;
    l->gale_pending = false;
    l->hitstop_dx = 0.0f;
    l->hitstop_dy = 0.0f;
}

GameCommand input_latch_take_command(InputLatch *l, RunPhase phase)
{
    GameCommand cmd = {.type = CMD_NONE, .arg = 0};
    if (l == NULL) {
        return cmd;
    }
    if (phase == RUN_PHASE_REWARD) {
        if (l->pick_pending >= 0) {
            cmd.type = CMD_PICK_REWARD;
            cmd.arg = l->pick_pending;
        } else if (l->repair_pending) {
            cmd.type = CMD_REPAIR_SHRINE;
            cmd.arg = 0;
        } else if (l->crystal_pending) {
            cmd.type = CMD_PICK_ENDLESS_CRYSTAL;
            cmd.arg = 0;
        } else if (l->reroll_pending) {
            cmd.type = CMD_REROLL_REWARD;
            cmd.arg = 0;
        }
    }
    l->pick_pending = -1;
    l->repair_pending = false;
    l->crystal_pending = false;
    l->reroll_pending = false;
    return cmd;
}

bool input_latch_take_confirm(InputLatch *l)
{
    if (l == NULL) {
        return false;
    }
    const bool confirmed = l->confirm_pending;
    l->confirm_pending = false;
    return confirmed;
}
