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
}

void input_latch_feed(InputLatch *l, const RawInput *raw)
{
    if (l == NULL || raw == NULL) {
        return;
    }
    l->cur_x = raw->mouse_x;
    l->cur_y = raw->mouse_y;
    l->left_down = raw->left_down;
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
    in.paint = l->left_down;
    l->prev_x = l->cur_x;
    l->prev_y = l->cur_y;
    return in;
}

GameCommand input_latch_take_command(InputLatch *l, RunPhase phase)
{
    GameCommand cmd = {.type = CMD_NONE, .arg = 0};
    if (l == NULL) {
        return cmd;
    }
    if (phase == RUN_PHASE_REWARD && l->pending_cmd != CMD_NONE) {
        cmd.type = l->pending_cmd;
        cmd.arg = l->pending_arg;
    }
    l->pending_cmd = CMD_NONE;
    l->pending_arg = 0;
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
