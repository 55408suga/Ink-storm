#ifndef INK_APP_INPUT_LATCH_H
#define INK_APP_INPUT_LATCH_H

#include "game.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float mouse_x, mouse_y;
    bool left_down;
    bool cyclone_pressed;
    bool gale_pressed;
    int32_t pick_pressed;
    bool reroll_pressed;
    bool repair_pressed;
    bool confirm_pressed;
    bool ui_click;
} RawInput;

typedef struct {
    float prev_x, prev_y;
    float cur_x, cur_y;
    bool left_down;
    bool paint_suppressed;
    bool cyclone_pending;
    bool gale_pending;
    int32_t pick_pending;
    bool repair_pending;
    bool reroll_pending;
    bool confirm_pending;
} InputLatch;

void input_latch_init(InputLatch *l, float x, float y);
void input_latch_feed(InputLatch *l, const RawInput *raw);
BrushInput input_latch_take_tick(InputLatch *l);
GameCommand input_latch_take_command(InputLatch *l, RunPhase phase);
bool input_latch_take_confirm(InputLatch *l);

#endif
