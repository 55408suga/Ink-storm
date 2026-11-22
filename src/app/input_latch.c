#include "input_latch.h"

#include <stddef.h>
#include <string.h>

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
    if (raw->reroll_pressed) {
        l->reroll_pending = true;
    }
    if (raw->confirm_pressed) {
        l->confirm_pending = true;
    }
}

BrushInput input_latch_take_tick(InputLatch *l)
{
    BrushInput in = {0};
    if (l == NULL) {
        return in;
    }
    in.mx = l->cur_x;
    in.my = l->cur_y;
    in.pmx = l->prev_x;
    in.pmy = l->prev_y;
    in.paint = l->left_down && !l->paint_suppressed;
    in.cyclone = l->cyclone_pending;
    in.gale = l->gale_pending;

    l->prev_x = l->cur_x;
    l->prev_y = l->cur_y;
    l->cyclone_pending = false;
    l->gale_pending = false;
    return in;
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
        } else if (l->reroll_pending) {
            cmd.type = CMD_REROLL_REWARD;
            cmd.arg = 0;
        }
    }
    l->pick_pending = -1;
    l->repair_pending = false;
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
