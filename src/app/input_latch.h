#ifndef INK_APP_INPUT_LATCH_H
#define INK_APP_INPUT_LATCH_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float mouse_x, mouse_y;
    bool left_down;
    bool cyclone_pressed;
    bool cyclone_ccw_pressed;
    bool gale_pressed;
    int32_t pick_pressed;
    bool reroll_pressed;
    bool repair_pressed;
    bool crystal_pressed;
    bool confirm_pressed;
    bool ui_click;
} RawInput;

typedef struct {
    float prev_x, prev_y;
    float cur_x, cur_y;
    float hitstop_dx, hitstop_dy;
    float cyclone_x, cyclone_y;
    float cyclone_ccw_x, cyclone_ccw_y;
    bool left_down;
    bool paint_suppressed;
    bool cyclone_pending;
    bool cyclone_ccw_pending;
    bool cyclone_ccw_first;
    bool gale_pending;
    int32_t pick_pending;
    bool repair_pending;
    bool crystal_pending;
    bool reroll_pending;
    bool confirm_pending;
} InputLatch;

void input_latch_init(InputLatch *l, float x, float y);
void input_latch_reset_for_ui_transition(InputLatch *l, float x, float y, bool left_down);
void input_latch_anchor(InputLatch *l, float x, float y);
void input_latch_feed(InputLatch *l, const RawInput *raw);
BrushInput input_latch_take_tick(InputLatch *l);
BrushInput input_latch_take_subtick(InputLatch *l, int32_t remaining_ticks);
BrushInput input_latch_take_subtick_for_game(InputLatch *l, int32_t remaining_ticks,
                                             const GameState *g);
void input_latch_clear_skills(InputLatch *l);
GameCommand input_latch_take_command(InputLatch *l, RunPhase phase);
bool input_latch_take_confirm(InputLatch *l);

#endif
